
#pragma once
#include "esphome.h"

namespace esphome {
namespace sen6x {

class SEN6x : public PollingComponent, public i2c::I2CDevice {
 public:
  // sensors
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *co2_{nullptr};
  sensor::Sensor *voc_{nullptr};
  sensor::Sensor *nox_{nullptr};
  sensor::Sensor *hcho_{nullptr};
  sensor::Sensor *pm1_{nullptr};
  sensor::Sensor *pm25_{nullptr};
  sensor::Sensor *pm4_{nullptr};
  sensor::Sensor *pm10_{nullptr};

  void set_temperature_sensor(sensor::Sensor *s) { temperature_ = s; }
  void set_humidity_sensor(sensor::Sensor *s) { humidity_ = s; }
  void set_co2_sensor(sensor::Sensor *s) { co2_ = s; }
  void set_voc_sensor(sensor::Sensor *s) { voc_ = s; }
  void set_nox_sensor(sensor::Sensor *s) { nox_ = s; }
  void set_hcho_sensor(sensor::Sensor *s) { hcho_ = s; }
  void set_pm1_sensor(sensor::Sensor *s) { pm1_ = s; }
  void set_pm_2_5_sensor(sensor::Sensor *s) { pm25_ = s; }
  void set_pm_4_0_sensor(sensor::Sensor *s) { pm4_ = s; }
  void set_pm_10_0_sensor(sensor::Sensor *s) { pm10_ = s; }

  void set_model(const std::string &m) { model_ = m; }

  void setup() override;
  void update() override;

 protected:
  std::string model_{"auto"};

  // helpers
  bool start_measurement_();
  bool read_command_(uint16_t cmd, uint8_t *dst, size_t len, uint16_t exec_ms);
  static uint8_t crc8_(const uint8_t *data, size_t len);
  static bool check_triplet_(const uint8_t *p) { return crc8_(p, 2) == p[2]; }
  static uint16_t be16_(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }
};

}  // namespace sen6x
}  // namespace esphome
