#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <iostream>

#include "webclient.h"


WebClient::WebClient(boost::asio::io_context &ioc, const std::string host, uint16_t port)
    : ioc_(ioc), host_(host), port_(port), ws_(boost::asio::ip::tcp::socket(ioc))
{
    ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
    spdlog::info("Initialized WebClient");
}

boost::asio::awaitable<void> WebClient::run()
{
    try
    {
        // 解析服务器地址
        boost::asio::ip::tcp::resolver resolver(ioc_);
        auto endpoint = co_await resolver.async_resolve(host_, std::to_string(port_), boost::asio::use_awaitable);

        // 连接服务器
        co_await boost::asio::async_connect(ws_.next_layer(), endpoint, boost::asio::use_awaitable);
        spdlog::info("Connect to server {}:{}", host_, port_);

        // 升级连接
        co_await ws_.async_handshake(host_, "/", boost::asio::use_awaitable);
        spdlog::info("Upgraded to WebSocket");

        // 设置ping/pong处理逻辑
        ws_.control_callback(
            [](boost::beast::websocket::frame_type kind, boost::beast::string_view payload)
            {
                if (kind == boost::beast::websocket::frame_type::pong)
                {
                    spdlog::info("Receive pong frame: {}", payload);
                }

                boost::ignore_unused(kind, payload);
            });

        // 持续读取消息
        boost::asio::co_spawn(ws_.get_executor(), read_message(), boost::asio::detached);

        boost::asio::steady_timer timer(ioc_, std::chrono::seconds(5));
        // 添加客户端的主动消息发送逻辑（循环发送ping消息）
        while (true)
        {
            // 发送auth消息
            static bool sent_auth = false;
            if (!sent_auth)
            {
                nlohmann::json auth_msg = {
                    {"type", "auth"},
                    {"userid", "1001"},
                    {"timestamp", std::time(nullptr)}};
                co_await send_message(auth_msg);
                sent_auth = true;
            }

            // 等待定时器触发
            co_await timer.async_wait(boost::asio::use_awaitable);

            boost::beast::websocket::ping_data payload{"heartbeat"};
            co_await ws_.async_ping(payload, boost::asio::use_awaitable);

            // 重置定时器：下次5秒后触发
            timer.expires_after(std::chrono::seconds(5));

            // 发送一个chat消息（仅发送一次，可注释掉）
            static bool sent_chat = false;
            if (!sent_chat)
            {
                nlohmann::json chat_msg = {
                    {"type", "chat"},
                    {"from", "client"},
                    {"content", "Hello, WebSocket Server!"},
                    {"timestamp", std::time(nullptr)}};
                co_await send_message(chat_msg);
                sent_chat = true;
            }
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Client error: {}", e.what());
        // 发生错误时关闭WebSocket连接
        boost::beast::error_code ec;
        ws_.close(boost::beast::websocket::close_code::normal, ec);
        if (ec)
        {
            spdlog::warn("Close error: {}", ec.message());
        }
    }
}

boost::asio::awaitable<void> WebClient::send_message(const nlohmann::json &msg)
{
    try
    {
        std::string msg_str = msg.dump();
        // 异步发送消息
        co_await ws_.async_write(boost::asio::buffer(msg_str), boost::asio::use_awaitable);
        spdlog::info("Sent message: {}", msg_str);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Send message error: {}", e.what());
        throw; // 重新抛出异常，让上层处理
    }
}

boost::asio::awaitable<void> WebClient::read_message()
{
    boost::beast::flat_buffer buffer; // 消息缓冲区
    try
    {
        while (true)
        {
            // 异步读取服务器发送的消息
            std::size_t n = co_await ws_.async_read(buffer, boost::asio::use_awaitable);
            std::string message = boost::beast::buffers_to_string(buffer.data());
            spdlog::info("Received message from server: {}", message);

            // 处理消息
            process_message(message);

            // 清空缓冲区，准备下一次读取
            buffer.consume(n);
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Read message error: {}", e.what());
        // 读取失败时关闭连接
        boost::beast::error_code ec;
        ws_.close(boost::beast::websocket::close_code::normal, ec);
        if (ec)
        {
            spdlog::warn("Close error in do_read: {}", ec.message());
        }
    }
}

void WebClient::process_message(const std::string &message)
{
    try
    {
        // 解析JSON消息
        nlohmann::json msg_json = nlohmann::json::parse(message);
        std::string type = msg_json["type"];

        // 处理不同类型的消息
        if (type == "pong")
        {
            spdlog::info("Received pong response, server timestamp: {}", std::time_t(msg_json["timestamp"]));
        }
        else if (type == "error")
        {
            spdlog::error("Received error from server: {}", std::time_t(msg_json["message"]));
        }
        else
        {
            spdlog::info("Received unknown message type: {}", type);
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        spdlog::error("JSON parse error: {}", e.what());
    }
}