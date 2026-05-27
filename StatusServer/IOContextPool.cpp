#include "IOContextPool.h"
#include "Logger.h"

// 为什么不用加锁？
// 因为每个线程都有自己的io_context对象，线程之间不会共享io_context对象，所以不需要加锁来保护io_context对象的访问。
// 同时，getIOContext()函数只是返回一个io_context对象的引用，并不会修改io_context对象的状态，所以也不需要加锁来保护io_context对象的状态。
IOContextPool::IOContextPool(size_t pool_size) : io_contexts_(pool_size), works_(pool_size)
    , next_io_context_(0) 
{
    // 创建io_context对象
    for(size_t i=0;i<pool_size;i++) {
        io_contexts_[i] = std::make_shared<IOContext>();
    }
    // 使用work管理io_context的生命周期，防止io_context.run()在没有任务时退出
    for(size_t i=0;i<pool_size;i++) {
        works_[i] = std::make_unique<Work>(boost::asio::make_work_guard(*io_contexts_[i]));
    }
    // 每个io_context都在一个独立的线程中运行
    for(size_t i=0;i<pool_size;i++) {
        threads_.emplace_back([this, i](){
            io_contexts_[i]->run(); // 阻塞直到io_context被销毁
        });
    }
}

IOContextPool::~IOContextPool() {
    // RAII，析构时自动调用stop()函数，停止io_context服务，等待线程退出
    stop();
    LOG_DEBUG("IOContextPool is destroyed");
}

boost::asio::io_context& IOContextPool::getIOContext() {
    // 轮询分配io_context
    auto& io_context = *io_contexts_[next_io_context_];
    next_io_context_ = (next_io_context_ + 1) % io_contexts_.size();
    return io_context;
}

void IOContextPool::stop() {
    for(auto& work : works_) {
        work->get_executor().context().stop(); // 先停止io_context服务，io_context.run()会退出
        work.reset(); // work.reset之后，智能指针会回收work对象，从而回收io_context
    }

    for(auto& t : threads_) {
        if(t.joinable()) {
            t.join(); // 等待线程退出
        }
    }
}