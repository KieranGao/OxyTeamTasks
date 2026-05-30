#ifndef ASYNCTASKPOOL_H
#define ASYNCTASKPOOL_H

#include "Singleton.h"
#include <boost/asio.hpp>
#include <functional>
#include <thread>
#include <vector>
#include <memory>

class AsyncTaskPool : public Singleton<AsyncTaskPool> {
    friend class Singleton<AsyncTaskPool>;
public:
    void post(std::function<void()> task);
    void stop();

private:
    AsyncTaskPool(size_t thread_count = 0);
    ~AsyncTaskPool();

    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::vector<std::thread> threads_;
};

#endif
