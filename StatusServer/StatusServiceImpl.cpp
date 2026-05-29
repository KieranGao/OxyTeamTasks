#include "StatusServiceImpl.h"
#include "ConfigManager.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "RedisManager.h"
#include <string>
#include <vector>
#include <json/json.h>
#include "Logger.h"

std::string generate_unique_string()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string unique_string = boost::uuids::to_string(uuid);
    return unique_string;
}

StatusServiceImpl::StatusServiceImpl()
{
    auto &g_config = ConfigManager::getInstance();
    auto server_list = g_config["PushServers"]["name"];
    allocate_method_ = g_config["AllocateMethods"]["methods"];
    std::vector<std::string> names;
    std::stringstream ss(server_list);
    std::string name;
    while(std::getline(ss,name,',')) names.push_back(name);
    server_cnt_ = names.size();
    if(server_cnt_ == 0) {
        LOG_ERROR("[StatusServer] FATAL: No PushServers configured in [PushServers] section!");
        std::abort();
    }
    if(allocate_method_ == "SegmentTree")
        SegTree_ = std::make_unique<SegmentTree>(std::vector<int>(server_cnt_ + 1));
    else {
        allocate_method_ = "Brute";
        server_conns_.resize(server_cnt_ + 1, 0);
    }
    int idx = 1;
    for(auto &name : names)
    {
        if(g_config[name]["name"].empty()) continue;
        PushServer server;
        server.host = g_config[name]["host"];
        server.port = g_config[name]["port"];
        server.name = g_config[name]["name"];
        server.id = idx;
        // 以 host:port 作为唯一标识
        std::string key = server.host + ":" + server.port;
        servers_[key] = server;
        servers_idx_[idx++] = server;
    }
}

PushServer& StatusServiceImpl::getPushServer() {
    std::lock_guard<std::mutex> lock(server_mtx_);
    assert(!servers_.empty());
    if(allocate_method_ == "Brute") {
        int minCon = INT_MAX;
        int minIdx = 1;
        for(int i=1;i<=server_cnt_;i++) {
            if(minCon >= server_conns_[i]) {
                minCon = server_conns_[i];
                minIdx = i;
            }
        }
        server_conns_[minIdx]++;
        return servers_idx_[minIdx];
    } else {
        int minIdx = SegTree_->queryMinidx(1,server_cnt_);
        int minCon = SegTree_->getVal(minIdx);
        SegTree_->updateVal(minIdx, minCon + 1);
        return servers_idx_[minIdx];
    }
}

// Caller must hold server_mtx_
void StatusServiceImpl::returnServer(PushServer& cs) {
    if(allocate_method_ == "Brute") {
        if(cs.id > 0 and cs.id <= server_cnt_) server_conns_[cs.id]--;
    } else {
        SegTree_->updateVal(cs.id, SegTree_->getVal(cs.id) - 1);
    }
}

Status StatusServiceImpl::AllocatePushServer(ServerContext* context, const AllocateReq* req, AllocateRsp* resp)
{
    LOG_INFO("[StatusServer] AllocatePushServer uid={}", req->uid());
    PushServer& server = getPushServer();
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_host(server.host);
    resp->set_port(server.port);
    resp->set_token(generate_unique_string());
    if (!insertToken(req->uid(), resp->token())) {
        LOG_ERROR("[StatusServer] AllocatePushServer: Failed to persist token for uid={}, Redis may be down", req->uid());
    }
    LOG_INFO("[StatusServer] Allocated {}:{} token={} for uid={}", server.host, server.port, resp->token(), req->uid());
    return Status::OK;
}

