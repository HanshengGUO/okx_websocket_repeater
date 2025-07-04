#include "repeater/repeater_core.hpp"
#include "nlohmann/json.hpp"
#include <iostream>
#include <fstream>
#include <string>

int main() {
    try {
        // 加载配置文件
        std::ifstream config_file("../config/repeater_config.json");
        if (!config_file.is_open()) {
            std::cerr << "Error: Could not open config/repeater_config.json" << std::endl;
            return EXIT_FAILURE;
        }

        nlohmann::json config;
        config_file >> config;

        repeater::RepeaterCore core(config);
        core.run();

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON configuration error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}