#include "rendering/api/origin.h"

#include "debug/log.h"
#include "math/vector2.h"

namespace ptgn {

namespace impl {

V2_float GetOriginOffsetHalf(Origin origin, const V2_float& half) {
	switch (origin) {
		case Origin::Center:	   return {};
		case Origin::TopLeft:	   return half;
		case Origin::CenterBottom: return V2_float{ 0.0f, -half.y };
		case Origin::CenterTop:	   return { 0.0f, half.y };
		case Origin::BottomRight:  return -half;
		case Origin::BottomLeft:   return V2_float{ half.x, -half.y };
		case Origin::TopRight:	   return V2_float{ -half.x, half.y };
		case Origin::CenterLeft:   return { half.x, 0.0f };
		case Origin::CenterRight:  return { -half.x, 0.0f };
		default:				   PTGN_ERROR("Failed to identify draw origin");
	}
}

} // namespace impl

V2_float GetOriginOffset(Origin origin, const V2_float& size) {
	return impl::GetOriginOffsetHalf(origin, size * 0.5f);
}

} // namespace ptgn