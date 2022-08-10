#pragma once

#include <cstdlib>       // std::size_t
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map
#include <utility>       // std::forward

#include "utility/TypeTraits.h"

namespace ptgn {

namespace manager {

using id = std::size_t;

/*
* @tparam T Type of item stored in the manager.
* @tparam I Type of the identifier that matches items.
*/
template <typename T, typename I = id>
class ResourceManager {
public:
    ResourceManager() = default;
    virtual ~ResourceManager() = default;
    ResourceManager(const ResourceManager& copy) = delete;
    ResourceManager(ResourceManager&& move) noexcept = default;
    ResourceManager& operator=(const ResourceManager& copy) = delete;
    ResourceManager& operator=(ResourceManager&& move) noexcept = default;
    
    template <typename ...TArgs,
        tt::constructible<T, TArgs...> = true>
    T& Load(const I key, TArgs&&... constructor_args) {
        return Set(key, new T{ std::forward<TArgs>(constructor_args)... });
    }

    T& LoadPointer(const I key, T* item) {
        return Set(key, item);
    }
    /*
    * @param key Id of the item to be unloaded.
    */
    void Unload(const I key) {
        map_.erase(key);
    }
    /*
    * Clears the manager.
    */
    void Clear() {
        map_.clear();
    }
    /*
    * @param key Id of the item to be checked for.
    * @return True if manager contains key, false otherwise
    */
    bool Has(const I key) const {
        auto it{ map_.find(key) };
        return it != std::end(map_) && it->second != nullptr;
    }
    /*
    * @param key Id of the item to be retrieved.
    * @return Pointer to the desired item, nullptr if no such item exists.
    */
    std::shared_ptr<T> Get(const I key) const {
        auto it{ map_.find(key) };
        if (it == std::end(map_))
            return nullptr;
        return it->second;
    }
    
    /*
    * Replaces or adds a new entry to the manager
    * @param key Id of the item to be added.
    * @param item Item to be added.
    */
    T& Set(const I key, T* item) {
        auto it{ map_.find(key) };
        if (it != std::end(map_) && it->second != nullptr) {
            return *it->second;
        } else {
            auto [new_it, inserted] = map_.emplace(key, item);
            return *new_it->second;
        }
    }

    template <typename TLambda>
    void ForEach(TLambda lambda) {
        for (auto it = map_.begin(); it != map_.end(); ++it)
            lambda(*it->second);
    }

    std::size_t Size() const {
        return map_.size();
    }

    std::unordered_map<I, std::shared_ptr<T>>& GetMap() {
        return map_;
    }
private:
    std::unordered_map<I, std::shared_ptr<T>> map_;
};

// TODO: Check that T is child of ResourceManager.
template <typename T>
T& Get() {
    static T manager{};
    return manager;
}

} // namespace manager

} // namespace ptgn