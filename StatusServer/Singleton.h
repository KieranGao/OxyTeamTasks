#ifndef SINGLETON_H
#define SINGLETON_H

#include <iostream>
#include <mutex>
#include <memory>

template<typename T>
class Singleton {
protected:
    ~Singleton() = default;
    Singleton() = default;
    Singleton<T>& operator=(const Singleton<T>&) = delete;
    Singleton(const Singleton<T>&) = delete;
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};


#endif /* SINGLETON_H */
