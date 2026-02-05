#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

// Commands
static const uint16_t SEN5X_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN5X_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN5X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN5X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN5X_CMD_NOX_ALGORITHM_TUNING = 0x60E1;
static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x0300; // SEN66 only
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN5X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;

static const uint16_t SEN6X_CMD_RESET = 0xD304;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen6x...");

  this->set_timeout(2000, [this]() {

    if (!this->write_command(SEN5X_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }

    uint16_t raw_read_status;
    if (!this->read_data(raw_read_status)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }

    uint32_t stop_measurement_delay = 0;

    if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor has data available, stopping periodic measurement / reset");
      if (!this->write_command(SEN6X_CMD_RESET)) {
        ESP_LOGE(TAG, "Failed to stop measurements / reset");
        this->mark_failed();
        return;
      }

      // Datasheet: sensor needs 200 ms after reset before accepting new commands
      delay(200);

      stop_measurement_delay = 1200;
    }

    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SEN5X_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_[0] = static_cast<bool>(uint16_t(raw_serial_number[0]) & 0xFF);
      this->serial_number_[1] = static_cast<uint16_t>(raw_serial_number[0] & 0xFF);
      this->serial_number_[2] = static_cast<uint16_t>(raw_serial_number[1] >> 8);

      uint16_t raw_product_name[16];
      if (!this->get_register(SEN5X_CMD_GET_PRODUCT_NAME, raw_product_name, 16, 20)) {
        ESP_LOGE(TAG, "Failed to read product name");
        this->error_code_ = PRODUCT_NAME_FAILED;
        this->mark_failed();
        return;
      }

      const uint16_t *current_int = raw_product_name;
      char current_char;
      uint8_t max = 16;
      do {
        current_char = *current_int >> 8;
        if (current_char) {
          product_name_.push_back(current_char);
          current_char = *current_int & 0xFF;
          if (current_char)
            product_name_.push_back(current_char);
        }
        current_int++;
      } while (current_char && --max);

      ESP_LOGD(TAG, "Productname %s", product_name_.c_str());

      // ⭐ Start measurement now
      if (!this->write_command(SEN5X_CMD_START_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }

      this->is_measuring_ = true;  // <<< FIXED
      initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized and measuring");
    });
  });
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "sen6x:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

// -----------------------------------------------------------------------------
//                          MESSWERTE AUSLESEN
// -----------------------------------------------------------------------------

void SEN5XComponent::update() {
  if (!initialized_) return;

  if (!this->is_measuring_) {
    auto set_nan = [&](Sensor *s) {
      if (s) s->publish_state(NAN);
    };

    set_nan(this->nc_0_5_sensor_);
    set_nan(this->nc_1_0_sensor_);
    set_nan(this->nc_2_5_sensor_);
    set_nan(this->nc_4_0_sensor_);
    set_nan(this->nc_10_0_sensor_);
    return;
  }

  // -------- NORMAL MEASUREMENT --------
  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT)) return;

  this->set_timeout(50, [this]() {
    uint16_t measurements[9];
    if (!this->read_data(measurements, 9)) return;

    // (PM / Temp / Hum / VOC / NOx / CO2 – unverändert)

    // -------- NUMBER CONCENTRATION --------
    // ⚠️ WICHTIG: mindestens 200 ms warten
    this->set_timeout(200, [this]() {
      if (!this->is_measuring_) return;

      uint16_t raw[5];
      if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION)) return;
      if (!this->read_data(raw, 5)) return;

      auto conv = [](uint16_t v) -> float {
        return (v == 0xFFFF) ? NAN : (float) v;
      };

      if (this->nc_0_5_sensor_) this->nc_0_5_sensor_->publish_state(conv(raw[0]));
      if (this->nc_1_0_sensor_) this->nc_1_0_sensor_->publish_state(conv(raw[1]));
      if (this->nc_2_5_sensor_) this->nc_2_5_sensor_->publish_state(conv(raw[2]));
      if (this->nc_4_0_sensor_) this->nc_4_0_sensor_->publish_state(conv(raw[3]));
      if (this->nc_10_0_sensor_) this->nc_10_0_sensor_->publish_state(conv(raw[4]));
    });
  });
}

// -----------------------------------------------------------------------------
//                          FUNCTIONS
// -----------------------------------------------------------------------------

bool SEN5XComponent::read_number_concentration(uint16_t *nc05, uint16_t *nc10,
                                               uint16_t *nc25, uint16_t *nc40,
                                               uint16_t *nc100) {
  uint16_t raw[5];

  if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Error writing Number Concentration command (0x0316), err=%d", this->last_error_);
    return false;
  }

  if (!this->read_data(raw, 5)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Error reading Number Concentration values (0x0316), err=%d", this->last_error_);
    return false;
  }

  *nc05  = raw[0];
  *nc10  = raw[1];
  *nc25  = raw[2];
  *nc40  = raw[3];
  *nc100 = raw[4];

  return true;
}

bool SEN5XComponent::start_measurement() {
  if (!write_command(SEN5X_CMD_START_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start measurement (%d)", this->last_error_);
    return false;
  }
  this->is_measuring_ = true;
  ESP_LOGD(TAG, "Measurement started");
  return true;
}

bool SEN5XComponent::stop_measurement() {
  if (!write_command(SEN5X_CMD_STOP_MEASUREMENTS)) return false;
  this->is_measuring_ = false;

  auto set_nan = [&](Sensor *s) {
    if (s) s->publish_state(NAN);
  };

  set_nan(this->nc_0_5_sensor_);
  set_nan(this->nc_1_0_sensor_);
  set_nan(this->nc_2_5_sensor_);
  set_nan(this->nc_4_0_sensor_);
  set_nan(this->nc_10_0_sensor_);

  return true;
}

bool SEN5XComponent::start_fan_cleaning() {
  if (!write_command(SEN5X_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  }
  ESP_LOGD(TAG, "Fan auto clean started");
  return true;
}

}  // namespace sen6x
}  // namespace esphome
