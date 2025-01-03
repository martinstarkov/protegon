#include "event/events.h"

#include "core/game.h"
#include "event/input_handler.h"
#include "math/vector2.h"
#include "renderer/render_target.h"

namespace ptgn {

namespace impl {

V2_float MouseEventBase::GetCurrent(const RenderTarget& render_target) const {
	return game.input.GetMousePosition(render_target);
}

V2_float MouseEventBase::GetPrevious(const RenderTarget& render_target) const {
	return game.input.GetMousePositionPrevious(render_target);
}

V2_float MouseEventBase::GetDifference(const RenderTarget& render_target) const {
	return game.input.GetMouseDifference(render_target);
}

} // namespace impl

} // namespace ptgn