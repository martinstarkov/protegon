#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "components/generic.h"
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
#include "utility/utility.h"

namespace ptgn {

struct SpriteTint : public ColorComponent {
	using ColorComponent::ColorComponent;
};

struct DrawColor : public ColorComponent {
	using ColorComponent::ColorComponent;
};

struct DrawLineWidth : public FloatComponent {
	using FloatComponent::FloatComponent;
};

struct SpriteFlip : public FlipComponent {
	using FlipComponent::FlipComponent;
};

inline void DrawRect(ecs::Entity entity, const Rect& rect) {
	rect.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black,
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : DrawLineWidth{ 1.0f },
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
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : DrawLineWidth{ 1.0f },
		entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{}
	);
}

inline void DrawLine(ecs::Entity entity, const Line& line) {
	line.Draw(
		entity.Has<DrawColor>() ? entity.Get<DrawColor>() : color::Black,
		entity.Has<DrawLineWidth>() ? entity.Get<DrawLineWidth>() : DrawLineWidth{ 1.0f },
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

		PTGN_ASSERT(texture.GetSize().x > 0, "Texture must have width > 0");
		PTGN_ASSERT(texture.GetSize().y > 0, "Texture must have height > 0");

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

	void Draw(ecs::Entity entity) const;

	Texture texture;

	Rect GetSource() const {
		return source;
	}

private:
	Rect source;
	V2_float draw_offset; // Offset of sprite relative to entity transform.
};

namespace impl {

// Represents a row of sprites within a texture.
struct SpriteSheet {
	SpriteSheet() = default;

	// Frames go from left to right.
	SpriteSheet(
		const Texture& texture, std::size_t frame_count, const V2_float& frame_size,
		const V2_float& start_pixel = {}
	) :
		texture{ texture }, sprite_size{ frame_size } {
		sprite_positions.reserve(frame_count);
		for (std::size_t i = 0; i < frame_count; i++) {
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

inline void DrawTexture(
	ecs::Entity entity, const Texture& texture, const V2_float& draw_offset, const Rect& source
) {
	PTGN_ASSERT(entity.Has<Transform>(), "Cannot draw entity with no transform component");
	const auto& t{ entity.Get<Transform>() };
	// Absolute value needed because scale can be negative for flipping.
	V2_float scaled_size{ source.size * V2_float{ FastAbs(t.scale.x), FastAbs(t.scale.y) } };
	Rect dest{ t.position + draw_offset, scaled_size, source.origin, t.rotation };
	Flip f{ Flip::None };
	bool flip_x{ t.scale.x < 0.0f };
	bool flip_y{ t.scale.y < 0.0f };
	if (flip_x && flip_y) {
		f = Flip::Both;
	} else if (flip_x) {
		f = Flip::Horizontal;
	} else if (flip_y) {
		f = Flip::Vertical;
	}
	TextureInfo info{ source.position, source.size,
					  entity.Has<SpriteFlip>() ? entity.Get<SpriteFlip>() : SpriteFlip{ f },
					  entity.Has<SpriteTint>() ? entity.Get<SpriteTint>() : color::White,
					  V2_float{ 0.5f, 0.5f } };
	texture.Draw(dest, info, entity.Has<LayerInfo>() ? entity.Get<LayerInfo>() : LayerInfo{});
}

} // namespace impl

// TODO: Maybe just inherit from tween as well.
// Represents an animated row of sprites within a texture.
struct Animation : public impl::SpriteSheet {
	Animation() = default;

	Animation(Animation&&)				   = default;
	Animation& operator=(Animation&&)	   = default;
	Animation(const Animation&)			   = default;
	Animation& operator=(const Animation&) = default;

	// TODO: Make animation info struct.
	// @param frame_size Size of an individual animation frame (single sprite).
	// @param origin Relative to what the draw offset is
	Animation(
		const Texture& texture, std::size_t frame_count, const V2_float& frame_size,
		milliseconds duration, const V2_float& start_pixel = {}, const V2_float& draw_offset = {},
		Origin origin = Origin::Center, std::size_t start_frame = 0
	) :
		impl::SpriteSheet{ texture, frame_count, frame_size, start_pixel } {
		this->duration	  = duration;
		this->draw_offset = draw_offset;
		this->origin	  = origin;
		this->start_frame = start_frame;
		PTGN_ASSERT(
			start_frame < GetCount(), "Start frame must be within sprite sheet frame count"
		);
		frame = std::make_shared<std::size_t>(start_frame);
	}

	~Animation() {
		game.tween.Remove(tween);
	}

	void Pause() {
		if (tween.IsValid()) {
			tween.Pause();
		}
	}

	void Resume() {
		if (tween.IsValid()) {
			tween.Resume();
		}
	}

	void Reset() {
		if (tween.IsValid()) {
			tween.Reset();
		}
	}

	void Stop() {
		if (tween.IsValid()) {
			tween.Stop();
		}
	}

	[[nodiscard]] bool IsRunning() const {
		return tween.IsRunning();
	}

	[[nodiscard]] bool IsStarted() const {
		return tween.IsStarted();
	}

	[[nodiscard]] bool IsPaused() const {
		return tween.IsPaused();
	}

	void Start() {
		std::size_t start{ this->start_frame };
		std::size_t frame_count{ GetCount() };
		milliseconds frame_duration{ duration / frame_count };
		tween = game.tween.Add(frame_duration)
					.Repeat(-1)
					.OnStart([=]() { Invoke(on_start); })
					.OnRepeat([=]() {
						Invoke(on_repeat);
						auto& f{ *frame };
						f = Mod(++f, frame_count);
					})
					.OnReset([=]() {
						auto& f{ *frame };
						f = start;
					})
					.OnUpdate([=](float t) { Invoke(on_update, t); })
					.Start();
	}

	void Toggle() {
		if (tween.IsStarted()) {
			Stop();
		} else {
			Start();
		}
	}

	void Draw(ecs::Entity entity) const;

	std::function<void()> on_start;
	std::function<void()> on_repeat;
	std::function<void(float)> on_update;

	[[nodiscard]] std::size_t GetCurrentFrame() const {
		PTGN_ASSERT(frame != nullptr, "Cannot retrieve current frame of uninitialized animation");
		std::size_t f{ *frame };
		PTGN_ASSERT(f < GetCount(), "Frame outside of animation sprite count");
		return f;
	}

	Rect GetSource() const {
		return Rect{ sprite_positions[GetCurrentFrame()], sprite_size, origin };
	}

private:
	Tween tween;

	Origin origin{ Origin::Center }; // Which origin of the sprite the draw offset is relative to.

	V2_float draw_offset;			 // Offset of sprite relative to entity transform.

	milliseconds duration{ 0 };		 // Duration of the entire animation.

	std::size_t start_frame{ 0 };

	std::shared_ptr<std::size_t> frame;
};

struct AnimationMap : public ActiveMapManager<Animation> {
public:
	using ActiveMapManager::ActiveMapManager;
	AnimationMap()									 = delete;
	~AnimationMap() override						 = default;
	AnimationMap(AnimationMap&&) noexcept			 = default;
	AnimationMap& operator=(AnimationMap&&) noexcept = default;
	AnimationMap(const AnimationMap&)				 = delete;
	AnimationMap& operator=(const AnimationMap&)	 = delete;

	// If the provided key is a not currently active, this function pauses the previously active
	// animation.
	void SetActive(const ActiveMapManager::Key& key) {
		// Key already active, do nothing.
		if (auto internal_key{ GetInternalKey(key) }; internal_key == active_key_) {
			return;
		}
		GetActive().Pause();
		ActiveMapManager::SetActive(key);
	}

	void Draw(ecs::Entity entity) const;
};

inline void Sprite::Draw(ecs::Entity entity) const {
	impl::DrawTexture(entity, texture, draw_offset, source);
}

inline void Animation::Draw(ecs::Entity entity) const {
	impl::DrawTexture(entity, texture, draw_offset, GetSource());
}

inline void AnimationMap::Draw(ecs::Entity entity) const {
	GetActive().Draw(entity);
}

} // namespace ptgn