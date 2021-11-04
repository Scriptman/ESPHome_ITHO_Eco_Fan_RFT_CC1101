import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import CONF_ID
from . import IthoEcoFanRftComponent, itho_ecofanrft_ns, CONF_ITHO_ECOFANRFT_ID

DEPENDENCIES = ['itho_ecofanrft']

IthoEcoFanRftFan = itho_ecofanrft_ns.class_('IthoEcoFanRftFan', fan.FanState)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(IthoEcoFanRftFan),
    cv.GenerateID(CONF_ITHO_ECOFANRFT_ID): cv.use_id(IthoEcoFanRftComponent),
})


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ITHO_ECOFANRFT_ID])
    ithofan = hub.Pget_fan()
    var = cg.Pvariable(config[CONF_ID], ithofan)
    yield fan.register_fan(var, config)