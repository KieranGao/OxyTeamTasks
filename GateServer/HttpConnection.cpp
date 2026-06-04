#include "HttpConnection.h"
#include "Logger.h"
#include "RedisManager.h"

HttpConnection::HttpConnection(boost::asio::io_context& ioc)
    : socket_(ioc) {}

void HttpConnection::start() {
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, req_, [self](beast::error_code ec, std::size_t
    byte_transferred) {
        try {
            if(ec) {
                LOG_ERROR("read error: {}", ec.message());
                return;
            }
            // 这里不需要使用byte_transferred变量，但编译器会警告未使用变量，所以用boost::ignore_unused来避免警告
            boost::ignore_unused(byte_transferred);
            // 否则就处理请求
            self->handleRequest_();
            self->checkDeadline_(); // 检查是否超时
        }
        catch (std::exception& exp) {
            LOG_ERROR("exception is {}", exp.what());
        }
    });
}
// 将一个字符转换为十六进制字符
unsigned char ToHex(unsigned char x) {
    return  x > 9 ? x + 55 : x + 48;
}
// 将一个十六进制字符转换为一个数字
unsigned char FromHex(unsigned char x) {
    unsigned char y;
    if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
    else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
    else if (x >= '0' && x <= '9') y = x - '0';
    else assert(0);
    return y;
}

std::string UrlEncode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //判断是否仅有数字和字母构成
        if (isalnum((unsigned char)str[i]) or
            (str[i] == '-') or
            (str[i] == '_') or
            (str[i] == '.') or
            (str[i] == '~'))
            strTemp += str[i];
        else if (str[i] == ' ') //为空字符
            strTemp += "+";
        else
        {
            //其他字符需要提前加%并且高四位和低四位分别转为16进制
            strTemp += '%';
            strTemp += ToHex((unsigned char)str[i] >> 4);
            strTemp += ToHex((unsigned char)str[i] & 0x0F);
        }
    }
    return strTemp;
}

std::string UrlDecode(const std::string& str)
{
    std::string strTemp = "";
    size_t length = str.length();
    for (size_t i = 0; i < length; i++)
    {
        //还原+为空
        if (str[i] == '+') strTemp += ' ';
        //遇到%将后面的两个字符从16进制转为char再拼接
        else if (str[i] == '%')
        {
            assert(i + 2 < length);
            unsigned char high = FromHex((unsigned char)str[++i]);
            unsigned char low = FromHex((unsigned char)str[++i]);
            strTemp += high * 16 + low;
        }
        else strTemp += str[i];
    }
    return strTemp;
}

void HttpConnection::PreParseGetParam() {
    // 提取 URI, 格式为 /path?key1=value1&key2=value2...
    std::string uri = req_.target().to_string();
    // 查找查询字符串的开始位置（即 '?' 的位置）  
    auto query_pos = uri.find('?');
    if (query_pos == std::string::npos) {
        get_url_ = uri;
        return;
    }

    get_url_ = uri.substr(0, query_pos);
    std::string query_string = uri.substr(query_pos + 1);
    std::string key;
    std::string value;
    size_t pos = 0;
    // 每找到一个 '&' 就将前面的部分作为一个 key=value 对进行处理，然后从查询字符串中删除已经处理的部分，直到没有 '&' 为止
    while ((pos = query_string.find('&')) != std::string::npos) {
        auto pair = query_string.substr(0, pos);
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码  
            value = UrlDecode(pair.substr(eq_pos + 1));
            get_params_[key] = value;
        } 
        query_string.erase(0, pos + 1);
    }
    // 处理最后一个参数对（如果没有 & 分隔符）  
    if (!query_string.empty()) {
        size_t eq_pos = query_string.find('=');
        if (eq_pos != std::string::npos) {
            key = UrlDecode(query_string.substr(0, eq_pos));
            value = UrlDecode(query_string.substr(eq_pos + 1));
            get_params_[key] = value;
        }
    }
}

