#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>


class WebClient
{
public:
    WebClient(boost::asio::io_context& ioc, const std::string host, uint16_t port);

    //启动客户端并连接服务器
    boost::asio::awaitable<void> run();

    //向服务器发送消息
    boost::asio::awaitable<void> send_message(const nlohmann::json& msg);

    //读取服务器返回的消息
    boost::asio::awaitable<void> read_message();

    //处理消息
    void process_message(const std::string& message);

private:
    boost::asio::io_context& ioc_;
    std::string host_;
    uint16_t port_;
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
};
