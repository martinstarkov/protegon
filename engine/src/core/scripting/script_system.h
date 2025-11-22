#ifndef ENGINE_SCRIPT_SYSTEM_H_
#define ENGINE_SCRIPT_SYSTEM_H_

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "event_handler.h"
#include "events.h"
#include "script_base.h"
#include "scripts.h"

namespace engine {

class Entity;

class ScriptSystem {
public:
	explicit ScriptSystem(EventHandler& event_handler);

	template <typename T, typename... Args>
	T& AddScript(Entity entity, Scripts& scripts_component, Args&&... args);

	template <typename T>
	void RemoveScript(Scripts& scripts_component);

private:
	EventHandler& event_handler_;

	std::vector<KeyScript*> key_scripts_;
	std::vector<CollisionScript*> collision_scripts_;

	std::unordered_map<BaseScript*, Scripts*> script_owners_;
	std::vector<BaseScript*> pending_removals_;

	int dispatch_depth_ = 0;

	bool is_dispatching() const {
		return dispatch_depth_ > 0;
	}

	template <typename T>
	void RegisterScript(T* script);

	template <typename T>
	void UnregisterScript(T* script);

	void BeginDispatch();
	void EndDispatch();
	void ApplyDeferredRemovals();

	void OnKeyDown(const KeyDownEvent& event);
	void OnCollision(const CollisionEvent& event);
};

inline ScriptSystem::ScriptSystem(EventHandler& event_handler) : event_handler_(event_handler) {
	event_handler_.Subscribe<KeyDownEvent>(this, [this](const KeyDownEvent& event) {
		OnKeyDown(event);
	});
	event_handler_.Subscribe<CollisionEvent>(this, [this](const CollisionEvent& event) {
		OnCollision(event);
	});
}

template <typename T, typename... Args>
T& ScriptSystem::AddScript(Entity entity, Scripts& scripts_component, Args&&... args) {
	if (is_dispatching()) {
		throw std::runtime_error("AddScript during dispatch is not allowed");
	}
	auto ptr = std::make_unique<T>(entity, std::forward<Args>(args)...);
	T& ref	 = *ptr;
	RegisterScript<T>(&ref);
	script_owners_[ptr.get()] = &scripts_component;
	scripts_component.instances.emplace_back(std::move(ptr));
	return ref;
}

template <typename T>
void ScriptSystem::RemoveScript(Scripts& scripts_component) {
	auto& vec = scripts_component.instances;
	for (auto& uptr : vec) {
		if (auto* derived = dynamic_cast<T*>(uptr.get())) {
			if (is_dispatching()) {
				pending_removals_.push_back(derived);
			} else {
				UnregisterScript<T>(derived);
				script_owners_.erase(derived);
				uptr.reset();
			}
			break;
		}
	}
	if (!is_dispatching()) {
		vec.erase(
			std::remove_if(
				vec.begin(), vec.end(),
				[](const std::unique_ptr<BaseScript>& p) { return p == nullptr; }
			),
			vec.end()
		);
	}
}

template <typename T>
void ScriptSystem::RegisterScript(T* script) {
	if constexpr (std::is_base_of_v<KeyScript, T>) {
		key_scripts_.push_back(static_cast<KeyScript*>(script));
	}
	if constexpr (std::is_base_of_v<CollisionScript, T>) {
		collision_scripts_.push_back(static_cast<CollisionScript*>(script));
	}
}

template <typename T>
void ScriptSystem::UnregisterScript(T* script) {
	if constexpr (std::is_base_of_v<KeyScript, T>) {
		auto* ks = static_cast<KeyScript*>(script);
		auto it	 = std::remove(key_scripts_.begin(), key_scripts_.end(), ks);
		key_scripts_.erase(it, key_scripts_.end());
	}
	if constexpr (std::is_base_of_v<CollisionScript, T>) {
		auto* cs = static_cast<CollisionScript*>(script);
		auto it	 = std::remove(collision_scripts_.begin(), collision_scripts_.end(), cs);
		collision_scripts_.erase(it, collision_scripts_.end());
	}
}

inline void ScriptSystem::BeginDispatch() {
	++dispatch_depth_;
}

inline void ScriptSystem::EndDispatch() {
	--dispatch_depth_;
	if (!is_dispatching()) {
		ApplyDeferredRemovals();
	}
}

inline void ScriptSystem::ApplyDeferredRemovals() {
	if (pending_removals_.empty()) {
		return;
	}
	auto to_remove = std::move(pending_removals_);
	pending_removals_.clear();
	for (BaseScript* script : to_remove) {
		if (!script) {
			continue;
		}
		if (auto* ks = dynamic_cast<KeyScript*>(script)) {
			auto it = std::remove(key_scripts_.begin(), key_scripts_.end(), ks);
			key_scripts_.erase(it, key_scripts_.end());
		}
		if (auto* cs = dynamic_cast<CollisionScript*>(script)) {
			auto it = std::remove(collision_scripts_.begin(), collision_scripts_.end(), cs);
			collision_scripts_.erase(it, collision_scripts_.end());
		}
		auto it_owner = script_owners_.find(script);
		if (it_owner == script_owners_.end()) {
			continue;
		}
		Scripts* scripts_component = it_owner->second;
		script_owners_.erase(it_owner);
		auto& vec = scripts_component->instances;
		vec.erase(
			std::remove_if(
				vec.begin(), vec.end(),
				[script](const std::unique_ptr<BaseScript>& p) { return p.get() == script; }
			),
			vec.end()
		);
	}
}

inline void ScriptSystem::OnKeyDown(const KeyDownEvent& event) {
	BeginDispatch();
	for (KeyScript* script : key_scripts_) {
		script->OnKeyDown(event);
	}
	EndDispatch();
}

inline void ScriptSystem::OnCollision(const CollisionEvent& event) {
	BeginDispatch();
	for (CollisionScript* script : collision_scripts_) {
		script->OnCollision(event);
	}
	EndDispatch();
}

} // namespace engine

#endif // ENGINE_SCRIPT_SYSTEM_H_
