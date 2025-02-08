#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include "components/generic.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/time.h"
#include "utility/tween.h"

namespace ptgn {

// TODO: Consider moving everything to this.
namespace component {} // namespace component

// TODO: Move some components of these elsewhere.

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct Visible : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Visible() : ArithmeticComponent{ true } {}
};

struct Alpha : public ArithmeticComponent<std::uint8_t> {
	using ArithmeticComponent::ArithmeticComponent;

	Alpha() : ArithmeticComponent{ 255 } {}
};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct LineWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	LineWidth() : ArithmeticComponent{ 1.0f } {}
};

struct Radius : public Vector2Component<float> {
	using Vector2Component::Vector2Component;

	Radius() : Vector2Component{ V2_float{ 1.0f, 1.0f } } {}
};

struct Size : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct Offset : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct RotationCenter : public Vector2Component<float> {
	using Vector2Component::Vector2Component;

	RotationCenter() : Vector2Component{ V2_float{ 0.5f, 0.5f } } {}
};

struct Sprite {
	// sprite_size = {} results in full texture size being used.
	Sprite(
		std::string_view texture_key, const V2_float& sprite_size = {},
		const V2_float& start_pixel = {}
	) :
		texture_key{ Hash(texture_key) } {
		V2_int texture_size{ game.texture.GetSize(texture_key) };

		PTGN_ASSERT(texture_size.x > 0, "Texture must have width > 0");
		PTGN_ASSERT(texture_size.y > 0, "Texture must have height > 0");

		source.position = start_pixel;

		if (sprite_size.IsZero()) {
			source.size = texture_size;
		} else {
			source.size = sprite_size;
		}
		source.origin = Origin::Center;

		PTGN_ASSERT(
			source.position.x < texture_size.x, "Source position X must be within texture width"
		);
		PTGN_ASSERT(
			source.position.y < texture_size.y, "Source position Y must be within texture height"
		);
		PTGN_ASSERT(
			source.position.x + source.size.x <= texture_size.x,
			"Source width must be within texture width"
		);
		PTGN_ASSERT(
			source.position.y + source.size.y <= texture_size.y,
			"Source height must be within texture height"
		);
	}

	Rect GetSource() const {
		return source;
	}

	std::size_t texture_key;
	Rect source;
};

namespace impl {

// Represents a row of sprites within a texture.
struct SpriteSheet {
	SpriteSheet() = default;

	// Frames go from left to right.
	SpriteSheet(
		std::string_view texture_key, std::size_t frame_count, const V2_float& frame_size,
		const V2_float& start_pixel = {}
	) :
		texture_key{ Hash(texture_key) }, sprite_size{ frame_size } {
		sprite_positions.reserve(frame_count);
		for (std::size_t i = 0; i < frame_count; i++) {
			float x = start_pixel.x + frame_size.x * static_cast<float>(i);
			PTGN_ASSERT(
				x < game.texture.GetSize(texture_key).x,
				"Source position X must be within texture width"
			);
			sprite_positions.emplace_back(x, start_pixel.y);
		}
	}

	[[nodiscard]] std::size_t GetCount() const {
		return sprite_positions.size();
	}

	std::size_t texture_key;
	std::vector<V2_float> sprite_positions; // Top left corners of sprites.
	V2_float sprite_size;					// Size of an individual sprite.
};

} // namespace impl

// TODO: Maybe just inherit Animation from Tween as well as SpriteSheet.
//
// Represents an animated row of sprites within a texture.
struct Animation : public impl::SpriteSheet {
	Animation() = default;

	// TODO: Make animation info struct.
	// @param frame_size Size of an individual animation frame (single sprite).
	Animation(
		ecs::Entity parent, std::string_view texture_key, std::size_t frame_count,
		const V2_float& frame_size, milliseconds animation_duration,
		const V2_float& start_pixel = {}, std::size_t starting_frame = 0
	);

	Animation(const Animation&)			   = delete;
	Animation& operator=(const Animation&) = delete;
	Animation(Animation&& other) noexcept;
	Animation& operator=(Animation&& other) noexcept;
	~Animation();

	bool operator==(const Animation& o) const {
		return parent_ == o.parent_;
	}

	bool operator!=(const Animation& o) const {
		return !(*this == o);
	}

	void Pause() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Pause();
	}

	void Resume() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Resume();
	}

	void Reset() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Reset();
	}

	void Stop() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Stop();
	}

	[[nodiscard]] bool IsRunning() const {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		return entity_.Get<Tween>().IsRunning();
	}

	[[nodiscard]] bool IsStarted() const {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		return entity_.Get<Tween>().IsStarted();
	}

	[[nodiscard]] bool IsPaused() const {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		return entity_.Get<Tween>().IsPaused();
	}

	// Start the animation if it is not running. If running, do nothing.
	void StartIfNotRunning() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().StartIfNotRunning();
	}

	void Start() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Start();
	}

	void Toggle() {
		PTGN_ASSERT(entity_.Has<Tween>(), "Animation has not been initialized");
		entity_.Get<Tween>().Toggle();
	}

	std::function<void()> on_start;
	std::function<void()> on_repeat;
	std::function<void(float)> on_update;

	[[nodiscard]] std::size_t GetCurrentFrame() const {
		PTGN_ASSERT(current_frame < GetCount(), "Frame outside of animation sprite count");
		return current_frame;
	}

	[[nodiscard]] std::size_t GetRepeat() const {
		return repeat;
	}

	[[nodiscard]] Rect GetSource() const {
		return Rect{ sprite_positions[GetCurrentFrame()], sprite_size, Origin::Center };
	}

	milliseconds duration{ 0 }; // Duration of the entire animation.

	std::size_t repeat{ 0 };
	std::size_t current_frame{ 0 };
	std::size_t start_frame{ 0 };

private:
	ecs::Entity parent_;
	ecs::Entity entity_;
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
	// animation. If the key is already active, does nothing.
	// @return True if active value changed, false otherwise.
	bool SetActive(const ActiveMapManager::Key& key) {
		if (auto internal_key{ GetInternalKey(key) }; internal_key == active_key_) {
			return false;
		}
		GetActive().Pause();
		ActiveMapManager::SetActive(key);
		return true;
	}
};

} // namespace ptgn