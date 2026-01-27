#include "runtime/scripting/script.h"

#include "app/context.h"
#include "core/event/event.h"
#include "core/event/event_handler.h"
#include "runtime/scene/scene.h"

namespace ptgn {

void Script::Emit(EventDispatcher d) {
	entity.GetScene().app().events.Emit(d);
}

void Script::EmitScene(EventDispatcher d) {
	entity.GetScene().events.Emit(d);
}

} // namespace ptgn