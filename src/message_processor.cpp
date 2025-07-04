#include "repeater/message_processor.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <chrono>

namespace repeater {

MessageProcessor::MessageProcessor(std::function<void(std::string_view)> forward_callback, bool debug)
    : forward_callback_(std::move(forward_callback)), debug_(debug) {}

void MessageProcessor::process(std::string_view message) {
    try {
        auto json_msg = nlohmann::json::parse(message);

        // 订阅成功等事件消息
        if (!json_msg.contains("arg") || !json_msg.contains("data")) {
            if (debug_) {
                std::cout << "[Processor] Ignored non-data message: " << message << std::endl;
            }
            return;
        }

        // 提取seqId
        const auto& data_array = json_msg["data"];
        if (data_array.empty() || !data_array[0].contains("seqId")) {
             if (debug_) {
                std::cout << "[Processor] Message missing seqId: " << message << std::endl;
            }
            return;
        }
        
        int64_t seq_id = data_array[0]["seqId"].get<int64_t>();

        // 线程安全的去重检查
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (seen_seq_ids_.count(seq_id)) {
                // 重复消息，直接丢弃
                if (debug_) {
                    std::cout << "[Processor] Duplicate message with seqId: " << seq_id << std::endl;
                }
                return;
            }
            // 新的seqId，记录下来
            seen_seq_ids_.insert(seq_id);
        }

        // 如果是新的，就转发出去
        if (debug_) {
            auto now = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            std::cout << "[Processor] Forwarding new message with seqId: " << seq_id << " at " << ms << std::endl;
        }
        forward_callback_(message);

    } catch (const nlohmann::json::parse_error& e) {
        if (debug_) {
            std::cerr << "[Processor] JSON parse error: " << e.what() << "\nMessage: " << message << std::endl;
        }
    }
}

} // namespace repeater