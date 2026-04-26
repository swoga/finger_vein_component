import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent, finger_vein_ns

DEPENDENCIES = ["finger_vein"]

CONF_SECURITY = "security"
CONF_TIMEOUT = "timeout"

FingerVeinSettingNumber = finger_vein_ns.class_("FingerVeinSettingNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_SECURITY): number.number_schema(FingerVeinSettingNumber),
        cv.Optional(CONF_TIMEOUT): number.number_schema(FingerVeinSettingNumber),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_SECURITY in config:
        var = cg.new_Pvariable(config[CONF_SECURITY][CONF_ID], parent)
        await number.register_number(var, config[CONF_SECURITY], min_value=0, max_value=2, step=1)
        cg.add(var.set_setting_code(0))
        cg.add(parent.set_security_number(var))
    if CONF_TIMEOUT in config:
        var = cg.new_Pvariable(config[CONF_TIMEOUT][CONF_ID], parent)
        await number.register_number(var, config[CONF_TIMEOUT], min_value=1, max_value=255, step=1)
        cg.add(var.set_setting_code(1))
        cg.add(parent.set_timeout_number(var))
