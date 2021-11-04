# https://github.com/jvanderneutstulen/esphome/tree/itho-ecofanrft/esphome/components/itho_ecofanrft
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome import pins
from esphome.automation import maybe_simple_id
from esphome.components import spi
from esphome.const import CONF_ID, CONF_SPI_ID, CONF_CS_PIN

DEPENDENCIES = ['spi']

CONF_ITHO_ECOFANRFT_ID = 'itho_ecofanrft_id'

itho_ecofanrft_ns = cg.esphome_ns.namespace('itho_ecofanrft')
IthoEcoFanRftComponent = itho_ecofanrft_ns.class_('IthoEcoFanRftComponent',
                                                  cg.Component, spi.SPIDevice)
SPIComponent = spi_ns.class_('SPIComponent', cg.Component)

# Actions
JoinAction = itho_ecofanrft_ns.class_('JoinAction', automation.Action)

MULTI_CONF = True
AUTOLOAD = ['fan']

CONF_ITHO_IRQ_PIN = 'irq_pin'
CONF_RF_ADDRESS = 'rf_address'
CONF_PEER_RF_ADDRESS = 'peer_rf_address'

def rf_address(value):
    value = string_strict(value)
    parts = value.split(':')
    if len(parts) != 3:
        raise Invalid("RF Address must consist of 3 : (colon) separated parts")
    parts_int = []

    if any(len(part) != 2 for part in parts):
        raise Invalid("RF Address must be format XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            raise Invalid("RF Address parts must be hexadecimal values from 00 to FF")

    return core.RFAddress(*parts_int)

def validate(config):
    if CONF_PEER_RF_ADDRESS in config:
        if config[CONF_PEER_RF_ADDRESS] == config[CONF_RF_ADDRESS]:
            raise cv.Invalid("RF address cannot be the same as peer RF address!")

    return config

SPI_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_SPI_ID): cv.use_id(SPIComponent),
    cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
})

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(IthoEcoFanRftComponent),
    cv.Required(CONF_ITHO_IRQ_PIN): pins.gpio_input_pin_schema,
    cv.Required(CONF_RF_ADDRESS): rf_address,
    cv.Optional(CONF_PEER_RF_ADDRESS): rf_address,
}).extend(cv.COMPONENT_SCHEMA).extend(SPI_DEVICE_SCHEMA), validate)


ECOFAN_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(IthoEcoFanRftComponent),
})


@automation.register_action('itho_ecofanrft.join', JoinAction, ECOFAN_ACTION_SCHEMA)
def fan_join_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    irq = yield cg.gpio_pin_expression(config[CONF_ITHO_IRQ_PIN])
    cg.add(var.set_irq_pin(irq))

    cg.add(var.set_rf_address(config[CONF_RF_ADDRESS].as_hex))
    if CONF_PEER_RF_ADDRESS in config:
        cg.add(var.set_peer_rf_address(config[CONF_PEER_RF_ADDRESS].as_hex))

    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)