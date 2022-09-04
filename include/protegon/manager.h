#pragma once

#include <cassert>       // assert
#include <cstdlib>       // std::size_t
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map
#include <utility>       // std::forward

#include "type_traits.h"

namespace ptgn {

/*
* @tparam T Type of item stored in the manager.
*/
template <typename T>
class ResourceManager {
public:
    ResourceManager() = default;
    virtual ~ResourceManager() = default;
    ResourceManager(ResourceManager&&) = default;
    ResourceManager& operator=(ResourceManager&&) = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    
    template <typename ...TArgs,
        type_traits::constructible<T, TArgs...> = true>
    std::shared_ptr<T> Load(std::size_t key, TArgs&&... constructor_args) {
        auto p = map_.try_emplace(key, nullptr);
        // If key is new, initialize a shared ptr, else return current value.
        if (p.second)
            p.first->second = std::make_shared<T>(std::forward<TArgs>(constructor_args)...);
        assert(p.first->second != nullptr && "Previous shared ptr with matching key reset externally?");
        return p.first->second;
    }

    /*
    * @param key Id of the item to be unloaded.
    */
    void Unload(std::size_t key) {
        map_.erase(key);
    }

    /*
    * @param key Id of the item to be checked for.
    * @return True if manager contains key, false otherwise
    */
    bool Has(std::size_t key) const {
        auto it{ map_.find(key) };
        return it != std::end(map_);
    }

    /*
    * @param key Id of the item to be retrieved.
    * @return Shared pointer to he desired item.
    */
    const std::shared_ptr<T> Get(std::size_t key) const {
        auto it{ map_.find(key) };
        if (it == std::end(map_))
            return nullptr;
        return it->second;
    }

    /*
    * @param key Id of the item to be retrieved.
    * @return Shared pointer to the desired item.
    */
    std::shared_ptr<T> Get(std::size_t key) {
        auto it{ map_.find(key) };
        if (it == std::end(map_))
            return nullptr;
        return it->second;
    }

    /*
    * Clears the manager.
    */
    void Clear() {
        map_.clear();
    }
private:
    std::unordered_map<std::size_t, std::shared_ptr<T>> map_;
};

} // namespace ptgn