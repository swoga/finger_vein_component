#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/number/number.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <functional>

namespace esphome
{
    namespace finger_vein
    {

        static const size_t XG_PACKET_SIZE = 24;

        enum XGCommand : uint8_t
        {
            XG_CMD_CONNECTION = 0x01,
            XG_CMD_CLOSE_CONNECTION = 0x02,
            XG_CMD_GET_SYSTEM_INFO = 0x03,
            XG_CMD_SET_SECURITYLEVEL = 0x07,
            XG_CMD_SET_TIMEOUT = 0x08,
            XG_CMD_SET_DUP_CHECK = 0x09,
            XG_CMD_SET_SAME_FV = 0x0D,
            XG_CMD_FINGER_STATUS = 0x10,
            XG_CMD_CLEAR_ENROLL = 0x11,
            XG_CMD_CLEAR_ALL_ENROLL = 0x12,
            XG_CMD_GET_EMPTY_ID = 0x13,
            XG_CMD_GET_ENROLL_INFO = 0x14,
            XG_CMD_GET_ID_INFO = 0x15,
            XG_CMD_ENROLL = 0x16,
            XG_CMD_VERIFY = 0x17,
            XG_CMD_IDENTIFY_FREE = 0x18,
            XG_CMD_CANCEL = 0x19,
            XG_CMD_GET_USERNAME = 0x3C,
            XG_CMD_SET_USERNAME = 0x3D,
        };

        enum XGCode : uint8_t
        {
            XG_ERR_SUCCESS = 0x00,
            XG_ERR_FAIL = 0x01,
            XG_ERR_COM = 0x02,
            XG_ERR_DATA = 0x03,
            XG_ERR_INVALID_PWD = 0x04,
            XG_ERR_INVALID_ID = 0x06,
            XG_ERR_EMPTY_ID = 0x07,
            XG_ERR_NOT_ENOUGH = 0x08,
            XG_ERR_NO_SAME_FINGER = 0x09,
            XG_ERR_DUPLICATION_ID = 0x0A,
            XG_ERR_TIME_OUT = 0x0B,
            XG_ERR_VERIFY = 0x0C,
            XG_ERR_NO_NULL_ID = 0x0D,
            XG_ERR_NO_CONNECT = 0x0F,
            XG_ERR_NO_VEIN = 0x11,

            XG_INPUT_FINGER = 0x20,   // user should place finger
            XG_RELEASE_FINGER = 0x21, // user should release finger
        };

        enum class Operation : uint8_t
        {
            IDLE,
            CONNECT,
            GET_SYSTEM_INFO,
            GET_ENROLL_INFO,
            IDENTIFY_FREE,
            CANCEL,
            POLL_FINGER,
            VERIFY,
            ENROLL_GET_EMPTY,
            ENROLL,
            GET_ID_INFO,
            CLEAR_USER,
            CLEAR_ALL,
            SET_SECURITY,
            SET_TIMEOUT,
            SET_DUP_CHECK,
            SET_SAME_FINGER,
            GET_USERNAME,
            SET_USERNAME
        };

        class FingerVeinComponent : public PollingComponent, public uart::UARTDevice
        {
        public:
            void loop() override;
            void update() override;
            void dump_config() override;

            float get_setup_priority() const override;

            Operation active_op{Operation::IDLE};

            void set_address(uint8_t address) { this->address_ = address; }
            void set_password(const std::string &password) { this->password_ = password; }
            void set_connect_timeout_ms(uint32_t timeout_ms) { this->connect_timeout_ms_ = timeout_ms; }
            void set_operation_timeout_ms(uint32_t timeout_ms) { this->operation_timeout_ms_ = timeout_ms; }
            void set_identify_free_enabled(bool enabled) { this->identify_free_enabled_ = enabled; }

