#pragma once

#include <optional>
#include <string>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Project owned window defaults. These are serialized in the project manifest.
/// Runtime window movement, resizing, and maximize changes do not mutate these defaults.
struct WindowSettings {
	std::string title{ "Default Title" };
	Color background_color{ color::Transparent };
	V2_int size{ 800, 800 };
	bool resizable{ true };
	bool maximized{ false };

	PTGN_REFLECT(WindowSettings, title, background_color, size, resizable, maximized)
};

/// @brief Machine/user specific overrides stored in the project .ptgnlocal file.
/// Optional fields allow project defaults to remain active when an override is absent.
struct WindowLocalSettings {
	std::optional<V2_int> position;
	std::optional<V2_int> size;
	std::optional<bool> maximized;

	PTGN_REFLECT(WindowLocalSettings, position, size, maximized)
};

} // namespace ptgn
