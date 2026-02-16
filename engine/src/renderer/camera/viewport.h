#pragma once

#include "core/math/vector2.h"
#include "serialization/json/enum.h"

namespace ptgn {

struct Viewport {
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;
};

enum class ViewportType {
	Game,
	Display,
	World,
	WindowCenter,
	WindowTopLeft
};

PTGN_SERIALIZE_ENUM(
	ViewportType, { { ViewportType::Game, "game" },
					{ ViewportType::Display, "display" },
					{ ViewportType::World, "world" },
					{ ViewportType::WindowCenter, "window_center" },
					{ ViewportType::WindowTopLeft, "window_top_left" } }
);

} // namespace ptgn