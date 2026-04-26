import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart
from esphome.const import CONF_ID, CONF_PASSWORD

CODEOWNERS = ["@swoga"]
AUTO_LOAD = ["sensor", "text_sensor", "number", "select", "switch"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

CONF_FINGER_VEIN_ID = "finger_vein_id"
CONF_ADDRESS = "address"
CONF_CONNECT_TIMEOUT_MS = "connect_timeout_ms"
CONF_OPERATION_TIMEOUT_MS = "operation_timeout_ms"
CONF_IDENTIFY_FREE_ENABLED = "identify_free"
CONF_USER_ID = "user_id"
CONF_USERNAME = "username"
CONF_ON_VERIFY_SUCCESS = "on_verify_success"
CONF_ON_VERIFY_FAILED = "on_verify_failed"
CONF_ON_ENROLL_SUCCESS = "on_enroll_success"
CONF_ON_PLACE_FINGER = "on_place_finger"
CONF_ON_RELEASE_FINGER = "on_release_finger"

finger_vein_ns = cg.esphome_ns.namespace("finger_vein")
FingerVeinComponent = finger_vein_ns.class_(
    "FingerVeinComponent", cg.PollingComponent, uart.UARTDevice
)

VerifyAction = finger_vein_ns.class_("VerifyAction", automation.Action)
EnrollAction = finger_vein_ns.class_("EnrollAction", automation.Action)
ClearUserAction = finger_vein_ns.class_("ClearUserAction", automation.Action)
ClearAllAction = finger_vein_ns.class_("ClearAllAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FingerVeinComponent),
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_PASSWORD, default="00000000"): cv.string,
            cv.Optional(CONF_CONNECT_TIMEOUT_MS, default=1000): cv.int_range(min=100, max=10000),
            cv.Optional(CONF_OPERATION_TIMEOUT_MS, default=6000): cv.int_range(min=1000, max=30000),
            cv.Optional(CONF_IDENTIFY_FREE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_ON_VERIFY_SUCCESS): automation.validate_automation({}),
            cv.Optional(CONF_ON_VERIFY_FAILED): automation.validate_automation({}),
            cv.Optional(CONF_ON_ENROLL_SUCCESS): automation.validate_automation({}),
            cv.Optional(CONF_ON_PLACE_FINGER): automation.validate_automation({}),
            cv.Optional(CONF_ON_RELEASE_FINGER): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("500ms"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_VERIFY_SUCCESS,
        "add_on_verify_success_callback",
        [(cg.uint8, "user_id"), (cg.std_string, "username")],
    ),
    automation.CallbackAutomation(
        CONF_ON_VERIFY_FAILED,
        "add_on_verify_failed_callback",
    ),
    automation.CallbackAutomation(
        CONF_ON_ENROLL_SUCCESS,
        "add_on_enroll_success_callback",
        [(cg.uint8, "user_id")],
    ),
    automation.CallbackAutomation(
        CONF_ON_PLACE_FINGER,
        "add_on_place_finger_callback",
    ),
    automation.CallbackAutomation(
        CONF_ON_RELEASE_FINGER,
        "add_on_release_finger_callback",
    ),
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    cg.add(var.set_connect_timeout_ms(config[CONF_CONNECT_TIMEOUT_MS]))
    cg.add(var.set_operation_timeout_ms(config[CONF_OPERATION_TIMEOUT_MS]))
    cg.add(var.set_identify_free_enabled(config[CONF_IDENTIFY_FREE_ENABLED]))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "finger_vein",
    require_rx=True,
    require_tx=True,
)


@automation.register_action(
    "finger_vein.verify",
    VerifyAction,
    cv.Schema({cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent)}),
    synchronous=False,
)
async def finger_vein_verify_action(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    return var


@automation.register_action(
    "finger_vein.enroll",
    EnrollAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
            cv.Optional(CONF_USER_ID, default=0): cv.templatable(
                cv.int_range(min=0, max=100)
            ),
            cv.Optional(CONF_USERNAME, default=""): cv.templatable(cv.string),
        }
    ),
    synchronous=False,
)
async def finger_vein_enroll_action(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    cg.add(var.set_user_id(await cg.templatable(config[CONF_USER_ID], args, cg.uint8)))
    cg.add(var.set_username(await cg.templatable(config[CONF_USERNAME], args, cg.std_string)))
    return var


@automation.register_action(
    "finger_vein.clear_user",
    ClearUserAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent),
            cv.Required(CONF_USER_ID): cv.templatable(cv.int_range(min=1, max=100)),
        }
    ),
    synchronous=False,
)
async def finger_vein_clear_user_action(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    cg.add(var.set_user_id(await cg.templatable(config[CONF_USER_ID], args, cg.uint8)))
    return var


@automation.register_action(
    "finger_vein.clear_all",
    ClearAllAction,
    cv.Schema({cv.GenerateID(CONF_FINGER_VEIN_ID): cv.use_id(FingerVeinComponent)}),
    synchronous=False,
)
async def finger_vein_clear_all_action(config, action_id, template_args, args):
    paren = await cg.get_variable(config[CONF_FINGER_VEIN_ID])
    var = cg.new_Pvariable(action_id, template_args, paren)
    return var
