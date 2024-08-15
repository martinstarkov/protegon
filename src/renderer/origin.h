#pragma once

#include "protegon/vector2.h"

namespace ptgn {

enum class Origin {
	Center,
	CenterTop,
	CenterBottom,
	CenterRight,
	CenterLeft,
	TopLeft,
	TopRight,
	BottomRight,
	BottomLeft,
};

[[nodiscard]] V2_float GetDrawOffset(const V2_float& size, Origin draw_origin);

} // namespace ptgn