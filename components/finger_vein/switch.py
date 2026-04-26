import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent, finger_vein_ns

DEPENDENCIES = ["finger_vein"]

CONF_DUP_CHECK = "dup_check"
CONF_SAME_FINGER = "same_finger"

FingerVeinSettingSwitch = finger_vein_ns.class_(
    "FingerVeinSettingSwitch", switch.Switch
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_DUP_CHECK): switch.switch_schema(FingerVeinSettingSwitch),
        cv.Optional(CONF_SAME_FINGER): switch.switch_schema(FingerVeinSettingSwitch),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_DUP_CHECK in config:
        var = cg.new_Pvariable(config[CONF_DUP_CHECK][CONF_ID], parent)
        await switch.register_switch(var, config[CONF_DUP_CHECK])
        cg.add(var.set_setting_code(0))
        cg.add(parent.set_dup_check_switch(var))
    if CONF_SAME_FINGER in config:
        var = cg.new_Pvariable(config[CONF_SAME_FINGER][CONF_ID], parent)
        await switch.register_switch(var, config[CONF_SAME_FINGER])
        cg.add(var.set_setting_code(1))
        cg.add(parent.set_same_finger_switch(var))
