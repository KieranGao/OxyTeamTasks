#ifndef SINGLETON_h
#define SINGLETON_h

#include <iostream>
#include <mutex>
#include <memory>

template<typename T> 
class Singleton {
protected:
    Singleton() = default;
    ~Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton& obj) = delete;
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};

#endif /* SINGLETON_h */


