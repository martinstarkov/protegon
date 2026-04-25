#pragma once

#include <cstdint>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/resources/id.h"

namespace ptgn {

enum class GlyphEffectType : std::uint8_t {
	None,
	Wobble,
	Wave,
	Shake,
	Pulse,
};

struct GlyphEffectStyle {
	GlyphEffectType type{ GlyphEffectType::None };
	float amplitude{ 0.0f };
	float frequency{ 0.0f };
	float speed{ 0.0f };
	float phase{ 0.0f };
};

struct GlyphRenderStyle {
	Color color{ color::White };
	GlyphEffectStyle effect;
};

struct GlyphInstance {
	std::uint32_t codepoint{ 0 };
	V2_float position;
	Rect plane;
	Rect uv;
	std::uint32_t texture_index{ 0 };
	impl::TextureId texture{ 0 };

	std::size_t source_run_index{ 0 };
	std::size_t source_codepoint_index{ 0 };
	std::size_t line_index{ 0 };
	std::size_t visible_order{ 0 };

	GlyphRenderStyle render_style;
	bool visible{ true };

	V2_float GetEffectOffset(float time) const;
	float GetEffectScale(float time) const;
};

} // namespace ptgn