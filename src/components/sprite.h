#pragma once

#include <vector>

#include "ecs/ecs.h"
#include "protegon/color.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/time.h"

namespace ptgn {

struct Sprite {
	ecs::Entity entity;
	Texture texture;
	Rectangle<float> rect;
};

struct SpriteSheet {
	ecs::Entity entity;
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

using SpriteOrigin = Origin;

using SpriteZ = float;

} // namespace ptgn