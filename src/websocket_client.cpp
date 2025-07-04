#include "repeater/websocket_client.hpp"
#include <iostream>
#include <string_view>

namespace repeater {

WebSocketClient::WebSocketClient(
    net::io_context& ioc,
    ssl::context& ctx,
    std::string url,
    std::string sub_msg,
    OnMessageCallback on_message_cb,
    bool debug,
    int id)
    : id_(id),
      resolver_(net::make_strand(ioc)),
      ws_(net::make_strand(ioc), ctx),
      url_str_(std::move(url)),
      sub_msg_(std::move(sub_msg)),
      on_message_cb_(std::move(on_message_cb)),
      debug_(debug),
      reconnect_timer_(ioc)
{
    if (debug_) {
        std::cout << "[Client " << id_ << "] Created for URL: " << url_str_ << std::endl;
    }
}

void WebSocketClient::run() {
    // --- 手动URL解析逻辑 ---
    std::string_view url_sv(url_str_);

    // 1. 确定协议并设置默认端口
    if (url_sv.rfind("wss://", 0) == 0) { // rfind with pos=0 is equivalent to starts_with
        port_ = "443";
        url_sv.remove_prefix(6); // 移除 "wss://"
    } else if (url_sv.rfind("ws://", 0) == 0) {
        port_ = "80";
        url_sv.remove_prefix(5); // 移除 "ws://"
    } else {
        std::cerr << "[Client " << id_ << "] Invalid WebSocket URL scheme in: " << url_str_ << std::endl;
        return;
    }

    auto path_pos = url_sv.find('/');
    std::string_view authority;
    if (path_pos == std::string_view::npos) {
        authority = url_sv;
        path_ = "/";
    } else {
        authority = url_sv.substr(0, path_pos);
        path_ = std::string(url_sv.substr(path_pos));
    }

    auto port_pos = authority.find(':');
    if (port_pos != std::string_view::npos) {
        host_ = std::string(authority.substr(0, port_pos));
        port_ = std::string(authority.substr(port_pos + 1));
    } else {
        host_ = std::string(authority);
        // port_ 已经设置为默认值
    }

    if (host_.empty()) {
        std::cerr << "[Client " << id_ << "] Could not extract host from URL: " << url_str_ << std::endl;
        return;
    }

    // 设置SNI主机名，这对于SSL非常重要
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host_.c_str())) {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::cerr << "[Client " << id_ << "] SSL_set_tlsext_host_name failed: " << ec.message() << std::endl;
        return;
    }
    
    resolver_.async_resolve(
        host_,
        port_,
        beast::bind_front_handler(&WebSocketClient::on_resolve, shared_from_this()));
}

void WebSocketClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) return fail(ec, "resolve");

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&WebSocketClient::on_connect, shared_from_this()));
}

void WebSocketClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    if (ec) return fail(ec, "connect");

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&WebSocketClient::on_ssl_handshake, shared_from_this()));
}

void WebSocketClient::on_ssl_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "ssl_handshake");

    beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
        }));

    ws_.async_handshake(host_, path_,
        beast::bind_front_handler(&WebSocketClient::on_handshake, shared_from_this()));
}

void WebSocketClient::on_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "handshake");

    if (debug_) std::cout << "[Client " << id_ << "] Connected. Sending subscription." << std::endl;
    
    ws_.async_write(
        net::buffer(sub_msg_),
        beast::bind_front_handler(&WebSocketClient::on_write, shared_from_this()));
}

void WebSocketClient::on_write(beast::error_code ec, std::size_t) {
    if (ec) return fail(ec, "write");

    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&WebSocketClient::on_read, shared_from_this()));
}

void WebSocketClient::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if (ec) return fail(ec, "read");

    std::string message = beast::buffers_to_string(buffer_.data());
    on_message_cb_(message);
    
    buffer_.consume(buffer_.size());
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&WebSocketClient::on_read, shared_from_this()));
}

void WebSocketClient::on_close(beast::error_code ec) {
    if (ec) return fail(ec, "close");
    if (debug_) std::cout << "[Client " << id_ << "] Connection closed cleanly." << std::endl;
}

void WebSocketClient::fail(beast::error_code ec, char const* what) {
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;

    std::cerr << "[Client " << id_ << "] Error in " << what << ": " << ec.message() << std::endl;
    reconnect();
}

void WebSocketClient::reconnect() {
    if (debug_) std::cout << "[Client " << id_ << "] Attempting to reconnect in 5 seconds..." << std::endl;
    reconnect_timer_.expires_after(std::chrono::seconds(5));
    reconnect_timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
        if (ec) {
            return;
        }
        self->run();
    });
}
} // namespace repeater