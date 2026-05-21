#ifndef SINGLETON_H
#define SINGLETON_H

#include <memory>
#include <mutex>
#include <iostream>

// 单例基类，使用CRTP（Curiously Recurring Template Pattern）实现单例模式
template<class T>
class Singleton {

protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton<T>& operator=(const Singleton<T>&) = delete;
    Singleton(const Singleton<T>&) = delete;

public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};



#endif /* SINGLETON_H */



