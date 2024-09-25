#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "core/manager.h"
#include "ecs/ecs.h"
#include "protegon/color.h"
#include "protegon/game.h"
#include "protegon/math.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/tween.h"
#include "protegon/vector2.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/debug.h"
#include "utility/time.h"

namespace ptgn {

struct Sprite {
	Texture texture;
	Rectangle<float> source;
};

struct SpriteSheet {
	SpriteSheet() = default;

	// Frames go from left to right.
	SpriteSheet(
		const Texture& texture, const V2_float& frame_size, std::size_t frames,
		const V2_float& start_pixel = {}
	) :
		texture{ texture }, sprite_size{ frame_size } {
		sprite_positions.reserve(frames);
		for (std::size_t i = 0; i < frames; i++) {
			float x = start_pixel.x + frame_size.x * static_cast<float>(i);
			PTGN_ASSERT(x < texture.GetSize().x, "Source position X must be within texture width");
			sprite_positions.emplace_back(x, start_pixel.y);
		}
	}

	Texture texture;
	std::vector<V2_float> sprite_positions; // Top left corners of sprites.
	V2_float sprite_size;					// Size of an individual sprite.
};

struct Animation : public SpriteSheet {
	Animation(
		const Texture& texture, const V2_float& frame_size, std::size_t frames,
		milliseconds animation_duration, const V2_float& draw_offset = {},
		Origin origin = Origin::Center, std::size_t start_frame = 0,
		const V2_float& start_pixel = {}
	) :
		SpriteSheet{ texture, frame_size, frames, start_pixel } {
		duration		  = animation_duration;
		this->draw_offset = draw_offset;
		this->origin	  = origin;
		PTGN_ASSERT(
			start_frame < sprite_positions.size(),
			"Start frame must be within sprite sheet frame count"
		);
		frame = std::make_shared<std::size_t>(start_frame);
	}

	~Animation() {
		game.tween.Remove(tween);
	}

	void Stop() {
		tween.Stop();
	}

	void Start() {
		std::size_t frame_count{ sprite_positions.size() };
		milliseconds frame_duration{ duration / frame_count };
		tween = game.tween.Add(frame_duration)
					.Repeat(-1)
					.OnRepeat([=]() {
						auto& f{ *frame };
						f = Mod(++f, frame_count);
					})
					.Start();
	}

	void Toggle() {
		if (tween.IsStarted()) {
			Stop();
		} else {
			Start();
		}
	}

	static void Draw(ecs::Entity entity);

	Tween tween;

private:
	Rectangle<float> GetSource() const {
		auto& f{ *frame };
		PTGN_ASSERT(f < sprite_positions.size());
		return { sprite_positions[f], sprite_size, origin };
	}

	V2_float draw_offset; // Offset of sprite relative to entity transform.

	Origin origin{ Origin::Center };

	milliseconds duration{ 0 }; // Duration of the entire animation.

	std::shared_ptr<std::size_t> frame;
};

struct AnimationMap : public MapManager<Animation> {
	// TODO: Add current animation key and draw function.
};

struct SpriteTint : public Color {
	using Color::Color;
	using Color::operator=;

	SpriteTint(const Color& c) : Color{ c } {}
};

using SpriteFlip = Flip;

using SpriteZ = float;

void Animation::Draw(ecs::Entity entity) {
	PTGN_ASSERT(entity.Has<Animation>());
	PTGN_ASSERT(entity.Has<Transform>());
	const auto& anim{ entity.Get<Animation>() };
	const auto& transform{ entity.Get<Transform>() };
	const auto& source{ anim.GetSource() };
	game.renderer.DrawTexture(
		anim.texture, transform.position + anim.draw_offset, anim.sprite_size * transform.scale,
		source.pos, source.size, source.origin,
		entity.Has<SpriteFlip>() ? entity.Get<SpriteFlip>() : Flip::None, transform.rotation,
		V2_float{ 0.5f, 0.5f }, entity.Has<SpriteZ>() ? entity.Get<SpriteZ>() : 0.0f,
		entity.Has<SpriteTint>() ? entity.Get<SpriteTint>() : color::White
	);
}

} // namespace ptgn