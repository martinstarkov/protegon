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
	Rectangle<float> rect;
};

struct SpriteSheet {
	std::vector<Sprite> sprites;
};

struct Animation : public SpriteSheet {
	milliseconds duration{ 0 };
	std::size_t current{ 0 };
};

struct SpriteTint : public Color {
	using Color::Color;
};

using SpriteFlip = Flip;

using SpriteZ = float;

} // namespace ptgn