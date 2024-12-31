#pragma once

#include "renderer/color.h"
#include "renderer/flip.h"

namespace ptgn {

struct ColorComponent : public Color {
	using Color::Color;
	using Color::operator=;

	ColorComponent(const Color& c) : Color{ c } {}

	operator Color() const {
		return Color{ r, g, b, a };
	}
};

struct FloatComponent {
	FloatComponent() = default;
	FloatComponent(float value) : value_{ value } {}

	operator float() const {
		return value_;
	}

private:
	float value_{ 0.0f };
};

struct FlipComponent {
	FlipComponent() = default;
	FlipComponent(Flip flip) : flip_{ flip } {}

	operator Flip() const {
		return flip_;
	}

private:
	Flip flip_{ Flip::None };
};


} // namespace ptgn