            void set_matched_user_id_sensor(sensor::Sensor *sensor) { this->matched_user_id_sensor_ = sensor; }
            void set_matched_username_sensor(text_sensor::TextSensor *sensor) { this->matched_username_sensor_ = sensor; }
            void set_security_number(number::Number *number) { this->security_number_ = number; }
            void set_timeout_number(number::Number *number) { this->timeout_number_ = number; }
            void set_dup_check_switch(switch_::Switch *sw) { this->dup_check_switch_ = sw; }
            void set_same_finger_switch(switch_::Switch *sw) { this->same_finger_switch_ = sw; }
            void set_registered_users_sensor(sensor::Sensor *sensor) { this->registered_users_sensor_ = sensor; }
            void set_max_users_sensor(sensor::Sensor *sensor) { this->max_users_sensor_ = sensor; }

            bool request_verify();
            bool request_enroll(uint8_t user_id = 0, const std::string &username = "");
            bool request_get_id_info(uint8_t user_id);
            bool request_clear_user(uint8_t user_id);
            bool request_clear_all();
            bool request_set_security(uint8_t level);
            bool request_set_timeout(uint8_t seconds);
            bool request_set_dup_check(bool enable);
            bool request_set_same_finger(bool enable);
            bool request_get_username(uint8_t user_id);
            bool request_set_username(uint8_t user_id, const std::string &username);

            template <typename F>
            void add_on_verify_success_callback(F &&callback)
            {
                this->verify_success_callback_.add(std::forward<F>(callback));
            }
            template <typename F>
            void add_on_verify_failed_callback(F &&callback)
            {
                this->verify_failed_callback_.add(std::forward<F>(callback));
            }
            template <typename F>
            void add_on_enroll_success_callback(F &&callback)
            {
                this->enroll_success_callback_.add(std::forward<F>(callback));
            }
            template <typename F>
            void add_on_place_finger_callback(F &&callback)
            {
                this->place_finger_callback_.add(std::forward<F>(callback));
            }
            template <typename F>
            void add_on_release_finger_callback(F &&callback)
            {
                this->release_finger_callback_.add(std::forward<F>(callback));
            }

        protected:
            struct Packet
            {
                uint8_t address;
                uint8_t cmd;
                uint8_t encode;
                uint8_t data_len;
                uint8_t data[16];
                uint16_t checksum;
            };

            bool begin_operation_(Operation op, uint32_t timeout_ms);
            void send_command_(uint8_t cmd, const uint8_t *payload, uint8_t len);
            bool parse_next_packet_(Packet *packet);
            void process_packet_(const Packet &packet);
            void reset_operation_(bool error);
            void maybe_connect_();
            void start_identify_free_();
            bool cancel_for_(std::function<void()> action);
            void poll_for_release_();
            void request_system_info_();
            void request_enroll_info_();

            void handle_connect_(const Packet &packet);
            void handle_get_system_info_(const Packet &packet);
            void handle_get_enroll_info_(const Packet &packet);
            void handle_identify_free_(const Packet &packet);
            void handle_poll_finger_(const Packet &packet);
            void handle_cancel_(const Packet &packet);
            void handle_verify_(const Packet &packet);
            void handle_enroll_get_empty_(const Packet &packet);
            void handle_enroll_(const Packet &packet);
            void handle_get_id_info_(const Packet &packet);
            void handle_clear_(const Packet &packet);
            void handle_get_username_(const Packet &packet);
            void handle_set_username_(const Packet &packet);

            uint16_t checksum_(const uint8_t *data, size_t len) const;
            uint32_t get_finger_timeout_ms_() const;
            std::string code_to_string_(uint8_t code) const;
            static const char *operation_to_string_(Operation op);

            uint8_t address_{0};
            std::string password_{"00000000"};
            uint32_t connect_timeout_ms_{1000};
            uint32_t operation_timeout_ms_{6000};

            bool connected_{false};
            bool identify_free_enabled_{true};

            uint32_t op_started_ms_{0};
            uint32_t op_timeout_ms_{0};

