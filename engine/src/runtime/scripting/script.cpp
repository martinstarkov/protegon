#include "runtime/scripting/script.h"

#include "core/event/dispatcher.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

void Script::Emit(EventDispatcher d) {
	entity.GetScene().ctx().event.Emit(d);
}

void Script::EmitScene(EventDispatcher d) {
	entity.GetScene().ctx().event.Emit(d);
}

} // namespace ptgn