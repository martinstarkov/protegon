#pragma once

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <utility>

#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

/*
 * @tparam Item Type of item stored in the manager.
 * @tparam Key Type of key used to uniquely identify items.
 */
template <typename ItemType, typename KeyType = std::size_t>
class Manager {
public:
	using Item = ItemType;
	using Key  = KeyType;

	Manager()						   = default;
	virtual ~Manager()				   = default;
	Manager(Manager&&)				   = default;
	Manager& operator=(Manager&&)	   = default;
	Manager(const Manager&)			   = delete;
	Manager& operator=(const Manager&) = delete;

	/*
	 * @param key Unique id of the item to be loaded.
	 * @return Reference to the loaded item.
	 */
	template <typename... TArgs, tt::constructible<Item, TArgs...> = true>
	Item& Load(const Key& key, TArgs&&... constructor_args) {
		auto& item{ map_[key] };
		item = std::move(Item{ std::forward<TArgs>(constructor_args)... });
		return item;
	}

	/*
	 * @param key Id of the item to be unloaded.
	 */
	void Unload(const Key& key) {
		map_.erase(key);
	}

	/*
	 * @param key Id of the item to be checked for.
	 * @return True if manager contains key, false otherwise
	 */
	[[nodiscard]] bool Has(const Key& key) const {
		auto it{ map_.find(key) };
		return it != std::end(map_);
	}

	/*
	 * @param key Id of the item to be retrieved.
	 * @return Reference to the desired item.
	 */
	[[nodiscard]] Item& Get(const Key& key) {
		auto it{ map_.find(key) };
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in resource manager");
		return it->second;
	}

	/*
	 * @param key Id of the item to be retrieved.
	 * @return Const reference to the desired item.
	 */
	[[nodiscard]] const Item& Get(const Key& key) const {
		auto it{ map_.find(key) };
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in resource manager");
		return it->second;
	}

	/*
	 * Clears the manager.
	 */
	void Clear() {
		map_.clear();
		PTGN_ASSERT(Count() == 0);
	}

	/*
	 * @return Number of items in the manager.
	 */
	[[nodiscard]] std::size_t Count() const {
		return map_.size();
	}

	void Reset() {
		map_ = {};
	}

protected:
	using Map = std::unordered_map<Key, Item>;

	[[nodiscard]] Map& GetMap() {
		return map_;
	}

	[[nodiscard]] const Map& GetMap() const {
		return map_;
	}

private:
	Map map_;
};

} // namespace ptgn