#ifndef REPEATER_WEBSOCKET_CLIENT_HPP
#define REPEATER_WEBSOCKET_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <string>
#include <memory>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace repeater {

/**
 * 封装了连接到单个WebSocket服务器的所有逻辑。
 *
 * 负责解析URL、建立TCP连接、进行SSL握手、WebSocket握手、发送订阅消息，
 * 并异步循环读取消息。它还实现了简单的自动重连逻辑。
 */
class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
public:
    using OnMessageCallback = std::function<void(const std::string&)>;

    WebSocketClient(
        net::io_context& ioc,
        ssl::context& ctx,
        std::string url,
        std::string sub_msg,
        OnMessageCallback on_message_cb,
        bool debug,
        int id);

    void run();

private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void on_ssl_handshake(beast::error_code ec);
    void on_handshake(beast::error_code ec);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_close(beast::error_code ec);
    void fail(beast::error_code ec, char const* what);
    void reconnect();

    int id_;
    tcp::resolver resolver_;
    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    beast::flat_buffer buffer_;
    std::string url_str_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::string sub_msg_;
    OnMessageCallback on_message_cb_;
    bool debug_;
    net::steady_timer reconnect_timer_;
};

} // namespace repeater

#endif // REPEATER_WEBSOCKET_CLIENT_HPP