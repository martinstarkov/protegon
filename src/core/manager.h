#pragma once

#include <type_traits>
#include <unordered_map>

#include "math/hash.h"
#include "utility/assert.h"

namespace ptgn {

template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class MapManager {
public:
	using Item		  = ItemType;
	using Key		  = ExternalKeyType;
	using InternalKey = InternalKeyType;

	static_assert(
		!use_hash
			? (std::is_convertible_v<Key, InternalKey> || std::is_constructible_v<InternalKey, Key>)
			: true,
		"When not using hash function, manager template argument list must provide key which is "
		"convertible to internal key"
	);

	// TODO: Add check that provided keys are hashable.
	// static_assert(use_hash ? tt::is_hashable<Key, InternalKey> : true);

	MapManager()								 = default;
	virtual ~MapManager()						 = default;
	MapManager(MapManager&&) noexcept			 = default;
	MapManager& operator=(MapManager&&) noexcept = default;
	MapManager(const MapManager&)				 = delete;
	MapManager& operator=(const MapManager&)	 = delete;

	/*
	 * If key already exists, does nothing.
	 * @param key Unique id of the item to be loaded.
	 * @return Reference to the loaded item.
	 */
	template <typename TKey, typename... TArgs>
	Item& Load(const TKey& key, TArgs&&... constructor_args) {
		static_assert(
			std::is_constructible_v<Item, TArgs...>,
			"Manager item must be constructible from provided constructor arguments"
		);
		auto [it, inserted] =
			map_.try_emplace(GetInternalKey(key), std::forward<TArgs>(constructor_args)...);
		return it->second;
	}

	/*
	 * Unload an item from the manager.
	 * @param key Id of the item to be unloaded.
	 */
	template <typename TKey>
	void Unload(const TKey& key) {
		map_.erase(GetInternalKey(key));
	}

	/*
	 * Check if the manager has a specified item.
	 * @param key Id of the item to be checked for.
	 * @return True if manager contains key, false otherwise.
	 */
	template <typename TKey>
	[[nodiscard]] bool Has(const TKey& key) const {
		return map_.find(GetInternalKey(key)) != std::end(map_);
	}

	/*
	 * Retrieve a specified item from the manager.
	 * @param key Id of the item to be retrieved.
	 * @return Const reference to the desired item.
	 */
	template <typename TKey>
	[[nodiscard]] const Item& Get(const TKey& key) const {
		auto it{ map_.find(GetInternalKey(key)) };
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in manager");
		return it->second;
	}

	/*
	 * Retrieve a specified item from the manager.
	 * @param key Id of the item to be retrieved.
	 * @return Reference to the desired item.
	 */
	template <typename TKey>
	[[nodiscard]] Item& Get(const TKey& key) {
		return const_cast<Item&>(std::as_const(*this).Get(key));
	}

	/*
	 * Clears all manager items. Maintains the capacity of the manager.
	 */
	void Clear() {
		map_.clear();
	}

	/*
	 * @return Number of items in the manager.
	 */
	[[nodiscard]] std::size_t Size() const {
		return map_.size();
	}

	/*
	 * @return True if the manager has no loaded items, false otherwise.
	 */
	[[nodiscard]] bool IsEmpty() const {
		return map_.empty();
	}

	/*
	 * Resets the manager entirely, including capacity.
	 */
	void Reset() {
		map_ = {};
	}

	// Cycles through each value in the manager.
	template <typename TFunc>
	void ForEachValue(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, value);
		}
	}

	// Cycles through each value in the manager.
	template <typename TFunc>
	void ForEachValue(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, value);
		}
	}

	// Cycles through each key in the manager.
	template <typename TFunc>
	void ForEachKey(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, key);
		}
	}

	// Cycles through each key in the manager.
	template <typename TFunc>
	void ForEachKey(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, key);
		}
	}

	// Cycles through each key and value pair in the manager.
	template <typename TFunc>
	void ForEachKeyValue(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, key, value);
		}
	}

	// Cycles through each key and value pair in the manager.
	template <typename TFunc>
	void ForEachKeyValue(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, key, value);
		}
	}

protected:
	// @return The key used internally by the manager when storing items (hashed or not).
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

	void SetMap(const Map& m) {
		map_ = m;
	}

	[[nodiscard]] Map& GetMap() {
		return map_;
	}

	[[nodiscard]] const Map& GetMap() const {
		return map_;
	}

private:
	Map map_;
};

