#include "Singleton.h"
#include <queue>
#include <thread>
#include "Session.h"
#include <queue>
#include <map>
#include <functional>
#include "Global.h"

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include <unordered_map>
#include <atomic>

typedef function<void(std::shared_ptr<Session>, const short &msg_id, const string &msg_data)> FunCallBack;
class LogicSystem : public Singleton<LogicSystem> {
	friend class Singleton<LogicSystem>;
public:
	~LogicSystem();
	void postMsgToQue(std::shared_ptr<LogicNode> msg);
private:
	LogicSystem();
	void dealMsg();
	void registerCallBacks();
	void loginHandler(std::shared_ptr<Session>, const short &msg_id, const string &msg_data);
	std::thread workers_thread_;
	std::queue<std::shared_ptr<LogicNode>> msg_queue_;
	std::mutex mtx_;
	std::condition_variable cv_;
	std::atomic<bool> is_running_;
	std::map<short, FunCallBack> fun_callbacks_;
	std::unordered_map<int, std::shared_ptr<UserInfo>> users_;
};