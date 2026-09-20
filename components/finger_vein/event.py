import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import event
from esphome.const import CONF_EVENT_TYPES

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent

DEPENDENCIES = ["finger_vein"]

CONF_USER_IDS = "user_ids"

CONFIG_SCHEMA = event.event_schema().extend(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Required(CONF_USER_IDS): cv.ensure_list(cv.int_range(min=1, max=100)),
        cv.Required(CONF_EVENT_TYPES): cv.ensure_list(cv.string_strict),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    var = await event.new_event(config, event_types=config[CONF_EVENT_TYPES])
    for user_id in config[CONF_USER_IDS]:
        cg.add(parent.set_user_event(user_id, var))