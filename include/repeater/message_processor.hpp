#ifndef REPEATER_MESSAGE_PROCESSOR_HPP
#define REPEATER_MESSAGE_PROCESSOR_HPP

#include <string>
#include <string_view>
#include <mutex>
#include <functional>
#include <cstdint>

namespace repeater {

/**
 * 线程安全的消息处理器，它转发seqId最新的消息
 * 当收到一个具有新`seqId`的消息时，它会调用一个回调函数来转发该消息。
 * !!! 注意：它只转发seqId最新的消息，如果有更旧的消息更晚到达，则会被丢弃。
 * 它适用于Market Data的场景
 */
class MessageProcessor {
public:
    MessageProcessor(std::function<void(std::string_view)> forward_callback, bool debug);

    void process(std::string_view message);

private:
    int64_t max_seq_id_ = 0;
    std::mutex mutex_;
    std::function<void(std::string_view)> forward_callback_;
    bool debug_;
};

} // namespace repeater

#endif // REPEATER_MESSAGE_PROCESSOR_HPP