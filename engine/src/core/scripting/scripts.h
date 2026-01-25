#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "core/event/event.h"
#include "core/scripting/script.h"
#include "ecs/entity.h"

namespace ptgn {

class Scripts {
public:
	Scripts()			= default;
	~Scripts() noexcept = default;

	Scripts(const Scripts&)			   = delete;
	Scripts& operator=(const Scripts&) = delete;

	Scripts(Scripts&&) noexcept			   = default;
	Scripts& operator=(Scripts&&) noexcept = default;

	template <typename T, typename... Args>
	// TODO: Add concept base of ScriptBase instead of static assert.
	T& Add(Entity e, Args&&... args) {
		static_assert(std::is_base_of_v<Script, T>);
		auto sp	   = std::make_unique<T>(std::forward<Args>(args)...);
		sp->entity = e;
		auto& script{ scripts_.emplace_back(std::move(sp)) };
		script->OnCreate();
		return static_cast<T&>(*script);
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

template <typename T, typename... TArgs>
T& AddScript(Entity e, TArgs&&... args) {
	static_assert(std::is_base_of_v<Script, T>, "T must derive from Script.");

	Scripts& sc = e.TryAdd<Scripts>();

	// Add script to the Scripts component
	return sc.Add<T>(e, std::forward<TArgs>(args)...);
}

} // namespace ptgn
