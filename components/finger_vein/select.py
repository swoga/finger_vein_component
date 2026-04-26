import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent, finger_vein_ns

DEPENDENCIES = ["finger_vein"]

CONF_BAUD_RATE = "baud_rate"

FingerVeinBaudSelect = finger_vein_ns.class_("FingerVeinBaudSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_BAUD_RATE): select.select_schema(FingerVeinBaudSelect),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_BAUD_RATE in config:
        var = cg.new_Pvariable(config[CONF_BAUD_RATE][CONF_ID], parent)
        await select.register_select(
            var,
            config[CONF_BAUD_RATE],
            options=["9600", "19200", "38400", "57600", "115200"],
        )
        cg.add(parent.set_baud_select(var))
