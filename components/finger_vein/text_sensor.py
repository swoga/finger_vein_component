import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent

DEPENDENCIES = ["finger_vein"]

CONF_REGISTERED_USERS_DETAILS = "registered_users_details"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_REGISTERED_USERS_DETAILS): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_REGISTERED_USERS_DETAILS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_REGISTERED_USERS_DETAILS])
        cg.add(parent.set_registered_users_details_sensor(sens))