#pragma once

#include <concepts>
#include <optional>

#include "core/event/event.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_dispatcher.h"

namespace ptgn {

class Script;

namespace impl {

class Scripts;

};

template <typename T>
concept ScriptType = std::derived_from<T, Script>;

class Script {
public:
	virtual ~Script() = default;

	virtual void OnCreate() { /* User implementation */ }

	/// @brief Called once per frame.
	virtual void OnUpdate() { /* User implementation */ }

	virtual void OnEvent(EventDispatcher) { /* User implementation */ }

protected:
	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushEvent(TArgs&&... args) {
		PushEvent<T>(entity, std::forward<TArgs>(args)...);
	}

	template <EventType T, typename... TArgs>
		requires std::constructible_from<T, TArgs...>
	void PushSceneEvent(TArgs&&... args) {
		PushEvent<T>(std::nullopt, std::forward<TArgs>(args)...);
	}

	Entity entity;

private:
	friend class impl::Scripts;

	void SetHash(std::size_t hash) {
		hash_ = hash;
	}

	std::size_t GetHash() const {
		return hash_;
	}

	std::size_t hash_{ 0 };
};

} // namespace ptgn