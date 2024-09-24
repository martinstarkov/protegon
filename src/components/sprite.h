#pragma once

#include <string>
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
	SpriteSheet() = default;

	SpriteSheet(const Texture& texture) : texture{ texture } {}

	void AddSprite() {}

	Texture texture;
	std::vector<Rectangle<float>> sprites; // Source rectangles.
};

struct Animation : public SpriteSheet {
	Animation(const std::string& name, const Texture& texture, milliseconds duration) :
		name{ name }, duration{ duration } {}

	std::string name;
	milliseconds duration{ 0 };
	std::size_t column{ 0 };
	std::size_t row{ 0 };
};

struct AnimationMap : public MapManager<Animation> {};

struct SpriteTint : public Color {
	using Color::Color;
	using Color::operator=;

	SpriteTint(const Color& c) : Color{ c } {}
};

using SpriteFlip = Flip;

using SpriteZ = float;

} // namespace ptgn