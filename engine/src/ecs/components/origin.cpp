#include "ecs/components/origin.h"

#include "core/log.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

V2_float GetOriginOffsetHalf(Origin origin, const V2_float& half) {
	switch (origin) {
		using enum ptgn::Origin;
		case Center:	   return {};
		case TopLeft:	   return -half;
		case CenterBottom: return V2_float{ 0.0f, half.y };
		case CenterTop:	   return { 0.0f, -half.y };
		case BottomRight:  return half;
		case BottomLeft:   return V2_float{ -half.x, half.y };
		case TopRight:	   return V2_float{ half.x, -half.y };
		case CenterLeft:   return { -half.x, 0.0f };
		case CenterRight:  return { half.x, 0.0f };
		default:		   PTGN_ERROR("Failed to identify draw origin");
	}
}

} // namespace impl

V2_float GetOriginOffset(Origin origin, const V2_float& size) {
	return impl::GetOriginOffsetHalf(origin, size * 0.5f);
}

} // namespace ptgn