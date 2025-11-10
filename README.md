# ESPHome Component for Sensirion SEN6x (SEN60–SEN68)

Local external component for ESPHome (>= 2025.10.x) to read **Sensirion SEN6x** air‑quality sensors via I²C.
Supports **SEN60, SEN63C, SEN65, SEN66, SEN68** with model auto/selection and publishes all signals
available per model (PMx, T, RH, VOC, NOx, CO₂, HCHO – see table). Works with **ESP‑IDF** and **Arduino**.

> This is a *local external component*: no internet required. Drop `components/sen6x` next to your YAML.

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
| SDA  | Blue | GPIO6    | I²C data |
| SCL  | Yellow | GPIO7  | I²C clock |

## Quick start (ESP‑IDF)
```yaml
esphome:
  name: sen6x_idf

esp32:
  board: esp32-c6-devkitc-1
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
api:
ota:

i2c:
  sda: GPIO6
  scl: GPIO7
  id: bus_a
  scan: true

external_components:
  - source: github://Cupra85/esphome_sen6x  # after you push the repo
    # or: - source: components/sen6x        # when used locally

sensor:
  - platform: sen6x
    i2c_id: bus_a
    address: 0x6B       # 0x6B for SEN6x; 0x6C for SEN60
    update_interval: 10s
    model: "auto"       # or: "SEN60","SEN63C","SEN65","SEN66","SEN68"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
    co2:
      name: "CO2"
    voc_index:
      name: "VOC Index"
    nox_index:
      name: "NOx Index"
    hcho:
      name: "HCHO"
    pm_1_0:
      name: "PM1.0"
    pm_2_5:
      name: "PM2.5"
    pm_4_0:
      name: "PM4.0"
    pm_10_0:
      name: "PM10"
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
