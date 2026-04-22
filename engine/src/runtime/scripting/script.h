#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <ostream>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Script;
class Scene;
class Tween;

namespace impl {

class TweenData;
class Scripts;

}; // namespace impl

template <typename T>
concept ScriptType = std::derived_from<T, Script>;

class Script {
public:
	virtual ~Script() = default;

	virtual void OnCreate() { /* User implementation */ }

	/// @brief Called once per frame.
	virtual void OnUpdate() { /* User implementation */ }

	virtual void OnEvent(Event) { /* User implementation */ }

protected:
	Entity entity;

private:
	friend class impl::Scripts;

	void SetHash(std::size_t hash);

	std::size_t GetHash() const;

	std::size_t hash_{ 0 };
};

namespace impl {

template <typename T>
struct EventScript : public Script {
	EventScript() = default;

	explicit EventScript(EventCallback<T> callback) : callback_{ std::move(callback) } {}

	void OnEvent(Event event) override {
		event.DispatchVariant<T>(callback_);
	}

private:
	EventCallback<T> callback_;
};

class Scripts {
public:
	Scripts()							   = default;
	~Scripts() noexcept					   = default;
	Scripts(const Scripts&)				   = delete;
	Scripts& operator=(const Scripts&)	   = delete;
	Scripts(Scripts&&) noexcept			   = default;
	Scripts& operator=(Scripts&&) noexcept = default;

	bool operator==(const Scripts&) const = default;

	/// @brief Adds an instance of a script of type T to the given entity (multiple instances can
	/// exist), forwarding the given arguments to its constructor.
	/// @return Reference to the added script instance.
	template <ScriptType T, typename... TArgs>
		requires BraceConstructible<T, TArgs...>
	T& Add(Entity entity, TArgs&&... constructor_args) {
		auto sp	   = std::unique_ptr<T>(new T{ std::forward<TArgs>(constructor_args)... });
		sp->entity = entity;
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

	friend void from_json(const json& j, Scripts& scripts);
	friend void to_json(json& j, const Scripts& scripts);

	friend std::ostream& operator<<(std::ostream& os, const Scripts& scripts) {
		os << "{ script_count: " << scripts.scripts_.size() << " }";
		return os;
	}

private:
	friend class ptgn::Entity;
	friend class ptgn::Scene;
	friend class ptgn::Tween;
	friend class TweenData;

	void Update() const;

	void ApplyPending();

	/// @brief Emits the given event to all scripts of the entity in the order they were added until
	/// one of them handles it (or until all scripts have been tried).
	void OnEvent(Event event) const;

	std::vector<std::unique_ptr<Script>> scripts_;
	std::vector<std::unique_ptr<Script>> pending_add_;
	std::vector<std::size_t> pending_remove_;
};

} // namespace impl

/// @brief Adds an instance of a script of type T to the given entity (multiple instances can
/// exist), forwarding the given arguments to its constructor.
/// @return Reference to the added script instance.
template <ScriptType T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
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