bool HttpConnection::authenticateRequest_() {

    std::string target = req_.target().to_string();
    auto query_pos = target.find('?');
    std::string path = (query_pos != std::string::npos) ? target.substr(0, query_pos) : target;
    // 如果是公开端点，则不需要token验证
    if(PUBLIC_PATHS.count(path)) return true;

    auto write401 = [this](int error_code) {
        resp_.result(http::status::unauthorized);
        resp_.set(http::field::content_type, "application/json");
        Json::Value err;
        err["error"] = error_code;
        beast::ostream(resp_.body()) << err.toStyledString();
    };

    // 提取Authorization: Bearer <token>
    auto auth_it = req_.find(http::field::authorization);
    if (auth_it == req_.end()) {
        write401(static_cast<int>(ErrorCodes::AUTH_TOKEN_MISSING));
        return false;
    }
    std::string auth_value(auth_it->value().data(), auth_it->value().size());
    std::string token;
    const std::string bearer_prefix = "Bearer ";
    if (auth_value.compare(0, bearer_prefix.size(), bearer_prefix) == 0)
        token = auth_value.substr(bearer_prefix.size());
    else
        token = auth_value;
    if (token.empty()) {
        write401(static_cast<int>(ErrorCodes::AUTH_TOKEN_MISSING));
        return false;
    }

    // 提取X-User-Id
    auto uid_it = req_.find("X-User-Id");
    if (uid_it == req_.end()) {
        write401(static_cast<int>(ErrorCodes::AUTH_TOKEN_MISSING));
        return false;
    }
    int uid = 0;
    try {
        uid = std::stoi(std::string(uid_it->value().data(), uid_it->value().size()));
    } catch (...) {
        write401(static_cast<int>(ErrorCodes::AUTH_TOKEN_MISSING));
        return false;
    }

    // 通过redis验证token
    std::string token_key = USER_TOKEN_PREFIX + std::to_string(uid);
    std::string stored_token;
    if (!RedisManager::getInstance().get(token_key, stored_token) || stored_token != token) {
        write401(static_cast<int>(ErrorCodes::AUTH_TOKEN_INVALID));
        return false;
    }
    LOG_DEBUG("[GateServer] uid = {} passed token authorization for url {}",uid, path);
    return true;
}

void HttpConnection::handleRequest_() {
    resp_.version(req_.version());
    resp_.keep_alive(false); // 维持HTTP短连接
    // 对于每个HTTP请求，都先进行鉴权
    if (!authenticateRequest_()) {
        makeResponse_();
        return;
    }

    if(req_.method() == http::verb::get) {
        PreParseGetParam(); // 预处理GET请求的URL和参数
        bool ok = LogicSystem::getInstance().handleGet(get_url_, shared_from_this());
        if (!ok) {
            resp_.result(http::status::not_found);
            resp_.set(http::field::content_type, "text/plain");
            beast::ostream(resp_.body()) << "url not found\r\n";
            makeResponse_();
            return;
        }
        resp_.result(http::status::ok);
        resp_.set(http::field::server, "GateServer");
        makeResponse_();
        return;
    }
    if(req_.method() == http::verb::post) {
        bool ok = LogicSystem::getInstance().handlePost(req_.target().to_string(), shared_from_this());
        if(!ok) {
            resp_.result(http::status::not_found);
            resp_.set(http::field::content_type, "text/plain");
            beast::ostream(resp_.body()) << "url not found\r\n";
            makeResponse_();
            return;
        }
        resp_.result(http::status::ok);
        resp_.set(http::field::server, "GateServer");
        makeResponse_();
        return;
    }
}

void HttpConnection::makeResponse_() {
    auto self = shared_from_this();
    resp_.content_length(resp_.body().size()); // 设置Content-Length头
    http::async_write(socket_, resp_, [self](beast::error_code ec, std::size_t byte_transferred) {
        self->socket_.shutdown(tcp::socket::shutdown_send, ec); // 发送完响应后关闭连接
        self->deadline_.cancel(); // 取消定时器
    });
        
}

void HttpConnection::checkDeadline_() {
    auto self = shared_from_this();
    // 定时器，异步等待，如果超时了就关闭连接
    deadline_.async_wait([self](beast::error_code ec) {
        // 如果ec有值，说明定时器被取消了，说明连接已经处理完了，不需要关闭连接
        if (ec) {
            return; // 定时器被取消了，说明连接已经处理完了，不需要关闭连接
        }
        // 否则说明连接超时了，关闭连接
        self->socket_.close(ec);
    });
}