Status StatusServiceImpl::ReportLogin(ServerContext* context, const LoginReportReq* request, LoginReportRsp* resp)
{
    auto uid = request->uid();
    auto token = request->token();

    std::string uid_str = std::to_string(uid);
    std::string token_key = USER_TOKEN_PREFIX + uid_str;
    std::string token_value = "";

    bool success = RedisManager::getInstance().get(token_key, token_value);
    if(!success) {
        resp->set_error(static_cast<int>(ErrorCodes::INVALID_UID));
        return Status::OK;
    }

    if(token_value != token) {
        resp->set_error(static_cast<int>(ErrorCodes::INVALID_TOKEN));
        return Status::OK;
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 断线上报 ----
// PushServer 在用户断开 WebSocket 时调用，递减该节点的连接计数
Status StatusServiceImpl::ReportDisconnect(ServerContext* context, const DisconnectReq* req, DisconnectRsp* resp) {
    std::lock_guard<std::mutex> lock(server_mtx_);
    LOG_INFO("[StatusServer] ReportDisconnect server={}", req->server_name());
    auto it = servers_.find(req->server_name());
    if (it != servers_.end()) {
        returnServer(it->second);
        resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
        LOG_INFO("[StatusServer] ReportDisconnect: decremented conn count for {}", req->server_name());
    } else {
        LOG_ERROR("[StatusServer] ReportDisconnect: unknown server={}", req->server_name());
        resp->set_error(static_cast<int>(ErrorCodes::INVALID_UID));
    }
    return Status::OK;
}

bool StatusServiceImpl::insertToken(int uid, std::string token) {
    std::string uid_str = std::to_string(uid);
    std::string token_key = USER_TOKEN_PREFIX + uid_str;
    bool ok = RedisManager::getInstance().setex(token_key, token, 604800);
    LOG_DEBUG("[StatusServer] Token {} inserted for uid = {}!",token_key, uid_str);
    if (!ok) {
        LOG_ERROR("[StatusServer] insertToken FAILED for uid={}: Redis setex returned false", uid);
    }
    return ok;
}


// ---- 日志上报 ----
// 日志由各服务 Logger 后台线程每 5s 批量 gRPC 上报至此
// INFO+ 级别 → LPUSH Redis List + LTRIM 500 + EXPIRE 7天
Status StatusServiceImpl::ReportLog(ServerContext* context, const ReportLogReq* req, ReportLogRsp* resp) {
    for (int i = 0; i < req->entries_size(); ++i) {
        const auto& entry = req->entries(i);
        if (entry.level() == "DEBUG") continue;  // DEBUG 只写本地文件，不推远程

        std::string redis_key = "logs:" + entry.service();
        std::string json = "{\"service\":\"" + entry.service() + "\",\"level\":\"" + entry.level()
            + "\",\"message\":\"" + jsonEscape(entry.message()) + "\",\"timestamp\":" + std::to_string(entry.timestamp()) + "}";

        auto& redis = RedisManager::getInstance();
        redis.lpush(redis_key, json);
        redis.ltrim(redis_key, 0, 499);
        redis.expire(redis_key, 604800);
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 日志查询 ----
// 教练在客户端查询时 → GateServer HTTP → 此 gRPC → Redis LRANGE
Status StatusServiceImpl::QueryLogs(ServerContext* context, const QueryLogsReq* req, QueryLogsRsp* resp) {
    int limit = req->limit();
    if (limit <= 0 || limit > 500) limit = 500;

    // 动态发现所有 logs:* key（不再硬编码）
    std::vector<std::string> keys;
    auto& redis = RedisManager::getInstance();
    std::string all_keys_json = redis.keys("logs:*");
    
    Json::Reader reader;
    Json::Value key_array;
    if (reader.parse(all_keys_json, key_array) && key_array.isArray()) {
        for (auto& k : key_array)
            keys.push_back(k.asString());
    }
    

    // 如果指定了 service，只留下匹配的 key（前缀匹配）
    if (!req->service().empty()) {
        std::vector<std::string> filtered;
        for (auto& k : keys) {
            // key 格式: "logs:GateServer" 或 "logs:PushServer(127.0.0.1:8890)"
            std::string svc_part = k.substr(5);  // 去掉 "logs:" 前缀
            if (svc_part.compare(0, req->service().size(), req->service()) == 0)
                filtered.push_back(k);
        }
        keys = std::move(filtered);
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    int added = 0;

    const int PER_SERVICE = 100;

    for (auto& key : keys) {
        if (added >= limit) break;
        int fetch = std::min(PER_SERVICE, limit - added);
        auto elements = redis.lrangeVec(key, 0, fetch - 1);
        for (auto& elem : elements) {
            if (added >= limit) break;
            Json::Value obj;
            if (!reader.parse(elem, obj)) continue;  // 跳过损坏的旧条目
            std::string svc = obj.get("service", "").asString();
            std::string lvl = obj.get("level", "").asString();
            if (!req->level().empty() && lvl != req->level()) continue;
            auto* entry = resp->add_entries();
            entry->set_service(svc);
            entry->set_level(lvl);
            entry->set_message(obj.get("message", "").asString());
            entry->set_timestamp(obj.get("timestamp", 0).asInt64());
            added++;
        }
    }
    return Status::OK;
}

// ---- 心跳 ----
// 以 host:port 为唯一 key，同一服务的多个实例（如 PushServer 集群）各自独立
Status StatusServiceImpl::ServerHeartbeat(ServerContext* context, const HeartbeatReq* req, HeartbeatRsp* resp) {
    std::string key = req->host() + ":" + req->port();
    std::lock_guard<std::mutex> lock(server_status_mtx_);
    auto& info = server_status_[key];
    info.service = req->service();
    info.host = req->host();
    info.port = req->port();
    info.last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.status = "online";
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 服务状态查询 ----
Status StatusServiceImpl::QueryServerStatus(ServerContext* context, const QueryServerStatusReq* req, QueryServerStatusRsp* resp) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::pair<std::string, ServerHeartbeatInfo>> servers;
    {
        std::lock_guard<std::mutex> lock(server_status_mtx_);
        for (auto& [key, info] : server_status_) {
            if (info.last_heartbeat > 0 && (now - info.last_heartbeat) > heartbeat_timeout_secs_)
                info.status = "offline";
            servers.push_back({key, info});
        }
    }

    for (auto& [key, info] : servers) {
        auto* srv = resp->add_servers();
        srv->set_service(info.service);
        srv->set_host(info.host);
        srv->set_port(info.port);
        srv->set_status(info.status);
        srv->set_last_heartbeat(info.last_heartbeat);

        // PushServer 节点附加当前连接数（以 host:port 匹配负载均衡器数据）
        int conns = 0;
        {
            std::lock_guard<std::mutex> lock(server_mtx_);
            std::string lb_key = info.host + ":" + info.port;
            auto it = servers_.find(lb_key);
            if (it != servers_.end()) {
                int id = it->second.id;
                if (allocate_method_ == "Brute") {
                    if (id > 0 && id <= server_cnt_) conns = server_conns_[id];
                } else {
                    conns = SegTree_->getVal(id);
                }
            }
        }
        srv->set_connections(conns);
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}
