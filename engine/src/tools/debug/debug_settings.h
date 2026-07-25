#pragma once

#include "core/graphics/fill_style.h"
#include "core/graphics/color.h"
#include "serialization/serialize.h"

namespace ptgn {

struct InteractiveDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	float draw_line_width{ 2.0f };

	PTGN_REFLECT(InteractiveDebugSettings, draw_enabled, draw_color, draw_line_width)
};

struct CollisionDebugSettings {
	/// @brief If true, draws continuous collision detection sweeps for debugging purposes.
	bool draw_ccd{ false };

	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	FillStyle draw_fill_style{ 1.0f };

	[[nodiscard]] bool DrawCCD() const {
		return draw_enabled && draw_ccd;
	}

	PTGN_REFLECT(CollisionDebugSettings, draw_ccd, draw_enabled, draw_color, draw_fill_style)
};

struct TextDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	Color clip_draw_color{ color::Blue };
	float draw_line_width{ 2.0f };

	PTGN_REFLECT(TextDebugSettings, draw_enabled, draw_color, clip_draw_color, draw_line_width)
};

struct LightVisibilityDebugSettings {
	bool draw_enabled{ true };
	bool draw_interiors{ true };

	Color polygon_color{ color::Yellow };
	Color masks_inside_color{ color::Red };
	Color does_not_mask_inside_color{ color::Green };

	FillStyle draw_fill_style{ 2.0f };

	PTGN_REFLECT(
		LightVisibilityDebugSettings, draw_enabled, draw_interiors, polygon_color,
		masks_inside_color, does_not_mask_inside_color, draw_fill_style
	)
};

struct DebugSettings {
    InteractiveDebugSettings interaction;
    CollisionDebugSettings collision;
    TextDebugSettings text;
    LightVisibilityDebugSettings light;

    PTGN_REFLECT(DebugSettings, interaction, collision, text, light)
};

} // namespace ptgn