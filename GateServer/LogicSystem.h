#ifndef LOGICSYSTEM_H
#define LOGICSYSTEM_H

#include "Global.h"
#include "Singleton.h"
#include <unordered_map>
#include <functional>
#include <string>

// 前置声明HttpConnection类，避免循环依赖
class HttpConnection;

// 定义一个函数类型，用于处理HTTP请求，参数是一个指向HttpConnection对象的shared_ptr
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

class LogicSystem : public Singleton<LogicSystem>
{
public:
    friend class Singleton<LogicSystem>; // 让Singleton<LogicSystem>类可以访问LogicSystem的私有成员函数和变量
    ~LogicSystem() = default;
    bool handleGet(std::string url, std::shared_ptr<HttpConnection> connection);
    void registerGet(std::string url, HttpHandler handler);
    bool handlePost(std::string url, std::shared_ptr<HttpConnection> connection);
    void registerPost(std::string url, HttpHandler handler);
private:
    LogicSystem();
    std::unordered_map<std::string, HttpHandler> getHandlers_; // 存储GET请求的URL和对应的处理函数
    std::unordered_map<std::string, HttpHandler> postHandlers_; // 存储POST请求的URL和对应的处理函数
};
#endif /* LOGICSYSTEM_H */
