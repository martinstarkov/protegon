#pragma once

#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "math/hash.h"
#include "utility/debug.h"
#include "utility/type_traits.h"

namespace ptgn {

template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class MapManager {
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

	MapManager()								 = default;
	virtual ~MapManager()						 = default;
	MapManager(MapManager&&) noexcept			 = default;
	MapManager& operator=(MapManager&&) noexcept = default;
	MapManager(const MapManager&)				 = delete;
	MapManager& operator=(const MapManager&)	 = delete;

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
	 * @return True if manager contains key, false otherwise.
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
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in manager");
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
		PTGN_ASSERT(it != std::end(map_), "Entry does not exist in manager");
		return it->second;
	}

	/*
	 * Clears the manager.
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
	[[nodiscard]] bool Empty() const {
		return map_.empty();
	}

	void Reset() {
		map_ = {};
	}

	template <typename TFunc>
	void ForEachValue(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, value);
		}
	}

	template <typename TFunc>
	void ForEachValue(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, value);
		}
	}

	template <typename TFunc>
	void ForEachKey(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, key);
		}
	}

	template <typename TFunc>
	void ForEachKey(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, key);
		}
	}

	template <typename TFunc>
	void ForEachKeyValue(const TFunc& func) {
		for (auto& [key, value] : map_) {
			std::invoke(func, key, value);
		}
	}

	template <typename TFunc>
	void ForEachKeyValue(const TFunc& func) const {
		for (const auto& [key, value] : map_) {
			std::invoke(func, key, value);
		}
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

template <typename ItemType>
class VectorManager {
public:
	using Item = ItemType;

public:
	VectorManager()									   = default;
	virtual ~VectorManager()						   = default;
	VectorManager(VectorManager&&) noexcept			   = default;
	VectorManager& operator=(VectorManager&&) noexcept = default;
	VectorManager(const VectorManager&)				   = delete;
	VectorManager& operator=(const VectorManager&)	   = delete;

	// If item exists in manager, it is returned, otherwise a copy of the item is added.
	Item& Add(const Item& item) {
		for (auto& i : vector_) {
			if (i == item) {
				return i;
			}
		}
		return vector_.emplace_back(item);
	}

	/*
	 * @param item Item to be unloaded.
	 */
	void Remove(const Item& item) {
		vector_.erase(std::remove(vector_.begin(), vector_.end(), item), vector_.end());
	}

	/*
	 * @return True if manager contains the item, false otherwise.
	 */
	template <typename TKey>
	[[nodiscard]] bool Contains(const Item& item) const {
		return std::find(vector_.begin(), vector_.end(), item) != vector_.end();
	}

	/*
	 * Clears the manager.
	 */
	void Clear() {
		vector_.clear();
	}

	/*
	 * @return Number of items in the manager.
	 */
	[[nodiscard]] std::size_t Size() const {
		return vector_.size();
	}

	/*
	 * @return True if the manager has no loaded items, false otherwise.
	 */
	[[nodiscard]] bool Empty() const {
		return vector_.empty();
	}

	void Reset() {
		vector_ = {};
	}

protected:
	using Vector = std::vector<Item>;

	[[nodiscard]] Vector& GetVector() {
		return vector_;
	}

	[[nodiscard]] const Vector& GetVector() const {
		return vector_;
	}

private:
	Vector vector_;
};

// Same as MapManager but has some functions for tracking an active item.
template <
	typename ItemType, typename ExternalKeyType = std::string_view,
	typename InternalKeyType = std::size_t, bool use_hash = true>
class ActiveMapManager : public MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash> {
public:
	using Key  = typename MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Key;
	using Item = typename MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Item;
	using InternalKey =
		typename MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::InternalKey;
	using MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::MapManager;

	ActiveMapManager(const Key& active_key, const Item& active_item) {
		MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Load(
			active_key, active_item
		);
		SetActive(active_key);
	}

	const Item& GetActive() const {
		PTGN_ASSERT(
			(MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Has(active_key_))
		);
		return MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Get(active_key_);
	}

	Item& GetActive() {
		PTGN_ASSERT(
			(MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Has(active_key_))
		);
		return MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Get(active_key_);
	}

	void SetActive(const Key& key) {
		PTGN_ASSERT(
			(MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::Has(key)),
			"Key must be loaded into the manager before setting it as active"
		);
		active_key_ =
			MapManager<ItemType, ExternalKeyType, InternalKeyType, use_hash>::GetInternalKey(key);
	}

private:
	InternalKey active_key_{ 0 };
};

} // namespace ptgn