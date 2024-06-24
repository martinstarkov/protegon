#pragma once

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <utility>

#include "protegon/debug.h"
#include "type_traits.h"

namespace ptgn {

/*
 * @tparam T Type of item stored in the manager.
 */
template <typename T>
class HandleManager {
public:
	HandleManager()								   = default;
	virtual ~HandleManager()					   = default;
	HandleManager(HandleManager&&)				   = default;
	HandleManager& operator=(HandleManager&&)	   = default;
	HandleManager(const HandleManager&)			   = delete;
	HandleManager& operator=(const HandleManager&) = delete;

	template <typename... TArgs, type_traits::constructible<T, TArgs...> = true>
	T Load(std::size_t key, TArgs&&... constructor_args) {
		return map_.try_emplace(key, std::forward<TArgs>(constructor_args)...)
			.first->second;
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
	[[nodiscard]] bool Has(std::size_t key) const {
		auto it{ map_.find(key) };
		return it != std::end(map_);
	}

	/*
	 * @param key Id of the item to be retrieved.
	 * @return Shared pointer to the desired item.
	 */
	[[nodiscard]] T Get(std::size_t key) {
		auto it{ map_.find(key) };
		PTGN_CHECK(
			it != std::end(map_), "Entry does not exist in resource manager"
		);
		return it->second;
	}

	/*
	 * Clears the manager.
	 */
	void Clear() {
		map_.clear();
	}

private:
	friend class SceneManager;
	using Map = std::unordered_map<std::size_t, T>;

	[[nodiscard]] Map& GetMap() {
		return map_;
	}

	Map map_;
};

} // namespace ptgn