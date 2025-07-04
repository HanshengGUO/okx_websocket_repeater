#ifndef REPEATER_PLAIN_WEBSOCKET_CLIENT_HPP
#define REPEATER_PLAIN_WEBSOCKET_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <string>
#include <memory>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace repeater {

/**
 * 封装了连接到单个非安全(ws://)WebSocket服务器的逻辑。
 *
 * 这是WebSocketClient的简化版本，专为非SSL连接设计。
 */
class PlainWebSocketClient : public std::enable_shared_from_this<PlainWebSocketClient> {
public:
    using OnMessageCallback = std::function<void(const std::string&)>;

    PlainWebSocketClient(
        net::io_context& ioc,
        std::string url,
        std::string sub_msg,
        OnMessageCallback on_message_cb,
        bool debug,
        int id);

    void run();

private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void on_handshake(beast::error_code ec);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void fail(beast::error_code ec, char const* what);
    void reconnect();

    int id_;
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string url_str_;
    std::string host_;
    std::string path_;
    std::string sub_msg_;
    OnMessageCallback on_message_cb_;
    bool debug_;
    net::steady_timer reconnect_timer_;
};

} // namespace repeater

#endif // REPEATER_PLAIN_WEBSOCKET_CLIENT_HPP