#pragma once

#include <cstdlib> // std::size_t
#include <memory> // std::shared_ptr
#include <unordered_map> // std::unordered_map
#include <type_traits> // std::enable_if_t
#include <utility> // std::forward

namespace ptgn {

namespace managers {

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
        std::enable_if_t<std::is_constructible_v<T, TArgs...>, bool> = true>
    T& Load(const I key, TArgs&&... constructor_args) {
        return Set(key, new T(std::forward<TArgs>(constructor_args)...));
    }

    T& Load(const I key, T* item) {
        return Set(key, item);
    }
    /*
    * @param key Id of the item to be unloaded.
    */
    void Unload(const I key) {
        map.erase(key);
    }
    /*
    * Clears the manager.
    */
    void Clear() {
        map.clear();
    }
    /*
    * @param key Id of the item to be checked for.
    * @return True if manager contains key, false otherwise
    */
    bool Has(const I key) const {
        auto it{ map.find(key) };
        return it != std::end(map) && it->second != nullptr;
    }
    /*
    * @param key Id of the item to be retrieved.
    * @return Pointer to the desired item, nullptr if no such item exists.
    */
    const T* Get(const I key) const {
        auto it{ map.find(key) };
        if (it == std::end(map))
            return nullptr;
        return it->second.get();
    }
    /*
    * @param key Id of the item to be retrieved.
    * @return Pointer to the desired item, nullptr if no such item exists.
    */
    T* Get(const I key) {
        return const_cast<T*>(const_cast<const ResourceManager<T>*>(this)->Get(key));
    }
    /*
    * Replaces or adds a new entry to the manager
    * @param key Id of the item to be added.
    * @param item Item to be added.
    */
    T& Set(const I key, T* item) {
        auto it{ map.find(key) };
        if (!(it == std::end(map)) && !(it->second.get() == item)) {
            it->second.reset(item);
            return *it->second;
        } else {
            auto [new_it, inserted] = map.emplace(key, item);
            return *new_it->second;
        }
    }
private:
    std::unordered_map<I, std::shared_ptr<T>> map;
};

template <typename T>
T& GetManager() {
    static T manager;
    return manager;
}

} // namespace managers

} // namespace ptgn