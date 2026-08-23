#pragma once

#include <cstdint>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"
#include "renderer/text/font_style.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class GlyphEffectType : std::uint8_t {
	None,
	Wobble,
	Wave,
	Shake,
	Pulse,
};
PTGN_REFLECT_ENUM(GlyphEffectType)

struct GlyphEffectStyle {
	GlyphEffectType type{ GlyphEffectType::None };
	float amplitude{ 0.0f };
	float frequency{ 0.0f };
	float speed{ 0.0f };
	float phase{ 0.0f };

	constexpr bool operator==(const GlyphEffectStyle& o) const {
		return type == o.type && NearlyEqual(amplitude, o.amplitude) &&
			   NearlyEqual(frequency, o.frequency) && NearlyEqual(speed, o.speed) &&
			   NearlyEqual(phase, o.phase);
	}

	PTGN_REFLECT(GlyphEffectStyle, type, amplitude, frequency, speed, phase)
};

struct GlyphRenderStyle {
	Color color{ color::White };
	GlyphEffectStyle effect{};
	FontStyle flags{ FontStyle::Normal };

	constexpr bool operator==(const GlyphRenderStyle&) const = default;
};

struct Glyph {
	std::uint32_t codepoint{ 0 };

	/// @brief Baseline pen position in local text coordinates.
	V2_float position{};

	Rect plane{};
	Rect uv{};
	impl::TextureId texture{ 0 };

	std::size_t source_run_index{ 0 };
	std::size_t source_codepoint_index{ 0 };
	std::size_t visible_order{ 0 };

	GlyphRenderStyle render_style{};

	/// @brief Logical cursor advance excluding spacing before the glyph.
	float advance{ 0.0f };
};

} // namespace ptgn
