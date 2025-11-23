#pragma once

#include "core/event/event.h"
#include "ecs/entity.h"

namespace ptgn {

class Scripts;

class Script {
public:
	virtual ~Script() = default;

	virtual void OnCreate() {}

	virtual void OnEvent(EventDispatcher) {}

protected:
	friend class Scripts;

	// Global emit (via ApplicationContext)
	void Emit(EventDispatcher d);

	void EmitScene(EventDispatcher d);

	Entity entity;
};

} // namespace ptgn