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

	template <ScriptType T, typename... Args>
	T& Add(Entity e, Args&&... args) {
		auto sp	   = std::make_unique<T>(std::forward<Args>(args)...);
		sp->entity = e;
		auto& script{ scripts_.emplace_back(std::move(sp)) };
		script->OnCreate();
		return static_cast<T&>(*script);
	}

	/// @return True if a script of type T was found and removed, false otherwise.
	template <ScriptType T>
	bool Remove() {
		auto it =
			std::find_if(scripts_.begin(), scripts_.end(), [](const std::unique_ptr<Script>& s) {
				return dynamic_cast<T*>(s.get()) != nullptr;
			});

		if (it == scripts_.end()) {
			return false;
		}

		// (*it)->OnDestroy(); // optional lifecycle hook
		scripts_.erase(it);
		return true;
	}

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

/// Adds a script of type T to the given entity, forwarding the given arguments to the script's
/// constructor.
template <ScriptType T, typename... TArgs>
T& AddScript(Entity entity, TArgs&&... args) {
	auto& sc = entity.TryAdd<impl::Scripts>();

	return sc.Add<T>(entity, std::forward<TArgs>(args)...);
}

/// @return True if a script of type T was found and removed, false otherwise.
template <ScriptType T>
bool RemoveScript(Entity entity) {
	if (auto* sc = entity.TryGet<impl::Scripts>()) {
		return sc->Remove<T>();
	}

	return false;
}

} // namespace ptgn
