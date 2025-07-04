#ifndef REPEATER_REPEATER_CORE_HPP
#define REPEATER_REPEATER_CORE_HPP

#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <memory>

namespace repeater {

class WebSocketClient;
class WebSocketServer;
class MessageProcessor;

/**
 * 项目的核心业务逻辑控制器。
 *
 * 负责初始化和管理所有的WebSocket客户端、服务器以及消息处理器。
 * 它还管理着用于所有异步操作的io_context和线程池。
 */
class RepeaterCore {
public:
    /**
     * @brief 构造函数。
     * @param config 从JSON文件加载的配置。
     */
    explicit RepeaterCore(const nlohmann::json& config);

    /**
     * @brief 启动整个重复器系统。
     */
    void run();

private:
    nlohmann::json config_;
    bool debug_;
};

} // namespace repeater

#endif // REPEATER_REPEATER_CORE_HPP