            std::function<void()> pending_action_{};
            bool release_poll_pending_{false};
            uint8_t pending_user_id_{0};
            std::string pending_username_{};

            sensor::Sensor *matched_user_id_sensor_{nullptr};
            text_sensor::TextSensor *matched_username_sensor_{nullptr};
            text_sensor::TextSensor *status_text_sensor_{nullptr};
            number::Number *security_number_{nullptr};
            number::Number *timeout_number_{nullptr};
            switch_::Switch *dup_check_switch_{nullptr};
            switch_::Switch *same_finger_switch_{nullptr};
            sensor::Sensor *registered_users_sensor_{nullptr};
            sensor::Sensor *max_users_sensor_{nullptr};

            LazyCallbackManager<void(uint8_t, std::string)> verify_success_callback_;
            LazyCallbackManager<void()> verify_failed_callback_;
            LazyCallbackManager<void(uint8_t)> enroll_success_callback_;
            LazyCallbackManager<void()> place_finger_callback_;
            LazyCallbackManager<void()> release_finger_callback_;

            std::vector<uint8_t> rx_buffer_{};
        };

        template <typename... Ts>
        class VerifyAction : public Action<Ts...>
        {
        public:
            explicit VerifyAction(FingerVeinComponent *parent) : parent_(parent) {}
            void play(const Ts &...x) override { this->parent_->request_verify(); }

        protected:
            FingerVeinComponent *parent_;
        };

        template <typename... Ts>
        class EnrollAction : public Action<Ts...>
        {
        public:
            explicit EnrollAction(FingerVeinComponent *parent) : parent_(parent) {}
            TEMPLATABLE_VALUE(uint8_t, user_id)
            TEMPLATABLE_VALUE(std::string, username)
            void play(const Ts &...x) override { this->parent_->request_enroll(this->user_id_.value(x...), this->username_.value(x...)); }

        protected:
            FingerVeinComponent *parent_;
        };

        template <typename... Ts>
        class ClearUserAction : public Action<Ts...>
        {
        public:
            explicit ClearUserAction(FingerVeinComponent *parent) : parent_(parent) {}
            TEMPLATABLE_VALUE(uint8_t, user_id)
            void play(const Ts &...x) override { this->parent_->request_clear_user(this->user_id_.value(x...)); }

        protected:
            FingerVeinComponent *parent_;
        };

        template <typename... Ts>
        class ClearAllAction : public Action<Ts...>
        {
        public:
            explicit ClearAllAction(FingerVeinComponent *parent) : parent_(parent) {}
            void play(const Ts &...x) override { this->parent_->request_clear_all(); }

        protected:
            FingerVeinComponent *parent_;
        };

        class FingerVeinSettingNumber : public number::Number
        {
        public:
            enum class SettingType : uint8_t
            {
                SECURITY,
                TIMEOUT
            };

            explicit FingerVeinSettingNumber(FingerVeinComponent *parent) : parent_(parent) {}

            void set_setting_type(SettingType type) { this->setting_type_ = type; }
            void set_setting_code(uint8_t code);

        protected:
            void control(float value) override;

            FingerVeinComponent *parent_;
            SettingType setting_type_{SettingType::SECURITY};
        };

        class FingerVeinSettingSwitch : public switch_::Switch
        {
        public:
            enum class SettingType : uint8_t
            {
                DUP_CHECK,
                SAME_FINGER
            };

            explicit FingerVeinSettingSwitch(FingerVeinComponent *parent) : parent_(parent) {}

            void set_setting_type(SettingType type) { this->setting_type_ = type; }
            void set_setting_code(uint8_t code)
            {
                this->setting_type_ = (code == 0) ? SettingType::DUP_CHECK : SettingType::SAME_FINGER;
            }

        protected:
            void write_state(bool state) override;

            FingerVeinComponent *parent_;
            SettingType setting_type_{SettingType::DUP_CHECK};
        };

    } // namespace finger_vein
} // namespace esphome
