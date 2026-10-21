"""Kamstrup 382 IEC 61107 mode A readout component for ESPHome."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_HOUR,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
)

CODEOWNERS = ["@johanelmis"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor"]

iec61107_ns = cg.esphome_ns.namespace("iec61107")
IEC61107Component = iec61107_ns.class_(
    "IEC61107Component", cg.PollingComponent, uart.UARTDevice
)

CONF_RESPONSE_TIMEOUT = "response_timeout"
CONF_FAILURE_THRESHOLD = "failure_threshold"
CONF_READOUT_OK = "readout_ok"
CONF_TYPE_NUMBER = "type_number"
CONF_SERIAL_NUMBER = "serial_number"
CONF_ENERGY = "energy"
CONF_TARIFF1 = "tariff1"
CONF_TARIFF2 = "tariff2"
CONF_HOUR_COUNTER = "hour_counter"
CONF_PULSE_INPUT = "pulse_input"
CONF_PEAK_POWER = "peak_power"
CONF_LAST_MONTH_PEAK = "last_month_peak"

# OBIS 1.20 / 1.20.1 / 1.20.2 -> cumulative energy registers (kWh, integer)
_energy_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT_HOURS,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_ENERGY,
    state_class=STATE_CLASS_TOTAL_INCREASING,
)

# OBIS 1.6 / 1.6*1 -> peak power (kW)
_power_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_KILOWATT,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_POWER,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IEC61107Component),
            cv.Optional(
                CONF_RESPONSE_TIMEOUT, default="2s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FAILURE_THRESHOLD, default=3): cv.int_range(min=1, max=255),
            cv.Optional(CONF_READOUT_OK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
            ),
            cv.Optional(CONF_TYPE_NUMBER): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_ENERGY): _energy_schema,
            cv.Optional(CONF_TARIFF1): _energy_schema,
            cv.Optional(CONF_TARIFF2): _energy_schema,
            cv.Optional(CONF_HOUR_COUNTER): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=0,
                icon="mdi:timer-outline",
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_PULSE_INPUT): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:counter",
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_PEAK_POWER): _power_schema,
            cv.Optional(CONF_LAST_MONTH_PEAK): _power_schema,
        }
    )
    .extend(cv.polling_component_schema("5min"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_response_timeout(config[CONF_RESPONSE_TIMEOUT]))

    cg.add(var.set_failure_threshold(config[CONF_FAILURE_THRESHOLD]))
    if CONF_READOUT_OK in config:
        cg.add(var.set_readout_ok_sensor(await binary_sensor.new_binary_sensor(config[CONF_READOUT_OK])))

    if CONF_TYPE_NUMBER in config:
        cg.add(var.set_type_number_sensor(await text_sensor.new_text_sensor(config[CONF_TYPE_NUMBER])))
    if CONF_SERIAL_NUMBER in config:
        cg.add(var.set_serial_number_sensor(await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])))
    if CONF_ENERGY in config:
        cg.add(var.set_energy_sensor(await sensor.new_sensor(config[CONF_ENERGY])))
    if CONF_TARIFF1 in config:
        cg.add(var.set_tariff1_sensor(await sensor.new_sensor(config[CONF_TARIFF1])))
    if CONF_TARIFF2 in config:
        cg.add(var.set_tariff2_sensor(await sensor.new_sensor(config[CONF_TARIFF2])))
    if CONF_HOUR_COUNTER in config:
        cg.add(var.set_hour_counter_sensor(await sensor.new_sensor(config[CONF_HOUR_COUNTER])))
    if CONF_PULSE_INPUT in config:
        cg.add(var.set_pulse_input_sensor(await sensor.new_sensor(config[CONF_PULSE_INPUT])))
    if CONF_PEAK_POWER in config:
        cg.add(var.set_peak_power_sensor(await sensor.new_sensor(config[CONF_PEAK_POWER])))
    if CONF_LAST_MONTH_PEAK in config:
        cg.add(var.set_last_month_peak_sensor(await sensor.new_sensor(config[CONF_LAST_MONTH_PEAK])))
