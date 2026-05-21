#include "UMSGrpcServiceImpl.h"
#include "ConfigManager.h"
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <csignal>

int main() {
    ConfigManager& config = ConfigManager::getInstance();
    std::string host = config["UMSServer"]["host"];
    std::string port = config["UMSServer"]["port"];
    std::string addr = host + ":" + port;

    UMSGrpcServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "[UMS] UserService gRPC server listening on " << addr << std::endl;

    // Wait for shutdown signal
    std::signal(SIGINT, [](int) { exit(0); });
    std::signal(SIGTERM, [](int) { exit(0); });

    server->Wait();
    return 0;
}