// Same as MapManager but has some additional functions for tracking an active item.
template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class ActiveMapManager : public MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash> {
public:
	using Key		  = typename MapManager<ItemType>::Key;
	using Item		  = typename MapManager<ItemType>::Item;
	using InternalKey = typename MapManager<ItemType>::InternalKey;
	using MapManager<ItemType>::MapManager;

	// Requires providing an initial active item.
	ActiveMapManager()										 = delete;
	~ActiveMapManager() override							 = default;
	ActiveMapManager(ActiveMapManager&&) noexcept			 = default;
	ActiveMapManager& operator=(ActiveMapManager&&) noexcept = default;
	ActiveMapManager(const ActiveMapManager&)				 = delete;
	ActiveMapManager& operator=(const ActiveMapManager&)	 = delete;

	// Load the initial active item into the manager.
	template <typename... TArgs>
	ActiveMapManager(const Key& active_key, TArgs&&... constructor_args) {
		MapManager<ItemType>::Load(active_key, std::forward<TArgs>(constructor_args)...);
		SetActive(active_key);
	}

	// @return The current active manager item.
	const Item& GetActive() const {
		PTGN_ASSERT(MapManager<ItemType>::Has(active_key_), "Active element has not been set");
		return MapManager<ItemType>::Get(active_key_);
	}

	// @return The current active manager item.
	Item& GetActive() {
		return const_cast<Item&>(std::as_const(*this).GetActive());
	}

	/*
	 * Set the current active manager item.
	 * @param key The key of the item to be set as active. It must be loaded in the manager
	 * beforehand.
	 */
	void SetActive(const Key& key) {
		PTGN_ASSERT(
			(MapManager<ItemType>::Has(key)),
			"Key must be loaded into the manager before setting it as active"
		);
		active_key_ = MapManager<ItemType>::GetInternalKey(key);
	}

protected:
	InternalKey active_key_{ 0 };
};

template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class MapManagerWithNameless :
	public MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash> {
public:
	using Key		  = typename MapManager<ItemType>::Key;
	using Item		  = typename MapManager<ItemType>::Item;
	using InternalKey = typename MapManager<ItemType>::InternalKey;
	using MapManager<ItemType>::MapManager;

	MapManagerWithNameless()											 = default;
	~MapManagerWithNameless() override									 = default;
	MapManagerWithNameless(MapManagerWithNameless&&) noexcept			 = default;
	MapManagerWithNameless& operator=(MapManagerWithNameless&&) noexcept = default;
	MapManagerWithNameless(const MapManagerWithNameless&)				 = delete;
	MapManagerWithNameless& operator=(const MapManagerWithNameless&)	 = delete;

	/*
	 * Load a nameless item into the manager. If the item already exists in the nameless list (based
	 * on equals comparison), nothing happens.
	 * @return Reference to the loaded nameless item.
	 */
	template <typename... TArgs, tt::constructible<Item, TArgs...> = true>
	[[nodiscard]] Item& Load(TArgs&&... constructor_args) {
		return nameless_.emplace_back(std::forward<TArgs>(constructor_args)...);
	}

	/*
	 * Clears the manager (including nameless items).
	 */
	void Clear() {
		nameless_.clear();
		MapManager<ItemType>::Clear();
	}

	/*
	 * @return Number of items in the manager (including nameless items).
	 */
	[[nodiscard]] std::size_t Size() const {
		return MapManager<ItemType>::Size() + nameless_.size();
	}

	/*
	 * @return True if the manager has no items (including no nameless items), false otherwise.
	 */
	[[nodiscard]] bool IsEmpty() const {
		return MapManager<ItemType>::IsEmpty() && nameless_.empty();
	}

	// Reset the manager containers (including nameless items).
	void Reset() {
		nameless_ = {};
		MapManager<ItemType>::Reset();
	}

	// Cycles through all the nameless manager items, followed by all those with a key.
	template <typename TFunc>
	void ForEachValue(const TFunc& func) {
		for (auto& value : nameless_) {
			std::invoke(func, value);
		}
		MapManager<ItemType>::ForEachValue(func);
	}

	// Cycles through all the nameless manager items, followed by all those with a key.
	template <typename TFunc>
	void ForEachValue(const TFunc& func) const {
		for (const auto& value : nameless_) {
			std::invoke(func, value);
		}
		MapManager<ItemType>::ForEachValue(func);
	}

protected:
	void SetNamelessContainer(const std::vector<Item>& nameless_container) {
		nameless_ = nameless_container;
	}

	[[nodiscard]] std::vector<Item>& GetNamelessContainer() {
		return nameless_;
	}

	[[nodiscard]] const std::vector<Item>& GetNamelessContainer() const {
		return nameless_;
	}

private:
	std::vector<Item> nameless_;
};

} // namespace ptgn