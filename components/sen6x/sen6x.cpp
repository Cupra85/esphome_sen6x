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
static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x0300;
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS_RHT_ONLY = 0x0037;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN5X_CMD_TEMPERATURE_COMPENSATION = 0x60B2;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_STATE = 0x6181;
static const uint16_t SEN5X_CMD_VOC_ALGORITHM_TUNING = 0x60D0;

static const uint16_t SEN6X_CMD_RESET = 0xD304;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;

// ============================================================================
// SETUP
// ============================================================================

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

    if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor had running measurement → reset");
      this->write_command(SEN6X_CMD_RESET);
      delay(200);
    }

    if (!this->write_command(SEN5X_CMD_START_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Failed to start measurements");
      this->mark_failed();
      return;
    }

    this->is_measuring_ = true;
    this->initialized_ = true;

    ESP_LOGI(TAG, "SEN66 initialized, measurement started");
  });
}

// ============================================================================
// DUMP CONFIG  ✅ FIX
// ============================================================================

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6x:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Sensor setup failed!");
  } else {
    ESP_LOGCONFIG(TAG, "  Measuring: %s", this->is_measuring_ ? "YES" : "NO");
  }
}

// ============================================================================
// UPDATE
// ============================================================================

void SEN5XComponent::update() {
  if (!this->initialized_)
    return;

  if (!this->is_measuring_) {
    auto nan = [](sensor::Sensor *s) {
      if (s != nullptr)
        s->publish_state(NAN);
    };

    nan(pm_1_0_sensor_);
    nan(pm_2_5_sensor_);
    nan(pm_4_0_sensor_);
    nan(pm_10_0_sensor_);
    nan(pm_0_10_sensor_);
    nan(temperature_sensor_);
    nan(humidity_sensor_);
    nan(voc_sensor_);
    nan(nox_sensor_);
    nan(co2_sensor_);
    nan(nc_0_5_sensor_);
    nan(nc_1_0_sensor_);
    nan(nc_2_5_sensor_);
    nan(nc_4_0_sensor_);
    nan(nc_10_0_sensor_);
    return;
  }

  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT))
    return;

  this->set_timeout(20, [this]() {
    uint16_t m[9];
    if (!this->read_data(m, 9))
      return;

    auto v = [](uint16_t x, float div) {
      return x == 0xFFFF ? NAN : x / div;
    };

    if (pm_1_0_sensor_) pm_1_0_sensor_->publish_state(v(m[0], 10));
    if (pm_2_5_sensor_) pm_2_5_sensor_->publish_state(v(m[1] - m[0], 10));
    if (pm_4_0_sensor_) pm_4_0_sensor_->publish_state(v(m[2] - m[1], 10));
    if (pm_10_0_sensor_) pm_10_0_sensor_->publish_state(v(m[3] - m[2], 10));
    if (pm_0_10_sensor_) pm_0_10_sensor_->publish_state(v(m[3], 10));
    if (humidity_sensor_) humidity_sensor_->publish_state(v(m[4], 100));
    if (temperature_sensor_) temperature_sensor_->publish_state((int16_t)m[5] / 200.0f);
    if (voc_sensor_) voc_sensor_->publish_state(m[6] == 0x7FFF ? NAN : m[6] / 10.0f);
    if (nox_sensor_) nox_sensor_->publish_state(m[7] == 0x7FFF ? NAN : m[7] / 10.0f);
    if (co2_sensor_) co2_sensor_->publish_state(m[8] == 0xFFFF ? NAN : m[8]);

    this->set_timeout(50, [this]() {
      uint16_t a, b, c, d, e;
      if (!read_number_concentration(&a, &b, &c, &d, &e))
        return;

      if (nc_0_5_sensor_) nc_0_5_sensor_->publish_state(a);
      if (nc_1_0_sensor_) nc_1_0_sensor_->publish_state(b);
      if (nc_2_5_sensor_) nc_2_5_sensor_->publish_state(c);
      if (nc_4_0_sensor_) nc_4_0_sensor_->publish_state(d);
      if (nc_10_0_sensor_) nc_10_0_sensor_->publish_state(e);
    });
  });
}

// ============================================================================
// HELPERS
// ============================================================================

bool SEN5XComponent::read_number_concentration(uint16_t *a, uint16_t *b,
                                               uint16_t *c, uint16_t *d,
                                               uint16_t *e) {
  uint16_t raw[5];
  if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION))
    return false;
  if (!this->read_data(raw, 5))
    return false;

  *a = raw[0];
  *b = raw[1];
  *c = raw[2];
  *d = raw[3];
  *e = raw[4];
  return true;
}

bool SEN5XComponent::start_measurement() {
  if (!this->write_command(SEN5X_CMD_START_MEASUREMENTS))
    return false;
  this->is_measuring_ = true;
  return true;
}

bool SEN5XComponent::stop_measurement() {
  if (!this->write_command(SEN5X_CMD_STOP_MEASUREMENTS))
    return false;
  this->is_measuring_ = false;
  return true;
}

bool SEN5XComponent::start_fan_cleaning() {
  return this->write_command(SEN5X_CMD_START_CLEANING_FAN);
}

}  // namespace sen6x
}  // namespace esphome
