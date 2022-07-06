#pragma once

#include <cstdlib> // std::size_t
#include <memory> // std::unique_ptr, std::default_delete
#include <unordered_map> // std::unordered_map
#include <cassert> // assert

namespace ptgn {

namespace managers {

using id = std::size_t;

/*
* @tparam T Type of item stored in the manager.
* @tparam I Type of the identifier that matches items.
* @tparam TDeleter Custom deleter for the type T.
*/
template <typename T, typename TDeleter = std::default_delete<T>, typename I = id>
class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager() = default;
    /*
    * @param key Id of the item to be loaded.
    * @param item Item to be loaded.
    */
    void Load(const I key, T* item) {
        Set(key, item);
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
    void Set(const I key, T* item) {
        auto it{ map.find(key) };
        if (!(it == std::end(map)) && !(it->second.get() == item))
            it->second.reset(item);
        else
            map.emplace(key, item);
    }
private:
    std::unordered_map<I, std::unique_ptr<T, TDeleter>> map;
};

template <typename T>
T& GetManager() {
    static T manager;
    return manager;
}

} // namespace managers

} // namespace ptgn