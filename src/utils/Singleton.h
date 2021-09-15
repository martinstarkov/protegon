#pragma once

namespace ptgn {

// Generic singleton class for inheritence use.
template <typename T>
class Singleton {
public:
    static T& GetInstance() {
        static T instance;
        return instance;
    }
    Singleton() = default;
    ~Singleton() = default;
    Singleton(Singleton const&) = delete;
    void operator=(Singleton const&) = delete;
};

} // namespace ptgn