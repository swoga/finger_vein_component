import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT

from . import CONF_FINGER_VEIN_ID, FingerVeinComponent

DEPENDENCIES = ["finger_vein"]

CONF_MATCHED_USER_ID = "matched_user_id"
CONF_REGISTERED_USERS = "registered_users"
CONF_MAX_USERS = "max_users"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
        cv.Optional(CONF_MATCHED_USER_ID): sensor.sensor_schema(
            accuracy_decimals=0,
            icon="mdi:account-key",
        ),
        cv.Optional(CONF_REGISTERED_USERS): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:account-group",
        ),
        cv.Optional(CONF_MAX_USERS): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:account-multiple",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    if CONF_MATCHED_USER_ID in config:
        sens = await sensor.new_sensor(config[CONF_MATCHED_USER_ID])
        cg.add(parent.set_matched_user_id_sensor(sens))
    if CONF_REGISTERED_USERS in config:
        sens = await sensor.new_sensor(config[CONF_REGISTERED_USERS])
        cg.add(parent.set_registered_users_sensor(sens))
    if CONF_MAX_USERS in config:
        sens = await sensor.new_sensor(config[CONF_MAX_USERS])
        cg.add(parent.set_max_users_sensor(sens))
