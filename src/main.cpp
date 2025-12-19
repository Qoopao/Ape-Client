#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include "webclient.h"

int main(int argc, char* argv[]) {
    // 初始化日志
    spdlog::set_level(spdlog::level::info);

    std::string host = "127.0.0.1";
    uint16_t port = 6666;

    try {
        // 创建IO上下文（核心：处理异步操作）
        boost::asio::io_context ioc;

        // 创建客户端实例
        WebClient client(ioc, host, port);

        // 启动协程运行客户端逻辑
        boost::asio::co_spawn(
            ioc,
            client.run(),
            [](std::exception_ptr e) {
                if (e) {
                    try {
                        std::rethrow_exception(e);
                    } catch (const std::exception& ex) {
                        spdlog::error("Coroutine error: {}", ex.what());
                    }
                }
            }
        );

        // 运行IO上下文（阻塞，直到所有异步操作完成）
        ioc.run();

    } catch (const std::exception& e) {
        spdlog::error("Main error: {}", e.what());
        return 1;
    }

    return 0;
}