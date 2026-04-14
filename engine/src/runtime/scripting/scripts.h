#pragma once

#include <algorithm>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


#include "core/util/hash.h"
#include "runtime/ecs/entity.h"

#include "runtime/scripting/script.h"

namespace ptgn {

class Scene;
class Tween;

namespace impl {

class TweenData;

class Scripts {
public:
	Scripts()			= default;
	~Scripts() noexcept = default;

	Scripts(const Scripts&)			   = delete;
	Scripts& operator=(const Scripts&) = delete;

	Scripts(Scripts&&) noexcept			   = default;
	Scripts& operator=(Scripts&&) noexcept = default;

	bool operator==(const Scripts&) const = default;

	/// @brief Adds an instance of a script of type T to the given entity (multiple instances can
	/// exist), forwarding the given arguments to its constructor.
	/// @return Reference to the added script instance.
	template <ScriptType T, typename... TArgs>
	T& Add(Entity e, TArgs&&... constructor_args) {
		auto sp	   = std::make_unique<T>(std::forward<TArgs>(constructor_args)...);
		sp->entity = e;
		constexpr auto hash{ Hash<T>() };
		sp->SetHash(hash);

		auto* raw = sp.get();
		pending_add_.push_back(std::move(sp));
		return *raw;
	}

	/// @brief Removes all instances of the script type T from the scripts.
	template <ScriptType T>
	void Remove() {
		constexpr auto hash{ Hash<T>() };
		pending_remove_.emplace_back(hash);
	}

	/// @return True if an instance of a script of type T was found, false otherwise.
	template <ScriptType T>
	[[nodiscard]] bool Has() const {
		constexpr auto hash{ Hash<T>() };
		return std::any_of(scripts_.begin(), scripts_.end(), [](const std::unique_ptr<Script>& s) {
			return s->GetHash() == hash;
		});
	}

private:
	friend class ptgn::Entity;
	friend class ptgn::Scene;
	friend class ptgn::Tween;
	friend class TweenData;

	void Update() {
		// Scripts wont be modified during dispatch because pending script changes are applied by
		// ApplyPending.
		for (const auto& s : scripts_) {
			s->OnUpdate();
		}
	}

	void ApplyPending() {
		if (!pending_remove_.empty()) {
			std::erase_if(scripts_, [this](const auto& s) {
				return std::ranges::contains(pending_remove_, s->hash_);
			});
		}

		// If a script was added and removed in the same frame, we never add it.
		if (!pending_remove_.empty()) {
			std::erase_if(pending_add_, [this](const auto& s) {
				return std::ranges::contains(pending_remove_, s->hash_);
			});
		}

		pending_remove_.clear();

		for (auto& s : pending_add_) {
			s->OnCreate();
			scripts_.push_back(std::move(s));
		}

		pending_add_.clear();
	}

	/// @brief Emits the given event to all scripts of the entity in the order they were added until
	/// one of them handles it (or until all scripts have been tried).
	void OnEvent(Event dispatcher) const {
		// Scripts wont be modified during dispatch because pending script changes are applied by
		// ApplyPending.
		for (const auto& s : scripts_) {
			s->OnEvent(dispatcher);
			if (dispatcher.IsHandled()) {
				break; // bubbling within this entity's scripts
			}
		}
	}

	std::vector<std::unique_ptr<Script>> scripts_;
	std::vector<std::unique_ptr<Script>> pending_add_;
	std::vector<std::size_t> pending_remove_;
};

} // namespace impl

/// @brief Adds an instance of a script of type T to the given entity (multiple instances can
/// exist), forwarding the given arguments to its constructor.
/// @return Reference to the added script instance.
template <ScriptType T, typename... TArgs>
T& AddScript(Entity entity, TArgs&&... constructor_args) {
	auto& sc = entity.TryAdd<impl::Scripts>();

	return sc.Add<T>(entity, std::forward<TArgs>(constructor_args)...);
}

/// @brief Removes all instances of the script type T from the entity's scripts.
template <ScriptType T>
void RemoveScript(Entity entity) {
	if (auto* sc = entity.TryGet<impl::Scripts>()) {
		sc->Remove<T>();
	}
}

/// @return True if the entity has an instance of a script of type T, false otherwise.
template <ScriptType T>
bool HasScript(Entity entity) {
	if (auto* sc = entity.TryGet<impl::Scripts>()) {
		return sc->Has<T>();
	}
	return false;
}

} // namespace ptgn
