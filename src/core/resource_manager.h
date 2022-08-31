#pragma once

#include <cassert>       // assert
#include <cstdlib>       // std::size_t
#include <memory>        // std::shared_ptr
#include <unordered_map> // std::unordered_map
#include <utility>       // std::forward

#include "protegon/type_traits.h"

namespace ptgn {

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
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = default;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager& operator=(ResourceManager&&) noexcept = default;
    
    template <typename ...TArgs,
        type_traits::constructible<T, TArgs...> = true>
    T& Load(I key, TArgs&&... constructor_args) {
        auto& it{ map_.find(key) };
        if (it != std::end(map_) && it->second != nullptr) {
            return *it->second;
        } else {
            auto [new_it, inserted] = map_.emplace(
                key, std::make_shared<T>(std::forward<TArgs>(constructor_args)...));
            return *new_it->second;
        }
    }

    /*
    * @param key Id of the item to be unloaded.
    */
    void Unload(I key) {
        map_.erase(key);
    }

    /*
    * @param key Id of the item to be checked for.
    * @return True if manager contains key, false otherwise
    */
    bool Has(I key) const {
        auto& it{ map_.find(key) };
        return it != std::end(map_) && it->second != nullptr;
    }

    /*
    * @param key Id of the item to be retrieved.
    * @return Pointer to the desired item, nullptr if no such item exists.
    */
    T& Get(const I key) const {
        auto& it{ map_.find(key) };
        assert(it != std::end(map_) && it->second != nullptr);
        return *it->second;
    }

    /*
    * Clears the manager.
    */
    void Clear() {
        map_.clear();
    }
private:
    std::unordered_map<I, std::shared_ptr<T>> map_;
};

} // namespace ptgn