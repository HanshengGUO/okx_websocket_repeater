#ifndef REPEATER_MESSAGE_PROCESSOR_HPP
#define REPEATER_MESSAGE_PROCESSOR_HPP

#include <string>
#include <string_view>
#include <unordered_set>
#include <mutex>
#include <functional>
#include <cstdint>

namespace repeater {

/**
 * 这是一个线程安全的类，它使用一个无序集合来跟踪已见过的`seqId`。
 * 当收到一个具有新`seqId`的消息时，它会调用一个回调函数来转发该消息。
 */
class MessageProcessor {
public:
    /**
     * @brief 构造函数。
     * @param forward_callback 当收到新的、唯一的消息时调用的回调函数。
     * @param debug 是否启用调试模式打印日志。
     */
    MessageProcessor(std::function<void(std::string_view)> forward_callback, bool debug);

    /**
     * @brief 处理传入的WebSocket消息。
     * @param message 从WebSocket客户端收到的原始消息。
     */
    void process(std::string_view message);

private:
    std::unordered_set<int64_t> seen_seq_ids_;
    std::mutex mutex_;
    std::function<void(std::string_view)> forward_callback_;
    bool debug_;
};

} // namespace repeater

#endif // REPEATER_MESSAGE_PROCESSOR_HPP