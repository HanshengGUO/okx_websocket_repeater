#ifndef REPEATER_WEBSOCKET_SERVER_HPP
#define REPEATER_WEBSOCKET_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_set>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace repeater {

class WebSocketSession;

/**
 * 接受外部客户端连接，并向所有连接的客户端广播消息。
 *
 * 管理所有活跃的WebSocket会话，并提供一个线程安全的广播接口。
 */
class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
public:
    WebSocketServer(net::io_context& ioc, tcp::endpoint endpoint, bool debug);

    void run();

    void broadcast(std::string_view message);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    void join(std::shared_ptr<WebSocketSession> session);
    void leave(std::shared_ptr<WebSocketSession> session);

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    bool debug_;
    
    std::mutex sessions_mutex_;
    std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_;
};

} // namespace repeater

#endif // REPEATER_WEBSOCKET_SERVER_HPP