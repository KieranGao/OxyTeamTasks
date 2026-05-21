#include "Global.h"
#include "MainServer.h"
#include "ConfigManager.h"
#include <hiredis/hiredis.h>
#include "assert.h"
#include "RedisManager.h"

int main() {
    ConfigManager& configManager = ConfigManager::getInstance();
    std::string gate_port_str = configManager["GateServer"]["port"];
    std::string verify_port_str = configManager["MailerServer"]["port"];
    unsigned short gate_port = std::atoi(gate_port_str.c_str());
    unsigned short verify_port = std::atoi(verify_port_str.c_str());
    // TestRedisManager();
    try
    {
        unsigned short port = static_cast<unsigned short>(gate_port);
        net::io_context ioc{ 1 };
        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& error, int signal_number) {
            if (error) 
                return;
            ioc.stop();
            });
        std::make_shared<MainServer>(ioc, port)->start();
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}