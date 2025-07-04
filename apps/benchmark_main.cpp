#include "repeater/websocket_client.hpp"
#include "repeater/plain_websocket_client.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <numeric>

using high_res_clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<high_res_clock>;

// 时间戳
struct TimestampPair {
    time_point okx_time;
    time_point repeater_time;
};

class BenchmarkCore {
public:
    explicit BenchmarkCore(const nlohmann::json& config)
        : config_(config), debug_(config.value("debug", false)) {}

    void run() {
        auto const repeater_host = config_["repeater_server"]["host"].get<std::string>();
        auto const repeater_port = config_["repeater_server"]["port"].get<unsigned short>();
        auto const repeater_url = "ws://" + (repeater_host == "0.0.0.0" ? "127.0.0.1" : repeater_host) + ":" + std::to_string(repeater_port);
        auto const okx_url = config_["okx_connections"][0].get<std::string>();
        auto const sub_message = config_["subscription_message"].dump();

        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);

        // 创建两个客户端
        auto okx_client = std::make_shared<repeater::WebSocketClient>(ioc, ctx, okx_url, sub_message, 
            [this](const std::string& msg) { this->on_okx_message(msg); }, debug_, 1);
            
        auto repeater_client = std::make_shared<repeater::PlainWebSocketClient>(ioc, repeater_url, "{}",
            [this](const std::string& msg) { this->on_repeater_message(msg); }, debug_, 2);

        // 启动客户端
        okx_client->run();
        repeater_client->run();

        // 设置15秒后停止的定时器
        net::steady_timer stop_timer(ioc, std::chrono::seconds(15));
        stop_timer.async_wait([&](const beast::error_code&){
            std::cout << "\n[Benchmark] 15 seconds elapsed. Stopping..." << std::endl;
            ioc.stop();
        });
        
        std::cout << "[Benchmark] Starting benchmark for 15 seconds..." << std::endl;
        ioc.run();
        
        calculate_and_print_stats();
    }

private:
    void on_okx_message(const std::string& message) {
        auto now = high_res_clock::now();
        try {
            auto json_msg = nlohmann::json::parse(message);
            if (json_msg.contains("data") && !json_msg["data"].empty() && json_msg["data"][0].contains("seqId")) {
                int64_t seq_id = json_msg["data"][0]["seqId"].get<int64_t>();
                std::lock_guard<std::mutex> lock(results_mutex_);
                results_[seq_id].okx_time = now;
            }
        } catch (...) {}
    }

    void on_repeater_message(const std::string& message) {
        auto now = high_res_clock::now();
        try {
            auto json_msg = nlohmann::json::parse(message);
            if (json_msg.contains("data") && !json_msg["data"].empty() && json_msg["data"][0].contains("seqId")) {
                int64_t seq_id = json_msg["data"][0]["seqId"].get<int64_t>();
                std::lock_guard<std::mutex> lock(results_mutex_);
                results_[seq_id].repeater_time = now;
            }
        } catch (...) {}
    }

    void calculate_and_print_stats() {
        std::cout << "\n--- Benchmark Results ---" << std::endl;
        
        long long total_diff_ns = 0;
        int complete_pairs = 0;
        int repeater_faster_count = 0;
        int okx_faster_count = 0;

        for (const auto& [seq_id, times] : results_) {
            if (times.okx_time.time_since_epoch().count() != 0 &&
                times.repeater_time.time_since_epoch().count() != 0) {
                
                complete_pairs++;
                auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(times.repeater_time - times.okx_time).count();
                total_diff_ns += diff;

                if (diff < 0) {
                    repeater_faster_count++;
                } else {
                    okx_faster_count++;
                }
            }
        }

        if (complete_pairs == 0) {
            std::cout << "No matching seqId pairs received from both sources. Cannot calculate stats." << std::endl;
            std::cout << "Please ensure the repeater is running and correctly configured." << std::endl;
            return;
        }

        double avg_diff_ms = static_cast<double>(total_diff_ns) / complete_pairs / 1'000'000.0;

        std::cout << "Total matching seqId pairs: " << complete_pairs << std::endl;
        std::cout << "Repeater was faster: " << repeater_faster_count << " times." << std::endl;
        std::cout << "OKX direct feed was faster: " << okx_faster_count << " times." << std::endl;
        std::cout << "Average latency (repeater_time - okx_time): " << std::fixed << std::setprecision(4) << avg_diff_ms << " ms" << std::endl;
        std::cout << "(A negative value means the repeater is faster on average)" << std::endl;
    }

    nlohmann::json config_;
    bool debug_;
    std::map<int64_t, TimestampPair> results_;
    std::mutex results_mutex_;
};

int main() {
    try {
        std::ifstream config_file("../config/repeater_config.json");
        if (!config_file.is_open()) {
            std::cerr << "Error: Could not open config/repeater_config.json" << std::endl;
            return EXIT_FAILURE;
        }

        nlohmann::json config;
        config_file >> config;

        BenchmarkCore benchmark(config);
        benchmark.run();

    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}