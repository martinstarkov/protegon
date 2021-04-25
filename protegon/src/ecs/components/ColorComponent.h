#pragma once

#include "renderer/Color.h"

namespace engine {

struct ColorComponent {
	ColorComponent() = default;
	ColorComponent(const Color& color) : color{ color } {}
	Color color{ colors::BLACK };
};

} // namespace engine