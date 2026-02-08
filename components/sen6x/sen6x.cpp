#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

// Commands
static const uint16_t CMD_DATA_READY              = 0x0202;
static const uint16_t CMD_READ_MEASUREMENT        = 0x0300;
static const uint16_t CMD_READ_NUMBER_CONCENTRATION = 0x0316;
static const uint16_t CMD_START_MEASUREMENTS      = 0x0021;
static const uint16_t CMD_STOP_MEASUREMENTS       = 0x0104;
static const uint16_t CMD_RESET                   = 0xD304;
static const uint16_t CMD_START_FAN_CLEANING      = 0x5607;

// ============================================================================
// SETUP
// ============================================================================

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN6X...");

  this->set_timeout(200, [this]() {
    this->write_command(CMD_RESET);
    delay(200);

    if (!this->write_command(CMD_START_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Failed to start measurement");
      this->mark_failed();
      return;
    }

    this->is_measuring_ = true;
    this->initialized_ = true;
    ESP_LOGI(TAG, "SEN66 measurement started");
  });
}

void SEN5XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6X:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

// ============================================================================
// UPDATE LOOP
// ============================================================================

void SEN5XComponent::update() {
  if (!this->initialized_ || !this->is_measuring_)
    return;

  // 1️⃣ DATA READY CHECK
  if (!this->write_command(CMD_DATA_READY)) {
    ESP_LOGW(TAG, "Data-ready write failed");
    return;
  }

  uint16_t ready = 0;
  if (!this->read_data(ready) || ready == 0) {
    ESP_LOGV(TAG, "Sensor not ready");
    return;
  }

  // 2️⃣ READ MAIN MEASUREMENTS
  if (!this->write_command(CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "Read measurement command failed");
    return;
  }

  this->set_timeout(20, [this]() {
    uint16_t m[9];
    if (!this->read_data(m, 9)) {
      ESP_LOGW(TAG, "Measurement read failed");
      return;
    }

    auto f = [](uint16_t v, float div = 1.0f) {
      return (v == 0xFFFF || v == 0x7FFF) ? NAN : v / div;
    };

    if (this->pm_1_0_sensor_)  this->pm_1_0_sensor_->publish_state(f(m[0], 10));
    if (this->pm_2_5_sensor_)  this->pm_2_5_sensor_->publish_state(f(m[1] - m[0], 10));
    if (this->pm_4_0_sensor_)  this->pm_4_0_sensor_->publish_state(f(m[2] - m[1], 10));
    if (this->pm_10_0_sensor_) this->pm_10_0_sensor_->publish_state(f(m[3] - m[2], 10));

    if (this->humidity_sensor_)    this->humidity_sensor_->publish_state(f(m[4], 100));
    if (this->temperature_sensor_) this->temperature_sensor_->publish_state((int16_t)m[5] / 200.0f);
    if (this->voc_sensor_)         this->voc_sensor_->publish_state(f(m[6], 10));
    if (this->nox_sensor_)         this->nox_sensor_->publish_state(f(m[7], 10));
    if (this->co2_sensor_)         this->co2_sensor_->publish_state(m[8]);

    // 3️⃣ NUMBER CONCENTRATION
    this->set_timeout(50, [this]() {
      uint16_t nc[5];
      if (!this->write_command(CMD_READ_NUMBER_CONCENTRATION))
        return;
      if (!this->read_data(nc, 5))
        return;

      auto n = [](uint16_t v) { return v == 0xFFFF ? NAN : (float) v; };

      if (this->nc_0_5_sensor_)  this->nc_0_5_sensor_->publish_state(n(nc[0]));
      if (this->nc_1_0_sensor_)  this->nc_1_0_sensor_->publish_state(n(nc[1]));
      if (this->nc_2_5_sensor_)  this->nc_2_5_sensor_->publish_state(n(nc[2]));
      if (this->nc_4_0_sensor_)  this->nc_4_0_sensor_->publish_state(n(nc[3]));
      if (this->nc_10_0_sensor_) this->nc_10_0_sensor_->publish_state(n(nc[4]));
    });
  });
}

// ============================================================================
// CONTROL
// ============================================================================

bool SEN5XComponent::start_measurement() {
  if (!this->write_command(CMD_START_MEASUREMENTS))
    return false;
  this->is_measuring_ = true;
  return true;
}

bool SEN5XComponent::stop_measurement() {
  if (!this->write_command(CMD_STOP_MEASUREMENTS))
    return false;
  this->is_measuring_ = false;
  return true;
}

bool SEN5XComponent::start_fan_cleaning() {
  return this->write_command(CMD_START_FAN_CLEANING);
}

}  // namespace sen6x
}  // namespace esphome
