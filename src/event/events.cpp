#include "event/events.h"

#include "core/game.h"
#include "event/input_handler.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

V2_float MouseEventBase::GetCurrent(std::size_t render_layer) const {
	return game.input.GetMousePosition(render_layer);
}

V2_float MouseEventBase::GetPrevious(std::size_t render_layer) const {
	return game.input.GetMousePositionPrevious(render_layer);
}

V2_float MouseEventBase::GetDifference(std::size_t render_layer) const {
	return game.input.GetMouseDifference(render_layer);
}

} // namespace impl

} // namespace ptgn