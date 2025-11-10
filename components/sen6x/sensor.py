import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, i2c
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_EMPTY,
)
from esphome.const import SensorDeviceClass  # ✅ neu für ESPHome ≥2024.7

DEPENDENCIES = ["i2c"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN6x = sen6x_ns.class_("SEN6x", cg.PollingComponent, i2c.I2CDevice)

CONF_MODEL = "model"

MODEL = cv.one_of("auto", "SEN60", "SEN63C", "SEN65", "SEN66", "SEN68", lower=True)

def _simple_schema(unit=UNIT_EMPTY):
    return sensor.sensor_schema(
        unit_of_measurement=unit,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(SEN6x),
        cv.Optional(CONF_MODEL, default="auto"): MODEL,

        cv.Optional("temperature"): sensor.sensor_schema(
            UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("humidity"): sensor.sensor_schema(
            UNIT_PERCENT, DEVICE_CLASS_HUMIDITY, STATE_CLASS_MEASUREMENT
        ),
        cv.Optional("co2"): sensor.sensor_schema(
            unit_of_measurement="ppm",
            device_class=SensorDeviceClass.CO2,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional("voc_index"): _simple_schema("index"),
        cv.Optional("nox_index"): _simple_schema("index"),
        cv.Optional("hcho"): _simple_schema("ppb"),
        cv.Optional("pm_1_0"): _simple_schema("µg/m³"),
        cv.Optional("pm_2_5"): _simple_schema("µg/m³"),
        cv.Optional("pm_4_0"): _simple_schema("µg/m³"),
        cv.Optional("pm_10_0"): _simple_schema("µg/m³"),
    })
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x6B))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))

    for key, setter in [
        ("temperature", "set_temperature_sensor"),
        ("humidity", "set_humidity_sensor"),
        ("co2", "set_co2_sensor"),
        ("voc_index", "set_voc_sensor"),
        ("nox_index", "set_nox_sensor"),
        ("hcho", "set_hcho_sensor"),
        ("pm_1_0", "set_pm1_sensor"),
        ("pm_2_5", "set_pm2_5_sensor"),
        ("pm_4_0", "set_pm4_0_sensor"),
        ("pm_10_0", "set_pm10_sensor"),
    ]:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(sens))
