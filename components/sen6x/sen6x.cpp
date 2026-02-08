#include "sen6x.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen6x {

static const char *const TAG = "sen6x";

// Commands
static const uint16_t SEN5X_CMD_READ_MEASUREMENT = 0x0300;
static const uint16_t SEN5X_CMD_START_MEASUREMENTS = 0x0021;
static const uint16_t SEN5X_CMD_STOP_MEASUREMENTS = 0x0104;
static const uint16_t SEN5X_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN6X_CMD_READ_NUMBER_CONCENTRATION = 0x0316;

void SEN5XComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN66...");

  this->set_timeout(2000, [this]() {
    if (!this->write_command(SEN5X_CMD_START_MEASUREMENTS)) {
      ESP_LOGE(TAG, "Failed to start measurements");
      this->mark_failed();
      return;
    }

    this->is_measuring_ = true;
    this->initialized_ = true;
    ESP_LOGI(TAG, "SEN66 measurement started");
  });
}

void SEN5XComponent::update() {
  if (!this->initialized_ || !this->is_measuring_)
    return;

  if (!this->write_command(SEN5X_CMD_READ_MEASUREMENT)) {
    ESP_LOGW(TAG, "READ_MEASUREMENT failed");
    return;
  }

  this->set_timeout(30, [this]() {
    uint16_t m[9];
    if (!this->read_data(m, 9)) {
      ESP_LOGW(TAG, "Measurement read failed");
      return;
    }

    auto conv_pm = [](uint16_t v) -> float {
      return (v == 0xFFFF) ? NAN : v / 10.0f;
    };

    float pm1  = conv_pm(m[0]);
    float pm25 = conv_pm(m[1]);
    float pm4  = conv_pm(m[2]);
    float pm10 = conv_pm(m[3]);

    if (this->pm_1_0_sensor_)  this->pm_1_0_sensor_->publish_state(pm1);
    if (this->pm_2_5_sensor_)  this->pm_2_5_sensor_->publish_state(pm25);
    if (this->pm_4_0_sensor_)  this->pm_4_0_sensor_->publish_state(pm4);
    if (this->pm_10_0_sensor_) this->pm_10_0_sensor_->publish_state(pm10);
    if (this->pm_0_10_sensor_) this->pm_0_10_sensor_->publish_state(pm10);

    if (this->humidity_sensor_)
      this->humidity_sensor_->publish_state(m[4] == 0xFFFF ? NAN : m[4] / 100.0f);

    if (this->temperature_sensor_)
      this->temperature_sensor_->publish_state(m[5] == 0xFFFF ? NAN : (int16_t)m[5] / 200.0f);

    if (this->voc_sensor_)
      this->voc_sensor_->publish_state(m[6] == 0x7FFF ? NAN : m[6] / 10.0f);

    if (this->nox_sensor_)
      this->nox_sensor_->publish_state(m[7] == 0x7FFF ? NAN : m[7] / 10.0f);

    if (this->co2_sensor_)
      this->co2_sensor_->publish_state(m[8] == 0xFFFF ? NAN : m[8]);

    // Number Concentration – with proper delay
    this->set_timeout(100, [this]() {
      uint16_t nc[5];
      if (!this->write_command(SEN6X_CMD_READ_NUMBER_CONCENTRATION))
        return;
      if (!this->read_data(nc, 5))
        return;

      auto conv = [](uint16_t v) -> float {
        return (v == 0xFFFF) ? NAN : (float)v;
      };

      if (this->nc_0_5_sensor_)  this->nc_0_5_sensor_->publish_state(conv(nc[0]));
      if (this->nc_1_0_sensor_)  this->nc_1_0_sensor_->publish_state(conv(nc[1]));
      if (this->nc_2_5_sensor_)  this->nc_2_5_sensor_->publish_state(conv(nc[2]));
      if (this->nc_4_0_sensor_)  this->nc_4_0_sensor_->publish_state(conv(nc[3]));
      if (this->nc_10_0_sensor_) this->nc_10_0_sensor_->publish_state(conv(nc[4]));
    });
  });
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
