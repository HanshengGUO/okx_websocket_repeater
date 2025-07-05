#include "repeater/message_processor.hpp"
#include "nlohmann/json.hpp"
#include <iostream>

namespace repeater {

MessageProcessor::MessageProcessor(std::function<void(std::string_view)> forward_callback, bool debug)
    : forward_callback_(std::move(forward_callback)), debug_(debug) {}

void MessageProcessor::process(std::string_view message) {
    try {
        auto json_msg = nlohmann::json::parse(message);

        if (!json_msg.contains("arg") || !json_msg.contains("data")) {
            return;
        }

        const auto& data_array = json_msg["data"];
        if (data_array.empty() || !data_array[0].contains("seqId")) {
            return;
        }
        
        int64_t seq_id = data_array[0]["seqId"].get<int64_t>();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (seq_id <= max_seq_id_) {
                if (debug_) {
                    std::cout << "[Processor] Discarding old or duplicate message with seqId: " << seq_id 
                              << " (max is " << max_seq_id_ << ")" << std::endl;
                }
                return; // 丢弃旧的或重复的消息
            }
            max_seq_id_ = seq_id;
        }

        // 转发最新的消息
        if (debug_) {
            std::cout << "[Processor] Forwarding newest message with seqId: " << seq_id << std::endl;
        }
        forward_callback_(message);

    } catch (const nlohmann::json::parse_error& e) {
        if (debug_) {
            std::cerr << "[Processor] JSON parse error: " << e.what() << "\nMessage: " << message << std::endl;
        }
    }
}

} // namespace repeater