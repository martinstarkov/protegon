#include "renderer/origin.h"

namespace ptgn {

V2_float GetOffsetFromCenter(const V2_float& size, Origin draw_origin) {
	// Not in the switch to skip half calculation.
	if (draw_origin == Origin::Center) {
		return {};
	}

	V2_float half{ size * 0.5f };
	V2_float offset;

	switch (draw_origin) {
		case Origin::TopLeft: {
			offset = -half;
			break;
		};
		case Origin::CenterBottom: {
			offset = { 0.0f, half.y };
			break;
		};
		case Origin::CenterTop: {
			offset = { 0.0f, -half.y };
			break;
		};
		case Origin::BottomRight: {
			offset = half;
			break;
		};
		case Origin::BottomLeft: {
			offset = V2_float{ -half.x, half.y };
			break;
		};
		case Origin::TopRight: {
			offset = V2_float{ half.x, -half.y };
			break;
		};
		case Origin::CenterLeft: {
			offset = { -half.x, 0.0f };
			break;
		};
		case Origin::CenterRight: {
			offset = { half.x, 0.0f };
			break;
		};
		default: PTGN_ERROR("Failed to identify draw origin");
	}

	return offset;
}

} // namespace ptgn