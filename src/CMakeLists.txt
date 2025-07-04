add_library(repeater_lib STATIC
    websocket_client.cpp
    plain_websocket_client.cpp
    websocket_server.cpp
    message_processor.cpp
    repeater_core.cpp
)

target_link_libraries(repeater_lib PUBLIC
    # Boost
    Boost::system
    Boost::thread

    # OpenSSL
    OpenSSL::SSL
    OpenSSL::Crypto

    # nlohmann_json
    nlohmann_json::nlohmann_json
)

target_include_directories(repeater_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)