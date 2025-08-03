#pragma once

#include <type_traits>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "math/hash.h"
#include "serialization/fwd.h"
#include "utility/file.h"

namespace ptgn {

namespace impl {

template <typename, typename = void>
struct has_static_load_from_file : std::false_type {};

template <typename T>
struct has_static_load_from_file<
	T, std::void_t<decltype(T::LoadFromFile(std::declval<const path&>()))>> : std::true_type {};

} // namespace impl

template <typename Derived, typename HandleType, typename ItemType>
class ResourceManager;

template <typename D, typename H, typename I>
void to_json(json&, const ResourceManager<D, H, I>&);

template <typename D, typename H, typename I>
void from_json(const json&, ResourceManager<D, H, I>&);

template <typename Derived, typename HandleType, typename ItemType>
class ResourceManager {
public:
	using ParentManager = ResourceManager<Derived, HandleType, ItemType>;

	ResourceManager()									   = default;
	virtual ~ResourceManager()							   = default;
	ResourceManager(const ResourceManager&)				   = default;
	ResourceManager& operator=(const ResourceManager&)	   = default;
	ResourceManager(ResourceManager&&) noexcept			   = default;
	ResourceManager& operator=(ResourceManager&&) noexcept = default;

	// Load resources from a json file. Json format must be:
	// {
	//    "resource_key": "path/file.extension",
	//    ...
	// }
	void LoadList(const path& json_filepath);
	void UnloadList(const path& json_filepath);

	void LoadJson(const json& resources);
	void UnloadJson(const json& resources);

	// @param filepath The file path to the resource.
	virtual void Load(const HandleType& key, const path& filepath);

	// Unload a resource by its key. Does nothing if the resource was not loaded.
	void Unload(const HandleType& key);

	// Clear all loaded resources.
	void Clear();

	// @return True if the resource key is loaded.
	[[nodiscard]] bool Has(const HandleType& key) const;

	// @return The path with which the resource was loaded.
	[[nodiscard]] const path& GetPath(const HandleType& key) const;

	friend void to_json<
		Derived, HandleType,
		ItemType>(json&, const ResourceManager<Derived, HandleType, ItemType>&);
	friend void from_json<
		Derived, HandleType,
		ItemType>(const json&, ResourceManager<Derived, HandleType, ItemType>&);

protected:
	[[nodiscard]] const ItemType& Get(const HandleType& key) const;

	struct ResourceInfo {
		ItemType resource;
		path filepath;
		HandleType key;
	};

	std::unordered_map<std::size_t, ResourceInfo> resources_;

private:
	[[nodiscard]] const ResourceInfo& GetResourceInfo(const HandleType& key) const;
};

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
	MapManager(const MapManager&)				 = default;
	MapManager& operator=(const MapManager&)	 = default;

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
		return map_.find(GetInternalKey(key)) != map_.end();
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

	ActiveMapManager()										 = default;
	~ActiveMapManager() override							 = default;
	ActiveMapManager(const ActiveMapManager&)				 = default;
	ActiveMapManager& operator=(const ActiveMapManager&)	 = default;
	ActiveMapManager(ActiveMapManager&&) noexcept			 = default;
	ActiveMapManager& operator=(ActiveMapManager&&) noexcept = default;

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