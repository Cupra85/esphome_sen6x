# ESPHome Component for Sensirion SEN66

Local external component for ESPHome (>= 2025.10.x) to read **Sensirion SEN66** air‑quality sensors via I²C.
Supports **only SEN66** and publishes all signals
available per model (PMx, T, RH, VOC, NOx, CO₂, – see table). Works with **Arduino**.

> This is a *external component*.

## Model capability matrix
| Model  | PM | RH/T | VOC | NOx | CO₂ | HCHO |
|:------:|:--:|:----:|:---:|:---:|:---:|:----:|
| SEN60  | ✅ |      |     |     |     |      |
| SEN63C | ✅ | ✅   |     |     | ✅  |      |
| SEN65  | ✅ | ✅   | ✅  | ✅  |     |      |
| SEN66  | ✅ | ✅   | ✅  | ✅  | ✅  |      |
| SEN68  | ✅ | ✅   | ✅  | ✅  |     | ✅   |

## Wiring (ESP32‑C6 example)
| Pin  | Wire | ESP32‑C6 | Note |
|-----:|------|----------|------|
| GND  | Black| GND      | Ground |
| VDD  | Red  | 3V3      | 3.3 V |
| SDA  | Blue | GPIO10    | I²C data |
| SCL  | Yellow | GPIO11  | I²C clock |

## Quick start (ESP‑IDF)
```yaml
esphome:
  name: sen6x_idf

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: arduino

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
api:
ota:

i2c:
  sda: GPIO10
  scl: GPIO11
  id: bus_a
  scan: true

external_components:
  - source: github://Cupra85/esphome_sen6x

sensor:

```

## Notes
- Default I²C address: **0x6B** (SEN6x), **0x6C** (SEN60).
- Start continuous measurement: 0x0021 (SEN6x) / 0x2152 (SEN60).
- Read measured values (examples): 0x0471 (SEN63C), 0x0446 (SEN65), 0x0300 (SEN66), 0x0467 (SEN68), 0xEC05 (SEN60).
- Scaling per datasheet:
  - PMx [µg/m³] = value / 10
  - RH [%] = value / 100
  - T [°C] = value / 200
  - VOC index = value / 10
  - NOx index = value / 10
  - CO₂ [ppm] = value
  - HCHO [ppb] = value

## License
MIT
