#pragma once

#include <concepts>

#include "core/event/dispatcher.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

class Scripts;

};

class Script {
public:
	virtual ~Script() = default;

	virtual void OnCreate() {}

	virtual void OnEvent(EventDispatcher) {}

protected:
	friend class impl::Scripts;

	// Global emit (via ApplicationContext)
	void Emit(EventDispatcher d);

	void EmitScene(EventDispatcher d);

	Entity entity;
};

template <typename T>
concept ScriptType = std::derived_from<T, Script>;

} // namespace ptgn