
#include "sen6x.h"

namespace esphome {
namespace sen6x {

// Sensirion CRC-8 (poly 0x31, init 0xFF)
uint8_t SEN6x::crc8_(const uint8_t *data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

bool SEN6x::read_command_(uint16_t cmd, uint8_t *dst, size_t len, uint16_t exec_ms) {
  uint8_t w[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
  auto err = this->write(w, 2);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW("sen6x", "write cmd 0x%04X failed err=%d", cmd, (int)err);
    return false;
  }
  delay(exec_ms);
  if (len == 0) return true;
  err = this->read(dst, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW("sen6x", "read cmd 0x%04X failed err=%d", cmd, (int)err);
    return false;
  }
  return true;
}

void SEN6x::setup() {
  // start continuous measurement
  // SEN6x models: 0x0021 ; SEN60: 0x2152
  bool ok = false;
  if (model_ == "sen60") {
    ok = read_command_(0x2152, nullptr, 0, 1);
  } else {
    ok = read_command_(0x0021, nullptr, 0, 50);
  }
  if (!ok) ESP_LOGW("sen6x", "failed to start measurement");
}

bool SEN6x::start_measurement_() {
  // already issued in setup(); keep method for potential future retries.
  return true;
}

void SEN6x::update() {
  // choose model (auto currently behaves like SEN66/SEN6x generic)
  std::string m = model_;
  for (auto &c : m) c = (char)tolower((unsigned char)c);

  // buffers large enough for all variants
  uint8_t buf[64] = {0};

  auto publish_pm = [&](int off_triplet, sensor::Sensor* s, float scale)->void{
    if (!s) return;
    if (off_triplet < 0) return;
    const uint8_t *p = &buf[off_triplet];
    if (!check_triplet_(p)) return;
    uint16_t raw = be16_(p);
    s->publish_state((float)raw / scale);
  };

  auto publish_int16 = [&](int off_triplet, sensor::Sensor* s, float scale)->void{
    if (!s) return;
    if (off_triplet < 0) return;
    const uint8_t *p = &buf[off_triplet];
    if (!check_triplet_(p)) return;
    int16_t raw = (int16_t)be16_(p);
    s->publish_state((float)raw / scale);
  };

  auto publish_u16 = [&](int off_triplet, sensor::Sensor* s, float scale)->void{
    if (!s) return;
    if (off_triplet < 0) return;
    const uint8_t *p = &buf[off_triplet];
    if (!check_triplet_(p)) return;
    uint16_t raw = be16_(p);
    s->publish_state((float)raw / scale);
  };

  // command IDs per datasheet
  uint16_t cmd = 0;
  uint16_t exec_ms = 20;
  size_t rx_len = 0;

  if (m == "sen60") {
    // Read measured values SEN60 (PM + number concentration)
    cmd = 0xEC05; rx_len = 27; exec_ms = 1;
    if (!read_command_(cmd, buf, rx_len, exec_ms)) return;

    // Table 33: PMx mass concentrations at triplets 0,3,6,9 (value/10)
    publish_pm(0,  pm1_, 10.0f);
    publish_pm(3,  pm25_, 10.0f);
    publish_pm(6,  pm4_,  10.0f);
    publish_pm(9,  pm10_, 10.0f);
    // number concentrations exist too, but we don't expose them as sensors here.
  }
  else if (m == "sen63c") {
    // Read measured values SEN63C (PM, RH, T, CO2)
    cmd = 0x0471; rx_len = 21; exec_ms = 20;
    if (!read_command_(cmd, buf, rx_len, exec_ms)) return;

    publish_pm(0,  pm1_, 10.0f);
    publish_pm(3,  pm25_, 10.0f);
    publish_pm(6,  pm4_,  10.0f);
    publish_pm(9,  pm10_, 10.0f);
    publish_int16(12, humidity_, 100.0f);
    publish_int16(15, temperature_, 200.0f);
    publish_int16(18, co2_, 1.0f);
  }
  else if (m == "sen65") {
    // Read measured values SEN65 (PM, RH, T, VOC idx, NOx idx)
    cmd = 0x0446; rx_len = 24; exec_ms = 20;
    if (!read_command_(cmd, buf, rx_len, exec_ms)) return;

    publish_pm(0,  pm1_, 10.0f);
    publish_pm(3,  pm25_, 10.0f);
    publish_pm(6,  pm4_,  10.0f);
    publish_pm(9,  pm10_, 10.0f);
    publish_int16(12, humidity_, 100.0f);
    publish_int16(15, temperature_, 200.0f);
    publish_int16(18, voc_, 10.0f);
    publish_int16(21, nox_, 10.0f);
  }
  else if (m == "sen68") {
    // Read measured values SEN68 (PM, RH, T, VOC idx, NOx idx, HCHO)
    cmd = 0x0467; rx_len = 27; exec_ms = 20;
    if (!read_command_(cmd, buf, rx_len, exec_ms)) return;

    publish_pm(0,  pm1_, 10.0f);
    publish_pm(3,  pm25_, 10.0f);
    publish_pm(6,  pm4_,  10.0f);
    publish_pm(9,  pm10_, 10.0f);
    publish_int16(12, humidity_, 100.0f);
    publish_int16(15, temperature_, 200.0f);
    publish_int16(18, voc_, 10.0f);
    publish_int16(21, nox_, 10.0f);
    publish_int16(24, hcho_, 1.0f); // ppb
  }
  else {
    // default SEN66 (or auto for generic SEN6x): PM, RH, T, VOC idx, NOx idx, CO2
    cmd = 0x0300; rx_len = 27; exec_ms = 20;
    if (!read_command_(cmd, buf, rx_len, exec_ms)) return;

    publish_pm(0,  pm1_, 10.0f);
    publish_pm(3,  pm25_, 10.0f);
    publish_pm(6,  pm4_,  10.0f);
    publish_pm(9,  pm10_, 10.0f);
    publish_int16(12, humidity_, 100.0f);
    publish_int16(15, temperature_, 200.0f);
    publish_int16(18, voc_, 10.0f);
    publish_int16(21, nox_, 10.0f);
    publish_int16(24, co2_, 1.0f);
  }
}

}  // namespace sen6x
}  // namespace esphome
