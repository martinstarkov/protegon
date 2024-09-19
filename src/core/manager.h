#pragma once

#include <string_view>
#include <unordered_map>

#include "protegon/hash.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

/*
 * @tparam Item Type of item stored in the manager.
 * @tparam Key Type of key used to uniquely identify items.
 */
template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class Manager {
public:
	using Item		  = ItemType;
	using Key		  = ExternalKeyType;
	using InternalKey = InternalKeyType;

public:
	static_assert(
		!use_hash ? std::is_convertible_v<Key, InternalKey> : true,
		"When not using hash function, manager template argument list must provide key which is "
		"convertible to internal key"
	);
	// TODO: Add check that provided keys are hashable.
	// static_assert(use_hash ? tt::is_hashable<Key, InternalKey> : true);

	Manager()							   = default;
	virtual ~Manager()					   = default;
	Manager(Manager&&) noexcept			   = default;
	Manager& operator=(Manager&&) noexcept = default;
	Manager(const Manager&)				   = delete;
	Manager& operator=(const Manager&)	   = delete;

	/*
	 * @param key Unique id of the item to be loaded.
	 * @return Reference to the loaded item.
	 */
	template <typename TKey, typename... TArgs, tt::constructible<Item, TArgs...> = true>
	Item& Load(const TKey& key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(key) };
		auto& item = map_[k];
		item	   = std::move(Item{ std::forward<TArgs>(constructor_args)... });
		return item;
	}

	/*
	 * @param key Id of the item to be unloaded.
	 */
	template <typename TKey>
	void Unload(const TKey& key) {
		auto k{ GetInternalKey(key) };
		map_.erase(k);
	}

	/*
	 * @param key Id of the item to be checked for.
	 * @return True if manager contains key, false otherwise
	 */
	template <typename TKey>
	[[nodiscard]] bool Has(const TKey& key) const {
		auto k{ GetInternalKey(key) };
		auto it{ map_.find(k) };
		return it != std::end(map_);
	}

	/*
	 * @param key Id of the item to be retrieved.
	 * @return Reference to the desired item.
	 */
	template <typename TKey>
	[[nodiscard]] Item& Get(const TKey& key) {
		auto k{ GetInternalKey(key) };
		auto it{ map_.find(k) };
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in resource manager");
		return it->second;
	}

	/*
	 * @param key Id of the item to be retrieved.
	 * @return Const reference to the desired item.
	 */
	template <typename TKey>
	[[nodiscard]] const Item& Get(const TKey& key) const {
		auto k{ GetInternalKey(key) };
		auto it{ map_.find(k) };
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

	/*
	 * @return True if the manager has no loaded items, false otherwise.
	 */
	[[nodiscard]] bool Empty() const {
		return map_.empty();
	}

	void Reset() {
		map_ = {};
	}

protected:
	template <typename TKey>
	[[nodiscard]] static InternalKey GetInternalKey(const TKey& key) {
		InternalKey k;
		if constexpr (use_hash && std::is_convertible_v<TKey, Key>) {
			k = Hash(key);
		} else {
			static_assert(std::is_convertible_v<TKey, InternalKey>);
			k = key;
		}
		return k;
	}

	using Map = std::unordered_map<InternalKey, Item>;

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