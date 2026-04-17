#pragma once

#include "core/util/concepts.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event_handler.h"

namespace ptgn {

template <typename T, typename... TArgs>
	requires BraceConstructible<T, TArgs...>
void PushEvent(Entity entity, TArgs&&... args) {
	Scene& scene{ entity.GetScene() };
	SceneContext& ctx{ scene.ctx() };
	LocalEventHandler& event_handler{ ctx.event };
	event_handler.Push<T>(entity, std::forward<TArgs>(args)...);
}

} // namespace ptgn