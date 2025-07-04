#include "repeater/plain_websocket_client.hpp"
#include <iostream>
#include <string_view>

namespace repeater {

PlainWebSocketClient::PlainWebSocketClient(
    net::io_context& ioc,
    std::string url,
    std::string sub_msg,
    OnMessageCallback on_message_cb,
    bool debug,
    int id)
    : id_(id),
      resolver_(net::make_strand(ioc)),
      ws_(net::make_strand(ioc)),
      url_str_(std::move(url)),
      sub_msg_(std::move(sub_msg)),
      on_message_cb_(std::move(on_message_cb)),
      debug_(debug),
      reconnect_timer_(ioc)
{
    if (debug_) {
        std::cout << "[Plain Client " << id_ << "] Created for URL: " << url_str_ << std::endl;
    }
}

void PlainWebSocketClient::run() {
    std::string_view url_sv(url_str_);
    std::string port = "80"; // Default for ws://

    if (url_sv.rfind("ws://", 0) == 0) {
        url_sv.remove_prefix(5);
    } else {
        std::cerr << "[Plain Client " << id_ << "] Invalid WebSocket URL scheme, expected ws:// in: " << url_str_ << std::endl;
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
        port = std::string(authority.substr(port_pos + 1));
    } else {
        host_ = std::string(authority);
    }

    if (host_.empty()) {
        std::cerr << "[Plain Client " << id_ << "] Could not extract host from URL: " << url_str_ << std::endl;
        return;
    }

    resolver_.async_resolve(
        host_,
        port,
        beast::bind_front_handler(&PlainWebSocketClient::on_resolve, shared_from_this()));
}

void PlainWebSocketClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) return fail(ec, "resolve");

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    beast::get_lowest_layer(ws_).async_connect(
        results,
        beast::bind_front_handler(&PlainWebSocketClient::on_connect, shared_from_this()));
}

void PlainWebSocketClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    if (ec) return fail(ec, "connect");

    beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::request_type& req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-plain");
        }));

    ws_.async_handshake(host_, path_,
        beast::bind_front_handler(&PlainWebSocketClient::on_handshake, shared_from_this()));
}

void PlainWebSocketClient::on_handshake(beast::error_code ec) {
    if (ec) return fail(ec, "handshake");

    if (debug_) std::cout << "[Plain Client " << id_ << "] Connected. Sending subscription." << std::endl;
    
    ws_.async_write(
        net::buffer(sub_msg_),
        beast::bind_front_handler(&PlainWebSocketClient::on_write, shared_from_this()));
}

void PlainWebSocketClient::on_write(beast::error_code ec, std::size_t) {
    if (ec) return fail(ec, "write");

    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&PlainWebSocketClient::on_read, shared_from_this()));
}

void PlainWebSocketClient::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if (ec) return fail(ec, "read");

    std::string message = beast::buffers_to_string(buffer_.data());
    on_message_cb_(message);
    
    buffer_.consume(buffer_.size());
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&PlainWebSocketClient::on_read, shared_from_this()));
}

void PlainWebSocketClient::fail(beast::error_code ec, char const* what) {
    if (ec == net::error::operation_aborted || ec == websocket::error::closed)
        return;

    std::cerr << "[Plain Client " << id_ << "] Error in " << what << ": " << ec.message() << std::endl;
    reconnect();
}

void PlainWebSocketClient::reconnect() {
    if (debug_) std::cout << "[Plain Client " << id_ << "] Attempting to reconnect in 5 seconds..." << std::endl;
    reconnect_timer_.expires_after(std::chrono::seconds(5));
    reconnect_timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
        if (ec) return;
        self->run();
    });
}

} // namespace repeater