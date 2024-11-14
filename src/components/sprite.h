#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "components/transform.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/layer_info.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

struct SpriteTint : public Color {
	using Color::Color;
	using Color::operator=;

	SpriteTint(const Color& c) : Color{ c } {}
};

struct DrawColor : public Color {
	using Color::Color;
	using Color::operator=;

	DrawColor(const Color& c) : Color{ c } {}
};

using DrawLineWidth = float;

using SpriteFlip = Flip;

inline void DrawRect(ecs::Entity entity, const Rect& rect) {
	rect.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black,
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : 1.0f,
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

inline void DrawPoint(ecs::Entity entity, const V2_float& point, float radius = 1.0f) {
	point.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black, radius,
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

inline void DrawCircle(ecs::Entity entity, const Circle& circle) {
	circle.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black,
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : 1.0f,
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

inline void DrawLine(ecs::Entity entity, const Line& line) {
	line.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black,
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : 1.0f,
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

inline void DrawTexture(
	ecs::Entity entity, const Texture& texture, const Transform& transform,
	const V2_float& draw_offset = {}, const Rect& source = {}
) {
	game.draw.Texture(
		texture,
		{ transform.position + draw_offset, source.size * transform.scale, source.origin,
		  transform.rotation },
		{ source.position, source.size,
		  entity.Has<SpriteFlip>() ? entity.Get<SpriteFlip>() : Flip::None,
		  entity.Has<SpriteTint>() ? entity.Get<SpriteTint>() : color::White,
		  V2_float{ 0.5f, 0.5f } },
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

struct Sprite {
	// sprite_size = {} results in full texture size being used.
	Sprite(
		const Texture& texture, V2_float draw_offset = {}, Origin origin = Origin::Center,
		const V2_float& sprite_size = {}, const V2_float& start_pixel = {}
	) :
		texture{ texture }, draw_offset{ draw_offset } {
		source.position = start_pixel;
		if (sprite_size.IsZero()) {
			source.size = texture.GetSize();
		} else {
			source.size = sprite_size;
		}
		source.origin = origin;

		PTGN_ASSERT(texture.GetSize().x > 0.0f, "Texture must have width > 0");
		PTGN_ASSERT(texture.GetSize().y > 0.0f, "Texture must have height > 0");

		PTGN_ASSERT(
			source.position.x < texture.GetSize().x,
			"Source position X must be within texture width"
		);
		PTGN_ASSERT(
			source.position.y < texture.GetSize().y,
			"Source position Y must be within texture height"
		);
		PTGN_ASSERT(
			source.position.x + source.size.x < texture.GetSize().x,
			"Source width must be within texture width"
		);
		PTGN_ASSERT(
			source.position.y + source.size.y < texture.GetSize().y,
			"Source height must be within texture height"
		);
	}

	void Draw(ecs::Entity entity, const Transform& transform) const;

	Texture texture;

	Rect GetSource() const {
		return source;
	}

private:
	Rect source;
	V2_float draw_offset; // Offset of sprite relative to entity transform.
};

namespace impl {

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

	[[nodiscard]] std::size_t GetCount() const {
		return sprite_positions.size();
	}

	Texture texture;
	std::vector<V2_float> sprite_positions; // Top left corners of sprites.
	V2_float sprite_size;					// Size of an individual sprite.
};

} // namespace impl

struct Animation : public impl::SpriteSheet {
	Animation() = default;

	Animation(Animation&&)				   = default;
	Animation& operator=(Animation&&)	   = default;
	Animation(const Animation&)			   = default;
	Animation& operator=(const Animation&) = default;

	// TODO: Make animation info struct.
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
			start_frame < GetCount(), "Start frame must be within sprite sheet frame count"
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
		std::size_t frame_count{ GetCount() };
		milliseconds frame_duration{ duration / frame_count };
		tween = game.tween.Add(frame_duration)
					.Repeat(-1)
					.OnStart([=]() {
						if (on_start != nullptr) {
							std::invoke(on_start);
						}
					})
					.OnRepeat([=]() {
						if (on_repeat != nullptr) {
							std::invoke(on_repeat);
						}
						auto& f{ *frame };
						f = Mod(++f, frame_count);
					})
					.OnUpdate([=](float t) {
						if (on_update != nullptr) {
							std::invoke(on_update, t);
						}
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

	void Draw(ecs::Entity entity, const Transform& transform) const;

	std::function<void()> on_start;
	std::function<void()> on_repeat;
	std::function<void(float)> on_update;

	Tween tween;

	[[nodiscard]] std::size_t GetCurrentFrame() const {
		PTGN_ASSERT(frame != nullptr, "Cannot retrieve current frame of uninitialized animation");
		std::size_t f{ *frame };
		PTGN_ASSERT(f < GetCount(), "Frame outside of animation sprite count");
		return f;
	}

	Rect GetSource() const {
		return { sprite_positions[GetCurrentFrame()], sprite_size, origin };
	}

	Origin origin{ Origin::Center };

private:
	V2_float draw_offset;		// Offset of sprite relative to entity transform.

	milliseconds duration{ 0 }; // Duration of the entire animation.

	std::shared_ptr<std::size_t> frame;
};

inline void Sprite::Draw(ecs::Entity entity, const Transform& transform) const {
	DrawTexture(entity, texture, transform, draw_offset, source);
}

inline void Animation::Draw(ecs::Entity entity, const Transform& transform) const {
	DrawTexture(entity, texture, transform, draw_offset, GetSource());
}

} // namespace ptgn