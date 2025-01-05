#include "event/events.h"

#include "core/game.h"
#include "event/input_handler.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

V2_float MouseEventBase::GetCurrent() const {
	return game.input.GetMousePosition();
}

V2_float MouseEventBase::GetPrevious() const {
	return game.input.GetMousePositionPrevious();
}

V2_float MouseEventBase::GetDifference() const {
	return game.input.GetMouseDifference();
}

} // namespace impl

} // namespace ptgn