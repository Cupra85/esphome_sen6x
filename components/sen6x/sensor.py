from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ID,
    CONF_OFFSET,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_4_0,
    CONF_PM_10_0,
    CONF_CO2,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_TEMPERATURE,
    ICON_CHEMICAL_WEAPON,
    ICON_RADIATOR,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

# ---------- NEW Constant Names ----------
CONF_NC_0_5 = "number_concentration_0_5"
CONF_NC_1_0 = "number_concentration_1_0"
CONF_NC_2_5 = "number_concentration_2_5"
CONF_NC_4_0 = "number_concentration_4_0"
CONF_NC_10_0 = "number_concentration_10_0"

CONF_ALGORITHM_TUNING = "algorithm_tuning"

CODEOWNERS = ["@martgras"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
SEN5XComponent = sen6x_ns.class_("SEN5XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice)

# ------------ Tuning Schema -------------
GAS_SENSOR = cv.Schema(
    {
        cv.Optional(CONF_ALGORITHM_TUNING): cv.Schema(
            {
                cv.Optional("index_offset", default=100): cv.int_range(1, 250),
                cv.Optional("learning_time_offset_hours", default=12): cv.int_range(1, 1000),
                cv.Optional("learning_time_gain_hours", default=12): cv.int_range(1, 1000),
                cv.Optional("gating_max_duration_minutes", default=720): cv.int_range(0, 3000),
                cv.Optional("std_initial", default=50): cv.int_,
                cv.Optional("gain_factor", default=230): cv.int_range(1, 1000),
            }
        )
    }
)

# ------------ Fix percentage trap -------------
def float_previously_pct(value):
    if isinstance(value, str) and "%" in value:
        raise cv.Invalid(
            f"The value '{value}' is a percentage. Suggested value: {float(value.strip('%')) / 100}"
        )
    return value

# ------------ CONFIG SCHEMA -------------
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SEN5XComponent),

            # ------- PM Sensors -------
            cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM25,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
                unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
                icon=ICON_CHEMICAL_WEAPON,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_PM10,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # ------- VOC, NOx, CO2 -------
            cv.Optional("voc"): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional("nox"): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_AQI,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(GAS_SENSOR),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional("voc_baseline"): cv.hex_uint16_t,

            # ------- Temperature, Humidity -------
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),

            # ------- Compensation -------
            cv.Optional(CONF_TEMPERATURE_COMPENSATION): cv.Schema(
                {
                    cv.Optional(CONF_OFFSET, default=0): cv.float_,
                    cv.Optional("normalized_offset_slope", default=0): cv.All(float_previously_pct, cv.float_),
                    cv.Optional("time_constant", default=0): cv.int_,
                }
            ),

            # ------- NEW → Number concentration sensors -------
            cv.Optional(CONF_NC_0_5): sensor.sensor_schema(unit_of_measurement="p/cm³", icon="mdi:counter", accuracy_decimals=0),
            cv.Optional(CONF_NC_1_0): sensor.sensor_schema(unit_of_measurement="p/cm³", icon="mdi:counter", accuracy_decimals=0),
            cv.Optional(CONF_NC_2_5): sensor.sensor_schema(unit_of_measurement="p/cm³", icon="mdi:counter", accuracy_decimals=0),
            cv.Optional(CONF_NC_4_0): sensor.sensor_schema(unit_of_measurement="p/cm³", icon="mdi:counter", accuracy_decimals=0),
            cv.Optional(CONF_NC_10_0): sensor.sensor_schema(unit_of_measurement="p/cm³", icon="mdi:counter", accuracy_decimals=0),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B))
)

# ------------ Map PM, VOC, NOx etc. -------------
SENSOR_MAP = {
    CONF_PM_1_0: "set_pm_1_0_sensor",
    CONF_PM_2_5: "set_pm_2_5_sensor",
    CONF_PM_4_0: "set_pm_4_0_sensor",
    CONF_PM_10_0: "set_pm_10_0_sensor",
    "voc": "set_voc_sensor",
    "nox": "set_nox_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
    CONF_CO2: "set_co2_sensor",
}

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Map standard sensors
    for key, func in SENSOR_MAP.items():
        if key in config:
            cg.add(getattr(var, func)(await sensor.new_sensor(config[key])))

    # Map number concentration sensors
    for (cfg, func) in [
        (CONF_NC_0_5, "set_nc_0_5_sensor"),
        (CONF_NC_1_0, "set_nc_1_0_sensor"),
        (CONF_NC_2_5, "set_nc_2_5_sensor"),
        (CONF_NC_4_0, "set_nc_4_0_sensor"),
        (CONF_NC_10_0, "set_nc_10_0_sensor"),
    ]:
        if cfg in config:
            cg.add(getattr(var, func)(await sensor.new_sensor(config[cfg])))

# ---- Actions ----
StartMeasurementAction = sen6x_ns.class_("StartMeasurementAction", automation.Action)
StopMeasurementAction = sen6x_ns.class_("StopMeasurementAction", automation.Action)
StartFanAction = sen6x_ns.class_("StartFanAction", automation.Action)

SEN5X_ACTION_SCHEMA = maybe_simple_id({cv.Required(CONF_ID): cv.use_id(SEN5XComponent)})

@automation.register_action("sen6x.start_measurement", StartMeasurementAction, SEN5X_ACTION_SCHEMA)
async def sen6x_start_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)

@automation.register_action("sen6x.stop_measurement", StopMeasurementAction, SEN5X_ACTION_SCHEMA)
async def sen6x_stop_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)

@automation.register_action("sen6x.start_fan_cleaning", StartFanAction, SEN5X_ACTION_SCHEMA)
async def sen6x_fan_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
