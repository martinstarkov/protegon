#include "runtime/graphics/text/text_effect.h"

#include <cmath>

#include "core/math/vector2.h"

namespace ptgn {

V2_float GlyphInstance::GetEffectOffset(float time) const {
	const auto& effect{ render_style.effect };
	auto order{ static_cast<float>(visible_order) };

	// TODO: Get rid of magic numbers.

	float phase{ effect.phase + order * 0.35f };
	float t{ time * effect.speed + phase };

	switch (effect.type) {
		using enum GlyphEffectType;
		case Wobble:
			return { std::sin(t * effect.frequency) * effect.amplitude,
					 std::cos(t * effect.frequency * 1.37f) * effect.amplitude };

		case Wave: return { 0.0f, std::sin(t * effect.frequency) * effect.amplitude };

		case Shake:
			return { std::sin(t * effect.frequency * 17.0f + order * 12.9898f) * effect.amplitude,
					 std::cos(t * effect.frequency * 23.0f + order * 78.233f) * effect.amplitude };

		case Pulse: [[fallthrough]];
		case None:	[[fallthrough]];
		default:	return { 0.0f, 0.0f };
	}
}

float GlyphInstance::GetEffectScale(float time) const {
	const auto& effect{ render_style.effect };
	if (effect.type != GlyphEffectType::Pulse) {
		return 1.0f;
	}

	auto order{ static_cast<float>(visible_order) };
	// TODO: Get rid of magic number.
	float phase{ effect.phase + order * 0.35f };
	return 1.0f + std::sin(time * effect.speed + phase) * effect.amplitude;
}

} // namespace ptgn
