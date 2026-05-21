#ifndef SINGLETON_H
#define SINGLETON_H

template<typename T>
class Singleton {
protected:
    ~Singleton() = default;
    Singleton() = default;
    Singleton<T>(const Singleton<T>&) = delete;
    Singleton<T>& operator=(const Singleton<T>&) = delete;
public:
    static T& getInstance() {
        static T instance;
        return instance;
    }
};  

#endif /* SINGLETON_H */
