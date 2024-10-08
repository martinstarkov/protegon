#include "renderer/origin.h"

#include "protegon/log.h"
#include "protegon/vector2.h"

namespace ptgn {

V2_float GetOffsetFromCenter(const V2_float& size, Origin draw_origin) {
	switch (draw_origin) {
		case Origin::Center:	   return {};
		case Origin::TopLeft:	   return -size * 0.5f;
		case Origin::CenterBottom: return V2_float{ 0.0f, size.y * 0.5f };
		case Origin::CenterTop:	   return { 0.0f, -size.y * 0.5f };
		case Origin::BottomRight:  return size * 0.5f;
		case Origin::BottomLeft:   return V2_float{ -size.x * 0.5f, size.y * 0.5f };
		case Origin::TopRight:	   return V2_float{ size.x * 0.5f, -size.y * 0.5f };
		case Origin::CenterLeft:   return { -size.x * 0.5f, 0.0f };
		case Origin::CenterRight:  return { size.x * 0.5f, 0.0f };
		default:				   PTGN_ERROR("Failed to identify draw origin");
	}
}

} // namespace ptgn