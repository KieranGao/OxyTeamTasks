#include "LogicSystem.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "IOContextPool.h"
#include "ConfigManager.h"
#include "MainServer.h"
#include <atomic>
#include <condition_variable>

std::atomic<bool> is_running{false};
std::condition_variable cv;
std::mutex quit_mtx;

int main() {
    try {
        auto& g_config = ConfigManager::getInstance();
        auto& pool = IOContextPool::getInstance();
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool](){
            io_context.stop();
            pool.stop();
        }); 
        auto port = g_config["SelfServer"]["port"];
        MainServer server(io_context, atoi(port.c_str()));
        io_context.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << '\n';
    }
    
}