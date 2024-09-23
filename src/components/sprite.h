#pragma once

#include <vector>

#include "protegon/color.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/time.h"

namespace ptgn {

struct Sprite {
	Texture texture;
	Rectangle<float> source;
};

struct SpriteSheet {
	Texture texture;
	std::vector<Rectangle<float>> sprites; // Source rectangles.
};

struct Animation : public SpriteSheet {
	milliseconds duration{ 0 };
	std::size_t current{ 0 };
};

struct SpriteTint : public Color {
	using Color::Color;
	using Color::operator=;

	SpriteTint(const Color& c) : Color{ c } {}
};

using SpriteFlip = Flip;

using SpriteZ = float;

} // namespace ptgn