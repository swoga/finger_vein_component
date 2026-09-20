#include "finger_vein.h"

#include <algorithm>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome
{
    namespace finger_vein
    {

        static const char *const TAG = "finger_vein";

        void FingerVeinComponent::loop()
        {
            while (this->available())
            {
                this->rx_buffer_.push_back(this->read());
            }
            // Discard whole-packet blocks to avoid splitting a valid frame.
            while (this->rx_buffer_.size() > 512)
            {
                this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + XG_PACKET_SIZE);
            }

            Packet packet;
            while (this->parse_next_packet_(&packet))
            {
                this->process_packet_(packet);
            }

            if (
                this->active_op != Operation::IDLE &&
                this->active_op != Operation::IDENTIFY_FREE)
            {
                auto elapsed = millis() - this->op_started_ms_;
                if (elapsed > this->op_timeout_ms_)
                {
                    ESP_LOGW(TAG, "operation timeout (op: %s, time: %ums)", operation_to_string_(this->active_op), static_cast<unsigned>(elapsed));
                    this->reset_operation_(true);
                }
            }
        }

        void FingerVeinComponent::update()
        {
            if (!this->connected_)
            {
                this->maybe_connect_();
            }

            if (this->active_op == Operation::IDLE)
            {
                if (this->release_poll_pending_)
                {
                    this->release_poll_pending_ = false;
                    this->poll_for_release_();
                }
                else
                {
                    this->start_identify_free_();
                }
            }
        }

        void FingerVeinComponent::dump_config()
        {
            ESP_LOGCONFIG(TAG, "Finger Vein Component:");
            ESP_LOGCONFIG(TAG, "  Address: %u", this->address_);
            ESP_LOGCONFIG(TAG, "  Connected: %s", YESNO(this->connected_));
            ESP_LOGCONFIG(TAG, "  Connect timeout: %ums", static_cast<unsigned>(this->connect_timeout_ms_));
            ESP_LOGCONFIG(TAG, "  Operation timeout: %ums", static_cast<unsigned>(this->operation_timeout_ms_));
        }

        float FingerVeinComponent::get_setup_priority() const { return setup_priority::DATA; }

        bool FingerVeinComponent::request_verify()
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this]
                                         { this->request_verify(); });
            }

            if (this->matched_user_id_sensor_ != nullptr)
            {
                this->matched_user_id_sensor_->publish_state(0);
            }

            const uint32_t finger_timeout_ms = this->get_finger_timeout_ms_();
            if (!this->begin_operation_(Operation::VERIFY, finger_timeout_ms))
            {
                return false;
            }
            uint8_t payload[10] = {0};
            this->send_command_(XG_CMD_VERIFY, payload, sizeof(payload));
            ESP_LOGD(TAG, "started verify operation with timeout %ums", static_cast<unsigned>(finger_timeout_ms));
            return true;
        }

        void FingerVeinComponent::handle_verify_(const Packet &packet)
        {
            switch (packet.data[0])
            {
            case XG_ERR_SUCCESS:
            {
                const uint8_t user_id = packet.data[1];
                if (this->matched_user_id_sensor_ != nullptr)
                {
                    this->matched_user_id_sensor_->publish_state(static_cast<float>(user_id));
                }
                ESP_LOGI(TAG, "verify operation: matched user_id %u", static_cast<unsigned>(user_id));
                this->reset_operation_(false);
                this->poll_for_release_();
                break;
            }
            case XG_INPUT_FINGER:
                this->op_started_ms_ = millis();
                this->place_finger_callback_();
                ESP_LOGD(TAG, "verify operation: input finger");
                break;
            case XG_RELEASE_FINGER:
                this->op_started_ms_ = millis();
                this->release_finger_callback_();
                ESP_LOGD(TAG, "verify operation: release finger");
                break;
            default:
                ESP_LOGW(TAG, "verify operation failed with code %s", this->code_to_string_(packet.data[0]));
                this->verify_failed_callback_();
                this->reset_operation_(true);
                break;
            }
        }

        bool FingerVeinComponent::request_enroll(uint8_t user_id)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, user_id]
                                         { this->request_enroll(user_id); });
            }

            if (user_id == 0)
            {
                if (!this->begin_operation_(Operation::ENROLL_GET_EMPTY, this->operation_timeout_ms_))
                {
                    return false;
                }
                this->send_command_(XG_CMD_GET_EMPTY_ID, nullptr, 0);
                ESP_LOGI(TAG, "get next empty user_id for enroll");
                return true;
            }

            if (user_id > 100)
            {
                ESP_LOGE(TAG, "invalid user_id %u for enroll", static_cast<unsigned>(user_id));
                return false;
            }

            const uint32_t finger_timeout_ms = this->get_finger_timeout_ms_();
            if (!this->begin_operation_(Operation::ENROLL, finger_timeout_ms))
            {
                return false;
            }
            this->pending_user_id_ = user_id;
            uint8_t payload[12] = {0};
            payload[0] = user_id;
            payload[5] = 3;
            payload[10] = 10;
            this->send_command_(XG_CMD_ENROLL, payload, sizeof(payload));
            ESP_LOGI(TAG, "started enroll operation for user_id %u with timeout %ums", static_cast<unsigned>(user_id), static_cast<unsigned>(finger_timeout_ms));
            return true;
        }

        void FingerVeinComponent::handle_enroll_get_empty_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            this->reset_operation_(false);
            const uint8_t user_id = packet.data[1];
            this->request_enroll(user_id);
        }

        void FingerVeinComponent::handle_enroll_(const Packet &packet)
        {
            switch (packet.data[0])
            {
            case XG_ERR_SUCCESS:
                ESP_LOGI(TAG, "enroll operation successful for user_id %u", static_cast<unsigned>(this->pending_user_id_));
                this->enroll_success_callback_(this->pending_user_id_);
                this->reset_operation_(false);
                return;
            case XG_INPUT_FINGER:
                this->op_started_ms_ = millis();
                this->place_finger_callback_();
                ESP_LOGI(TAG, "enroll operation: input finger");
                return;
            case XG_RELEASE_FINGER:
                this->op_started_ms_ = millis();
                this->release_finger_callback_();
                ESP_LOGI(TAG, "enroll operation: release finger");
                return;
            default:
                this->reset_operation_(true);
                return;
            }
        }

        bool FingerVeinComponent::request_clear_user(uint8_t user_id)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, user_id]
                                         { this->request_clear_user(user_id); });
            }

            if (user_id == 0 || user_id > 100)
            {
                ESP_LOGE(TAG, "invalid user_id %u for clear_user", static_cast<unsigned>(user_id));
                return false;
            }

            if (!this->begin_operation_(Operation::CLEAR_USER, this->operation_timeout_ms_))
            {
                return false;
            }
            uint8_t payload[2] = {user_id, 0};
            this->send_command_(XG_CMD_CLEAR_ENROLL, payload, sizeof(payload));
            ESP_LOGI(TAG, "started clear_user operation for user_id %u", static_cast<unsigned>(user_id));
            return true;
        }

        bool FingerVeinComponent::request_clear_all()
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this]
                                         { this->request_clear_all(); });
            }

            if (!this->begin_operation_(Operation::CLEAR_ALL, 30000u))
            {
                return false;
            }
            this->send_command_(XG_CMD_CLEAR_ALL_ENROLL, nullptr, 0);
            ESP_LOGI(TAG, "started clear_all operation");
            return true;
        }

        void FingerVeinComponent::handle_clear_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            this->reset_operation_(false);
            ESP_LOGI(TAG, "clear operation successful");
            this->request_enroll_info_();
        }

        bool FingerVeinComponent::request_set_security(uint8_t level)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, level]
                                         { this->request_set_security(level); });
            }
            if (level > 2)
            {
                ESP_LOGE(TAG, "invalid security level %u", static_cast<unsigned>(level));
                return false;
            }

            if (!this->begin_operation_(Operation::SET_SECURITY, this->operation_timeout_ms_))
            {
                return false;
            }
            uint8_t payload[1] = {level};
            this->send_command_(XG_CMD_SET_SECURITYLEVEL, payload, sizeof(payload));
            ESP_LOGI(TAG, "started set_security operation with level %u", static_cast<unsigned>(level));
            return true;
        }

        bool FingerVeinComponent::request_set_timeout(uint8_t seconds)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, seconds]
                                         { this->request_set_timeout(seconds); });
            }
            if (seconds == 0)
            {
                ESP_LOGE(TAG, "invalid timeout %u", static_cast<unsigned>(seconds));
                return false;
            }
            uint8_t payload[1] = {seconds};
            if (!this->begin_operation_(Operation::SET_TIMEOUT, this->operation_timeout_ms_))
            {
                return false;
            }
            this->send_command_(XG_CMD_SET_TIMEOUT, payload, sizeof(payload));
            ESP_LOGI(TAG, "started set_timeout operation with %us", static_cast<unsigned>(seconds));
            return true;
        }

        bool FingerVeinComponent::request_set_dup_check(bool enable)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, enable]
                                         { this->request_set_dup_check(enable); });
            }

            if (!this->begin_operation_(Operation::SET_DUP_CHECK, this->operation_timeout_ms_))
            {
                return false;
            }
            uint8_t payload[1] = {enable ? uint8_t(1) : uint8_t(0)};
            this->send_command_(XG_CMD_SET_DUP_CHECK, payload, sizeof(payload));
            ESP_LOGI(TAG, "started set_dup_check operation with %s", enable ? "enabled" : "disabled");
            return true;
        }

        bool FingerVeinComponent::request_set_same_finger(bool enable)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, enable]
                                         { this->request_set_same_finger(enable); });
            }

            if (!this->begin_operation_(Operation::SET_SAME_FINGER, this->operation_timeout_ms_))
            {
                return false;
            }
            uint8_t payload[1] = {enable ? uint8_t(1) : uint8_t(0)};
            this->send_command_(XG_CMD_SET_SAME_FV, payload, sizeof(payload));
            ESP_LOGI(TAG, "started set_same_finger operation with %s", enable ? "enabled" : "disabled");
            return true;
        }

        void FingerVeinComponent::request_system_info_()
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                this->cancel_for_([this]
                                  { this->request_system_info_(); });
                return;
            }

            if (!this->begin_operation_(Operation::GET_SYSTEM_INFO, this->connect_timeout_ms_))
            {
                return;
            }
            this->send_command_(XG_CMD_GET_SYSTEM_INFO, nullptr, 0);
            ESP_LOGI(TAG, "requested system info");
        }

        void FingerVeinComponent::handle_get_system_info_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            const uint8_t security = packet.data[5];
            const uint8_t timeout_s = packet.data[6];
            const bool dup_check = packet.data[7] != 0;
            const bool same_finger = packet.data[8] != 0;

            if (this->security_number_ != nullptr)
                this->security_number_->publish_state(static_cast<float>(security));
            if (this->timeout_number_ != nullptr)
                this->timeout_number_->publish_state(static_cast<float>(timeout_s));
            if (this->dup_check_switch_ != nullptr)
                this->dup_check_switch_->publish_state(dup_check);
            if (this->same_finger_switch_ != nullptr)
                this->same_finger_switch_->publish_state(same_finger);

            this->reset_operation_(false);
            ESP_LOGI(TAG, "get system info successful");
            this->request_enroll_info_();
        }

        void FingerVeinComponent::request_enroll_info_()
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                this->cancel_for_([this]
                                  { this->request_enroll_info_(); });
                return;
            }

            if (!this->begin_operation_(Operation::GET_ENROLL_INFO, this->connect_timeout_ms_))
            {
                return;
            }
            this->send_command_(XG_CMD_GET_ENROLL_INFO, nullptr, 0);
            ESP_LOGI(TAG, "requested enroll info");
        }

        void FingerVeinComponent::handle_get_enroll_info_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            const uint16_t registered = static_cast<uint16_t>(packet.data[1]) | (static_cast<uint16_t>(packet.data[2]) << 8);
            const uint16_t max_users = static_cast<uint16_t>(packet.data[9]) | (static_cast<uint16_t>(packet.data[10]) << 8);
            if (this->registered_users_sensor_ != nullptr)
                this->registered_users_sensor_->publish_state(static_cast<float>(registered));
            if (this->max_users_sensor_ != nullptr)
                this->max_users_sensor_->publish_state(static_cast<float>(max_users));

            this->reset_operation_(false);
            ESP_LOGI(TAG, "get enroll info successful");
            this->start_id_info_scan_();
        }

        void FingerVeinComponent::start_identify_free_()
        {
            if (!this->identify_free_enabled_)
            {
                return;
            }

            if (this->matched_user_id_sensor_ != nullptr)
            {
                this->matched_user_id_sensor_->publish_state(0);
            }

            if (!this->begin_operation_(Operation::IDENTIFY_FREE, 0))
            {
                return;
            }
            uint8_t payload[10] = {0};
            this->send_command_(XG_CMD_IDENTIFY_FREE, payload, sizeof(payload));
            ESP_LOGD(TAG, "started identify_free operation");
        }

        void FingerVeinComponent::handle_identify_free_(const Packet &packet)
        {
            switch (packet.data[0])
            {
            case XG_ERR_SUCCESS:
            {
                const uint8_t user_id = packet.data[1];
                if (this->matched_user_id_sensor_ != nullptr)
                    this->matched_user_id_sensor_->publish_state(static_cast<float>(user_id));
                ESP_LOGI(TAG, "identify_free operation: matched user_id %u", static_cast<unsigned>(user_id));
                this->reset_operation_(false);
                this->poll_for_release_();
                break;
            }

            case XG_INPUT_FINGER:
                this->place_finger_callback_();
                ESP_LOGD(TAG, "identify_free operation: input finger");
                break;

            case XG_RELEASE_FINGER:
                this->release_finger_callback_();
                ESP_LOGD(TAG, "identify_free operation: release finger");
                break;

            default:
                this->verify_failed_callback_();
                this->reset_operation_(false);
                break;
            }
        }

        bool FingerVeinComponent::request_get_id_info(uint8_t user_id)
        {
            if (this->active_op == Operation::IDENTIFY_FREE)
            {
                return this->cancel_for_([this, user_id]
                                         { this->request_get_id_info(user_id); });
            }

            if (user_id == 0 || user_id > 100)
            {
                ESP_LOGE(TAG, "invalid user_id %u for get_id_info", static_cast<unsigned>(user_id));
                return false;
            }

            if (!this->begin_operation_(Operation::GET_ID_INFO, this->operation_timeout_ms_))
            {
                return false;
            }
            uint8_t payload[4] = {0};
            payload[0] = user_id;
            this->pending_user_id_ = user_id;
            this->send_command_(XG_CMD_GET_ID_INFO, payload, sizeof(payload));
            ESP_LOGI(TAG, "started get_id_info operation for user_id %u", static_cast<unsigned>(user_id));
            return true;
        }

        void FingerVeinComponent::start_id_info_scan_()
        {
            this->id_info_scan_active_ = true;
            this->id_info_scan_next_ = 1;
            this->id_info_scan_results_.clear();
            this->request_next_id_info_();
        }

        void FingerVeinComponent::request_next_id_info_()
        {
            if (this->id_info_scan_next_ > 100)
            {
                this->publish_id_info_scan_();
                this->poll_for_release_();
                return;
            }

            const uint8_t user_id = this->id_info_scan_next_++;
            this->request_get_id_info(user_id);
        }

        void FingerVeinComponent::publish_id_info_scan_()
        {
            this->id_info_scan_active_ = false;
            if (this->registered_users_details_sensor_ == nullptr)
            {
                return;
            }

            std::string result = "{";
            bool first = true;
            for (const auto &entry : this->id_info_scan_results_)
            {
                if (!first)
                {
                    result += ",";
                }
                result += std::to_string(entry.first);
                result += ":";
                result += std::to_string(entry.second);
                first = false;
            }
            result += "}";
            this->registered_users_details_sensor_->publish_state(result);
        }

        void FingerVeinComponent::handle_get_id_info_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            const uint8_t template_count = packet.data[1];
            ESP_LOGI(TAG, "get_id_info operation successful for user_id %u with template_count %u", static_cast<unsigned>(this->pending_user_id_), static_cast<unsigned>(template_count));
            this->reset_operation_(false);
            if (this->id_info_scan_active_)
            {
                if (template_count > 0)
                {
                    this->id_info_scan_results_[this->pending_user_id_] = template_count;
                }
                this->request_next_id_info_();
            }
        }

        void FingerVeinComponent::poll_for_release_()
        {
            if (!this->identify_free_enabled_)
            {
                return;
            }

            if (!this->begin_operation_(Operation::POLL_FINGER, this->operation_timeout_ms_))
            {
                this->reset_operation_(true);
                return;
            }
            this->send_command_(XG_CMD_FINGER_STATUS, nullptr, 0);
            ESP_LOGV(TAG, "poll for finger release");
        }

        void FingerVeinComponent::handle_poll_finger_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(false);
                return;
            }

            this->reset_operation_(false);
            const bool present = packet.data[1] != 0;
            if (present)
            {
                this->release_poll_pending_ = true; // re-check at next update() interval
            }
        }

        bool FingerVeinComponent::cancel_for_(std::function<void()> action)
        {
            // active_op_ == IDENTIFY_FREE is checked by caller

            this->active_op = Operation::IDLE; // set to idle dummy value, so begin_operation_() doesn't fail
            this->begin_operation_(Operation::CANCEL, this->operation_timeout_ms_);
            this->pending_action_ = std::move(action);
            this->send_command_(XG_CMD_CANCEL, nullptr, 0);
            ESP_LOGD(TAG, "sent cancel command to interrupt identify_free");
            return true;
        }

        void FingerVeinComponent::handle_cancel_(const Packet &packet)
        {
            this->reset_operation_(false);
            if (this->pending_action_)
            {
                auto action = std::move(this->pending_action_);
                this->pending_action_ = nullptr;
                action();
            }
        }

        bool FingerVeinComponent::begin_operation_(Operation op, uint32_t timeout_ms)
        {
            if (this->active_op != Operation::IDLE)
            {
                ESP_LOGW(TAG, "cannot start operation %s because another operation is active (%s)", operation_to_string_(op), operation_to_string_(this->active_op));
                return false;
            }
            this->active_op = op;
            this->op_started_ms_ = millis();
            this->op_timeout_ms_ = timeout_ms;
            this->flush();
            this->rx_buffer_.clear();
            return true;
        }

        void FingerVeinComponent::send_command_(uint8_t cmd, const uint8_t *payload, uint8_t len)
        {
            Packet packet{};
            packet.address = this->address_;
            packet.cmd = cmd;
            packet.encode = 0;
            packet.data_len = len;
            if (payload != nullptr && len > 0)
            {
                std::memcpy(packet.data, payload, std::min<size_t>(len, sizeof(packet.data)));
            }

            uint8_t frame[XG_PACKET_SIZE] = {0};
            frame[0] = 0xBB;
            frame[1] = 0xAA;
            frame[2] = packet.address;
            frame[3] = packet.cmd;
            frame[4] = packet.encode;
            frame[5] = packet.data_len;
            std::memcpy(&frame[6], packet.data, sizeof(packet.data));

            packet.checksum = this->checksum_(frame, 22);
            frame[22] = static_cast<uint8_t>(packet.checksum & 0xFF);
            frame[23] = static_cast<uint8_t>((packet.checksum >> 8) & 0xFF);

            this->write_array(frame, sizeof(frame));
            this->flush();
        }

        bool FingerVeinComponent::parse_next_packet_(Packet *packet)
        {
            while (this->rx_buffer_.size() >= XG_PACKET_SIZE)
            {
                if (this->rx_buffer_[0] != 0xBB ||
                    this->rx_buffer_[1] != 0xAA)
                {
                    this->rx_buffer_.erase(this->rx_buffer_.begin());
                    continue;
                }

                uint16_t expected = this->checksum_(this->rx_buffer_.data(), 22);
                uint16_t got = static_cast<uint16_t>(this->rx_buffer_[22]) | (static_cast<uint16_t>(this->rx_buffer_[23]) << 8);
                if (expected != got)
                {
                    ESP_LOGW(TAG, "Checksum mismatch exp=0x%04X got=0x%04X", expected, got);
                    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 2);
                    continue;
                }

                packet->address = this->rx_buffer_[2];
                packet->cmd = this->rx_buffer_[3];
                packet->encode = this->rx_buffer_[4];
                packet->data_len = this->rx_buffer_[5];
                std::memcpy(packet->data, &this->rx_buffer_[6], sizeof(packet->data));
                packet->checksum = got;

                this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + XG_PACKET_SIZE);
                return true;
            }
            return false;
        }

        void FingerVeinComponent::process_packet_(const Packet &packet)
        {
            switch (this->active_op)
            {
            case Operation::IDLE:
                return;
            case Operation::CONNECT:
                this->handle_connect_(packet);
                break;
            case Operation::GET_SYSTEM_INFO:
                this->handle_get_system_info_(packet);
                break;
            case Operation::GET_ENROLL_INFO:
                this->handle_get_enroll_info_(packet);
                break;
            case Operation::IDENTIFY_FREE:
                this->handle_identify_free_(packet);
                break;
            case Operation::POLL_FINGER:
                this->handle_poll_finger_(packet);
                break;
            case Operation::CANCEL:
                this->handle_cancel_(packet);
                break;
            case Operation::VERIFY:
                this->handle_verify_(packet);
                break;
            case Operation::ENROLL_GET_EMPTY:
                this->handle_enroll_get_empty_(packet);
                break;
            case Operation::ENROLL:
                this->handle_enroll_(packet);
                break;
            case Operation::GET_ID_INFO:
                this->handle_get_id_info_(packet);
                break;
            case Operation::CLEAR_USER:
            case Operation::CLEAR_ALL:
                this->handle_clear_(packet);
                break;
            default:
                ESP_LOGE(TAG, "received packet for unknown operation %s", operation_to_string_(this->active_op));
                this->reset_operation_(true);
                break;
            }
        }

        void FingerVeinComponent::reset_operation_(bool error)
        {
            ESP_LOGV(TAG, "reset operation %s (error: %s)", operation_to_string_(this->active_op), YESNO(error));

            if (error)
            {
                if (this->active_op == Operation::CONNECT)
                {
                    this->connected_ = false;
                }
            }
            this->active_op = Operation::IDLE;
            this->op_started_ms_ = 0;
            this->op_timeout_ms_ = 0;
        }

        void FingerVeinComponent::maybe_connect_()
        {
            if (this->active_op != Operation::IDLE)
            {
                return;
            }

            uint8_t payload[8] = {0};
            for (size_t i = 0; i < sizeof(payload) && i < this->password_.size(); i++)
            {
                payload[i] = static_cast<uint8_t>(this->password_[i]);
            }

            if (!this->begin_operation_(Operation::CONNECT, this->connect_timeout_ms_))
            {
                return;
            }

            // cancel potentially stale operations (e.g. if device was reset while we were in the middle of something)
            this->send_command_(XG_CMD_CANCEL, nullptr, 0);
            this->send_command_(XG_CMD_CONNECTION, payload, sizeof(payload));
            ESP_LOGI(TAG, "connecting");
        }

        void FingerVeinComponent::handle_connect_(const Packet &packet)
        {
            if (packet.data[0] != XG_ERR_SUCCESS)
            {
                this->reset_operation_(true);
                return;
            }

            this->connected_ = true;
            this->active_op = Operation::IDLE;
            this->op_started_ms_ = 0;
            this->op_timeout_ms_ = 0;
            ESP_LOGI(TAG, "connected");
            this->request_system_info_();
        }

        uint32_t FingerVeinComponent::get_finger_timeout_ms_() const
        {
            if (this->timeout_number_ == nullptr)
            {
                return this->operation_timeout_ms_;
            }

            return static_cast<uint32_t>(this->timeout_number_->state * 1000.0f) + 1000u;
        }

        uint16_t FingerVeinComponent::checksum_(const uint8_t *data, size_t len) const
        {
            uint16_t sum = 0;
            for (size_t i = 0; i < len; i++)
            {
                sum = static_cast<uint16_t>(sum + data[i]);
            }
            return sum;
        }

        const char *FingerVeinComponent::operation_to_string_(Operation op)
        {
            switch (op)
            {
            case Operation::IDLE:
                return "IDLE";
            case Operation::CONNECT:
                return "CONNECT";
            case Operation::GET_SYSTEM_INFO:
                return "GET_SYSTEM_INFO";
            case Operation::GET_ENROLL_INFO:
                return "GET_ENROLL_INFO";
            case Operation::IDENTIFY_FREE:
                return "IDENTIFY_FREE";
            case Operation::CANCEL:
                return "CANCEL";
            case Operation::POLL_FINGER:
                return "POLL_FINGER";
            case Operation::VERIFY:
                return "VERIFY";
            case Operation::ENROLL_GET_EMPTY:
                return "ENROLL_GET_EMPTY";
            case Operation::GET_ID_INFO:
                return "GET_ID_INFO";
            case Operation::ENROLL:
                return "ENROLL";
            case Operation::CLEAR_USER:
                return "CLEAR_USER";
            case Operation::CLEAR_ALL:
                return "CLEAR_ALL";
            case Operation::SET_SECURITY:
                return "SET_SECURITY";
            case Operation::SET_TIMEOUT:
                return "SET_TIMEOUT";
            case Operation::SET_DUP_CHECK:
                return "SET_DUP_CHECK";
            case Operation::SET_SAME_FINGER:
                return "SET_SAME_FINGER";
            default:
                return "UNKNOWN";
            }
        }

        std::string FingerVeinComponent::code_to_string_(uint8_t code) const
        {
            switch (code)
            {
            case XG_ERR_SUCCESS:
                return "success";
            case XG_ERR_FAIL:
                return "fail";
            case XG_ERR_COM:
                return "com_error";
            case XG_ERR_DATA:
                return "data_error";
            case XG_ERR_INVALID_PWD:
                return "invalid_password";
            case XG_ERR_INVALID_ID:
                return "invalid_id";
            case XG_ERR_EMPTY_ID:
                return "empty_id";
            case XG_ERR_NOT_ENOUGH:
                return "memory_full";
            case XG_ERR_NO_SAME_FINGER:
                return "no_same_finger";
            case XG_ERR_DUPLICATION_ID:
                return "duplicate_id";
            case XG_ERR_TIME_OUT:
                return "device_timeout";
            case XG_ERR_VERIFY:
                return "verify_failed";
            case XG_ERR_NO_NULL_ID:
                return "no_empty_id";
            case XG_ERR_NO_CONNECT:
                return "no_connect";
            case XG_ERR_NO_VEIN:
                return "no_vein";
            default:
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "unknown_0x%02X", code);
                return std::string(buf);
            }
            }
        }

        void FingerVeinSettingNumber::control(float value)
        {
            if (this->parent_ == nullptr)
            {
                return;
            }

            const auto rounded = static_cast<uint8_t>(value);
            bool accepted = false;
            switch (this->setting_type_)
            {
            case SettingType::SECURITY:
                accepted = this->parent_->request_set_security(rounded);
                break;
            case SettingType::TIMEOUT:
                accepted = this->parent_->request_set_timeout(rounded);
                break;
            }

            if (accepted)
            {
                this->publish_state(value);
            }
        }

        void FingerVeinSettingNumber::set_setting_code(uint8_t code)
        {
            switch (code)
            {
            case 0:
                this->setting_type_ = SettingType::SECURITY;
                break;
            case 1:
                this->setting_type_ = SettingType::TIMEOUT;
                break;
            default:
                this->setting_type_ = SettingType::SECURITY;
                break;
            }
        }

        void FingerVeinSettingSwitch::write_state(bool state)
        {
            if (this->parent_ == nullptr)
            {
                return;
            }

            bool accepted = false;
            switch (this->setting_type_)
            {
            case SettingType::DUP_CHECK:
                accepted = this->parent_->request_set_dup_check(state);
                break;
            case SettingType::SAME_FINGER:
                accepted = this->parent_->request_set_same_finger(state);
                break;
            }

            if (accepted)
            {
                this->publish_state(state);
            }
        }

    } // namespace finger_vein
} // namespace esphome
