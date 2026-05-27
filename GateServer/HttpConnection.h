#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include "Global.h"
#include "LogicSystem.h"
#include <unordered_set>
class HttpConnection : public std::enable_shared_from_this<HttpConnection> 
{
public:
    friend class LogicSystem; // 让LogicSystem类可以访问HttpConnection的私有成员函数和变量
    HttpConnection(boost::asio::io_context& ioc);
    void start();
    tcp::socket& socket() { return socket_; }
private:
    void checkDeadline_();
    void handleRequest_();
    void makeResponse_();
    void PreParseGetParam();
    bool authenticateRequest_();
    tcp::socket socket_;
    // 用于存储从socket读取的数据
    beast::flat_buffer buffer_ {8192}; // 8KB
    http::request<http::dynamic_body> req_; // 用于存储解析后的HTTP请求
    http::response<http::dynamic_body> resp_; // 用于存储要发送的HTTP响应
    net::steady_timer deadline_ { // 用于管理连接的超时
        socket_.get_executor(), // socket_的io上下文
        std::chrono::seconds(60) // 60秒超时
    };
    // GET请求的URL和参数
    std::string get_url_;
    std::unordered_map<std::string, std::string> get_params_;
    // 公开端点，用户访问时无需TOKEN验证，否则均需要
    // （C++17 语法糖）inline static 的作用就是：允许你在头文件里直接初始化静态成员变量，并且保证整个程序里只有一份实例，不会多重定义报错。
    inline static const std::unordered_set<std::string> PUBLIC_PATHS = {
        "/get_verify_code",
        "/user_register",
        "/user_resetpass",
        "/user_login",
        "/get_test"
    };
};
#endif /* HTTPCONNECTION_H */
