import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent

DEPENDENCIES = ["finger_vein"]

CONF_MATCHED_USERNAME = "matched_username"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_MATCHED_USERNAME): text_sensor.text_sensor_schema(
            icon="mdi:account-badge",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_MATCHED_USERNAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MATCHED_USERNAME])
        cg.add(parent.set_matched_username_sensor(sens))
