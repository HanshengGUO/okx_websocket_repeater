#include "repeater/repeater_core.hpp"
#include "repeater/websocket_client.hpp"
#include "repeater/websocket_server.hpp"
#include "repeater/message_processor.hpp"

#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

namespace repeater {

RepeaterCore::RepeaterCore(const nlohmann::json& config)
    : config_(config), debug_(config.value("debug", false)) {}

void RepeaterCore::run() {
    // 1. 获取配置
    auto const server_host = net::ip::make_address(config_["repeater_server"]["host"].get<std::string>());
    auto const server_port = config_["repeater_server"]["port"].get<unsigned short>();
    auto const okx_urls = config_["okx_connections"].get<std::vector<std::string>>();
    auto const sub_message = config_["subscription_message"].dump();
    auto const threads = config_.value("threads", 1);

    if (debug_) {
        std::cout << "[Core] Starting with " << threads << " I/O threads." << std::endl;
        std::cout << "[Core] Subscription message: " << sub_message << std::endl;
    }

    // 2. 初始化IO上下文和SSL上下文
    net::io_context ioc{static_cast<int>(threads)};
    ssl::context ctx{ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    // 3. 创建核心组件
    auto server = std::make_shared<WebSocketServer>(ioc, tcp::endpoint{server_host, server_port}, debug_);
    
    auto processor_callback = [&](std::string_view msg) {
        server->broadcast(msg);
    };
    auto processor = std::make_shared<MessageProcessor>(processor_callback, debug_);

    std::vector<std::shared_ptr<WebSocketClient>> clients;
    int client_id = 0;
    for (const auto& url : okx_urls) {
        auto client_callback = [processor](const std::string& msg) {
            processor->process(msg);
        };
        clients.emplace_back(std::make_shared<WebSocketClient>(ioc, ctx, url, sub_message, client_callback, debug_, ++client_id));
    }

    // 4. 启动所有组件
    server->run();
    for (auto& client : clients) {
        client->run();
    }

    // 5. 设置信号处理，优雅地关闭
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){
        if (debug_) std::cout << "[Core] Signal received, shutting down." << std::endl;
        ioc.stop();
    });

    // 6. 启动线程池运行io_context
    std::vector<std::thread> thread_pool;
    thread_pool.reserve(threads);
    for(int i = 0; i < threads; ++i) {
        thread_pool.emplace_back([&ioc] {
            ioc.run();
        });
    }

    if (debug_) std::cout << "[Core] Repeater is running. Press Ctrl+C to exit." << std::endl;

    // 等待所有线程完成
    for(auto& t : thread_pool) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (debug_) std::cout << "[Core] Shutdown complete." << std::endl;
}

} // namespace repeater