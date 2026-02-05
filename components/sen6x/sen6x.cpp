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
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);

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

      if (!this->write_command(SEN5X_CMD_START_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      initialized_ = true;
      ESP_LOGD(TAG, "Sensor initialized");
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

  // 🛑 Wenn Messung gestoppt: Alle Sensorwerte auf UNBEKANNT (NAN)
  if (!this->is_measuring_) {

    auto set_nan = [&](sensor::Sensor *s) {
      if (s != nullptr) s->publish_state(NAN);
    };

    set_nan(this->pm_1_0_sensor_);
    set_nan(this->pm_2_5_sensor_);
    set_nan(this->pm_4_0_sensor_);
    set_nan(this->pm_10_0_sensor_);
    set_nan(this->pm_0_10_sensor_);
    set_nan(this->temperature_sensor_);
    set_nan(this->humidity_sensor_);
    set_nan(this->voc_sensor_);
    set_nan(this->nox_sensor_);
    set_nan(this->co2_sensor_);
    set_nan(this->nc_0_5_sensor_);
    set_nan(this->nc_1_0_sensor_);
    set_nan(this->nc_2_5_sensor_);
    set_nan(this->nc_4_0_sensor_);
    set_nan(this->nc_10_0_sensor_);

    this->status_set_warning();
    return;
  }

  // 🌡 Standard Measurement 0x0300
  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write error read measurement (%d)", this->last_error_);
    return;
  }

  this->set_timeout(20, [this]() {
    uint16_t measurements[9];
    if (!this->read_data(measurements, 9)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
      return;
    }

    float pm_1_0 = (measurements[0] == 0xFFFF) ? NAN : measurements[0] / 10.0f;
    float pm_2_5 = (measurements[1] == 0xFFFF || measurements[0] == 0xFFFF) ? NAN : (measurements[1] - measurements[0]) / 10.0f;
    float pm_4_0 = (measurements[2] == 0xFFFF || measurements[1] == 0xFFFF) ? NAN : (measurements[2] - measurements[1]) / 10.0f;
    float pm_10_0 = (measurements[3] == 0xFFFF || measurements[2] == 0xFFFF) ? NAN : (measurements[3] - measurements[2]) / 10.0f;
    float pm_0_10 = (measurements[3] == 0xFFFF) ? NAN : measurements[3] / 10.0f;
    float humidity = (measurements[4] == 0xFFFF) ? NAN : measurements[4] / 100.0f;
    float temperature = (measurements[5] == 0xFFFF) ? NAN : (int16_t) measurements[5] / 200.0f;
    float voc = (measurements[6] == 0x7FFF) ? NAN : measurements[6] / 10.0f;
    float nox = (measurements[7] == 0x7FFF) ? NAN : measurements[7] / 10.0f;
    float co2 = (measurements[8] == 0xFFFF) ? NAN : measurements[8];

    if (this->pm_1_0_sensor_) this->pm_1_0_sensor_->publish_state(pm_1_0);
    if (this->pm_2_5_sensor_) this->pm_2_5_sensor_->publish_state(pm_2_5);
    if (this->pm_4_0_sensor_) this->pm_4_0_sensor_->publish_state(pm_4_0);
    if (this->pm_10_0_sensor_) this->pm_10_0_sensor_->publish_state(pm_10_0);
    if (this->pm_0_10_sensor_) this->pm_0_10_sensor_->publish_state(pm_0_10);
    if (this->temperature_sensor_) this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_) this->humidity_sensor_->publish_state(humidity);
    if (this->voc_sensor_) this->voc_sensor_->publish_state(voc);
    if (this->nox_sensor_) this->nox_sensor_->publish_state(nox);
    if (this->co2_sensor_) this->co2_sensor_->publish_state(co2);

    this->status_clear_warning();

    // 🧮 Number Concentration with Delay
    this->set_timeout(50, [this]() {
      uint16_t nc05_u, nc10_u, nc25_u, nc40_u, nc100_u;
      if (!this->read_number_concentration(&nc05_u, &nc10_u, &nc25_u, &nc40_u, &nc100_u)) return;

      auto conv = [](uint16_t v) -> float {
        if (v == 0xFFFF) return NAN;
        return (float) v;
      };

      if (this->nc_0_5_sensor_) this->nc_0_5_sensor_->publish_state(conv(nc05_u));
      if (this->nc_1_0_sensor_) this->nc_1_0_sensor_->publish_state(conv(nc10_u));
      if (this->nc_2_5_sensor_) this->nc_2_5_sensor_->publish_state(conv(nc25_u));
      if (this->nc_4_0_sensor_) this->nc_4_0_sensor_->publish_state(conv(nc40_u));
      if (this->nc_10_0_sensor_) this->nc_10_0_sensor_->publish_state(conv(nc100_u));
    });

  });
}

// -----------------------------------------------------------------------------
//                          FUNKTIONEN
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

  // Datenblatt: nach 0x0316 typischerweise ~20 ms warten, damit die Werte bereit sind.
  // (Hier bewusst 200 ms als robustes Delay, um '0'-Werte durch zu frühes Lesen zu vermeiden.)
  delay(200);

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
  ESP_LOGD(TAG, "Measurement started");
  this->is_measuring_ = true;
  return true;
}

bool SEN5XComponent::stop_measurement() {
  if (!write_command(SEN5X_CMD_STOP_MEASUREMENTS)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error stop measurement (%d)", this->last_error_);
    return false;
  }
  ESP_LOGD(TAG, "Measurement stopped");
  this->is_measuring_ = false;
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
