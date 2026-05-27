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

struct UserInfo {
	std::string username;
	std::string password;
	std::string email;
	int uid;
	int role;
	int belong_captain_id;
	int belong_team_id;
	int status;
};

enum class ErrorCodes {
    SUCCESS = 0,
    JSON_PARSE_ERROR = 1001,
    RPC_ERROR = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    USER_ALREADY_EXISTS = 1004,
    USER_DO_NOT_EXISTS = 1005,
    USER_LOGIN_ERROR = 1006,
    USER_ID_INVALID = 1007,
    USER_NOT_APPROVED = 1008,
    USER_ALREADY_APPROVED = 1009,
    AUTH_TOKEN_INVALID = 1010,
    AUTH_TOKEN_MISSING = 1011
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

#define MAX_LENGTH  2048
//头部总长度
#define HEAD_TOTAL_LEN 4
//头部id长度
#define HEAD_ID_LEN 2
//头部数据长度
#define HEAD_DATA_LEN 2
#define MAX_RECVQUE  10000
#define MAX_SENDQUE 1000

enum MSG_IDS {
	MSG_CHAT_LOGIN = 1005, //用户登陆
	MSG_CHAT_LOGIN_RSP = 1006, //用户登陆回包
};


#endif /* GLOBAL_H */