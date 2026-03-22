#include "runtime/scripting/script.h"

#include "app/context.h"
#include "core/event/dispatcher.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_handler.h"
#include "runtime/scene/scene.h"

namespace ptgn {

void Script::Emit(EventDispatcher d) {
	entity.GetScene().app().event.Emit(d);
}

void Script::EmitScene(EventDispatcher d) {
	entity.GetScene().event.Emit(d);
}

} // namespace ptgn