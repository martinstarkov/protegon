#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "core/event/dispatcher.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

class Scripts {
public:
	Scripts()			= default;
	~Scripts() noexcept = default;

	Scripts(const Scripts&)			   = delete;
	Scripts& operator=(const Scripts&) = delete;

	Scripts(Scripts&&) noexcept			   = default;
	Scripts& operator=(Scripts&&) noexcept = default;

	/// @brief Adds an instance of a script of type T to the given entity (multiple instances can
	/// exist), forwarding the given arguments to its constructor.
	/// @return Reference to the added script instance.
	template <ScriptType T, typename... TArgs>
	T& Add(Entity e, TArgs&&... constructor_args) {
		auto sp	   = std::make_unique<T>(std::forward<TArgs>(constructor_args)...);
		sp->entity = e;
		auto& script{ scripts_.emplace_back(std::move(sp)) };
		script->OnCreate();
		return static_cast<T&>(*script);
	}

	/// @brief Removes all instances of the script type T from the scripts.
	/// @return True if a script of type T was found and removed, false otherwise.
	template <ScriptType T>
	bool Remove() {
		bool removed = false;

		auto it =
			std::remove_if(scripts_.begin(), scripts_.end(), [&](const std::unique_ptr<Script>& s) {
				if (dynamic_cast<T*>(s.get())) {
					// s->OnDestroy(); // optional lifecycle hook
					removed = true;
					return true;
				}
				return false;
			});

		scripts_.erase(it, scripts_.end());
		return removed;
	}

	/// @return True if an instance of a script of type T was found, false otherwise.
	template <ScriptType T>
	[[nodiscard]] bool Has() const {
		return std::any_of(scripts_.begin(), scripts_.end(), [](const std::unique_ptr<Script>& s) {
			return dynamic_cast<T*>(s.get()) != nullptr;
		});
	}

	/// @brief Emits the given event to all scripts of the entity in the order they were added until
	/// one of them handles it (or until all scripts have been tried).
	void Emit(EventDispatcher d) {
		for (auto& s : scripts_) {
			s->OnEvent(d);
			if (d.IsHandled()) {
				break; // bubbling within this entity's scripts
			}
		}
	}

private:
	std::vector<std::unique_ptr<Script>> scripts_;
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
/// @return True if a script of type T was found and removed, false otherwise.
template <ScriptType T>
bool RemoveScript(Entity entity) {
	if (auto* sc = entity.TryGet<impl::Scripts>()) {
		return sc->Remove<T>();
	}

	return false;
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
