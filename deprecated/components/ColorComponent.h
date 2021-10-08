#pragma once

#include "renderer/Color.h"
#include "renderer/Colors.h"

namespace ptgn {

struct ColorComponent {
	ColorComponent() = default;
	~ColorComponent() = default;
	ColorComponent(const Color& color) : color{ color } {}
	Color color;
};

} // namespace ptgn