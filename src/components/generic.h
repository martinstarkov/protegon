#pragma once

#include "renderer/color.h"
#include "renderer/flip.h"
#include "utility/type_traits.h"

namespace ptgn {

struct ColorComponent : public Color {
	using Color::Color;
	using Color::operator=;

	ColorComponent(const Color& c) : Color{ c } {}
};

template <typename T, tt::enable<std::is_arithmetic_v<T>> = true>
struct ArithmeticComponent {
	ArithmeticComponent() = default;

	ArithmeticComponent(T value) : value_{ value } {}

	operator T() const {
		return value_;
	}

private:
	T value_{ 0 };
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