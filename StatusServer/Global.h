#ifndef GLOBAL_H
#define GLOBAL_H

#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <mutex>
#include <iostream>
#include <json/json.h>

#define CODE_PREFIX "code_"
#define USER_TOKEN_PREFIX  "utoken_"

namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;   // from <boost/asio/ip/tcp.hpp>

enum class ErrorCodes {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    USER_ALREADY_EXISTS = 1004,
    USER_DO_NOT_EXISTS = 1005,
    USER_LOGIN_ERROR = 1006,
    INVALID_UID = 1007,
    USER_NOT_APPROVED = 1008,
    USER_ALREADY_APPROVED = 1009,
    INVALID_TOKEN = 1010
};

// class ConfigManager;
// extern ConfigManager g_configManager;


// Defer类，类似Go语言的Defer机制，在类对象生命周期结束时调用func回调
class Defer {
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() {
        func_();
    } 
private:
    std::function<void()> func_;
};

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

#endif /* GLOBAL_H */
