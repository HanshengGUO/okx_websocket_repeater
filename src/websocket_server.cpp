#include "repeater/websocket_server.hpp"
#include <iostream>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;


namespace repeater {

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::vector<std::shared_ptr<const std::string>> queue_;
    std::function<void(std::shared_ptr<WebSocketSession>)> on_leave_;
    bool debug_;

public:
    WebSocketSession(tcp::socket&& socket, std::function<void(std::shared_ptr<WebSocketSession>)> on_leave, bool debug)
        : ws_(std::move(socket)), on_leave_(std::move(on_leave)), debug_(debug) {}

    ~WebSocketSession() {
        if (debug_) std::cout << "[Server Session] Destroyed." << std::endl;
    }

    void run() {
        net::dispatch(ws_.get_executor(),
            beast::bind_front_handler(&WebSocketSession::on_run, shared_from_this()));
    }

    void on_run() {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
            }));
        ws_.async_accept(beast::bind_front_handler(&WebSocketSession::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec) {
            if (debug_) std::cerr << "[Server Session] Accept error: " << ec.message() << std::endl;
            return;
        }
        do_read();
    }

    void do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&WebSocketSession::on_read, shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            on_leave_(shared_from_this());
            return;
        }
        if (ec) {
            if (debug_) std::cerr << "[Server Session] Read error: " << ec.message() << std::endl;
            on_leave_(shared_from_this());
            return;
        }
        buffer_.consume(buffer_.size());
        do_read();
    }

    void send(std::shared_ptr<const std::string> const& ss) {
        net::post(ws_.get_executor(),
            beast::bind_front_handler(&WebSocketSession::on_send, shared_from_this(), ss));
    }

private:
    void on_send(std::shared_ptr<const std::string> const& ss) {
        queue_.push_back(ss);
        if (queue_.size() > 1) return;
        do_write();
    }

    void do_write() {
        ws_.async_write(net::buffer(*queue_.front()),
            beast::bind_front_handler(&WebSocketSession::on_write, shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            if (debug_) std::cerr << "[Server Session] Write error: " << ec.message() << std::endl;
            on_leave_(shared_from_this());
            return;
        }
        queue_.erase(queue_.begin());
        if (!queue_.empty()) {
            do_write();
        }
    }
};

WebSocketServer::WebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, bool debug)
    : ioc_(ioc), acceptor_(ioc), debug_(debug) {
    beast::error_code ec;
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        std::cerr << "[Server] Open error: " << ec.message() << std::endl;
        return;
    }
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        std::cerr << "[Server] Set option error: " << ec.message() << std::endl;
        return;
    }
    acceptor_.bind(endpoint, ec);
    if (ec) {
        std::cerr << "[Server] Bind error: " << ec.message() << std::endl;
        return;
    }
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        std::cerr << "[Server] Listen error: " << ec.message() << std::endl;
        return;
    }
}

void WebSocketServer::run() {
    if (debug_) std::cout << "[Server] Started listening on " << acceptor_.local_endpoint() << std::endl;
    do_accept();
}

void WebSocketServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&WebSocketServer::on_accept, shared_from_this()));
}

void WebSocketServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        std::cerr << "[Server] Accept error: " << ec.message() << std::endl;
    } else {
        auto on_leave_cb = [this](std::shared_ptr<WebSocketSession> session) {
            this->leave(session);
        };
        auto session = std::make_shared<WebSocketSession>(std::move(socket), on_leave_cb, debug_);
        join(session);
        session->run();
    }
    do_accept();
}

void WebSocketServer::join(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.insert(session);
    if (debug_) std::cout << "[Server] Client joined. Total clients: " << sessions_.size() << std::endl;
}

void WebSocketServer::leave(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session);
    if (debug_) std::cout << "[Server] Client left. Total clients: " << sessions_.size() << std::endl;
}

void WebSocketServer::broadcast(std::string_view message) {
    auto const shared_msg = std::make_shared<const std::string>(message);
    
    std::vector<std::weak_ptr<WebSocketSession>> v;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        v.reserve(sessions_.size());
        for (auto session : sessions_) {
            v.emplace_back(session);
        }
    }

    for (auto const& weak_session : v) {
        if (auto session = weak_session.lock()) {
            session->send(shared_msg);
        }
    }
}

} // namespace repeater