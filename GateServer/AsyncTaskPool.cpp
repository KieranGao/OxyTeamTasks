#include "AsyncTaskPool.h"

AsyncTaskPool::AsyncTaskPool(size_t thread_count)
    : work_guard_(boost::asio::make_work_guard(ioc_))
{
    if (thread_count == 0)
        thread_count = std::thread::hardware_concurrency();
    if (thread_count < 4)
        thread_count = 4;
    for (size_t i = 0; i < thread_count; ++i) {
        threads_.emplace_back([this]() {
            ioc_.run();
        });
    }
}

AsyncTaskPool::~AsyncTaskPool() {
    stop();
}

void AsyncTaskPool::post(std::function<void()> task) {
    boost::asio::post(ioc_, std::move(task));
}

void AsyncTaskPool::stop() {
    work_guard_.reset();
    ioc_.stop();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}
