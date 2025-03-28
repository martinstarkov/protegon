#pragma once

#include <string_view>

#include "components/generic.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "utility/time.h"

namespace ptgn {

// @param manager Which manager the entity is added to.
// @param texture_key Key of the texture loaded into the texture manager.
ecs::Entity CreateSprite(ecs::Manager& manager, std::string_view texture_key);

// @param manager Which manager the entity is added to.
// @param texture_key Key of the texture loaded into the texture manager.
// @param frame_count Number of frames in the animation sequence.
// @param frame_size Pixel size of an individual animation frame within the texture.
// @param animation_duration Duration of the full animation sequence.
// @param start_pixel Pixel within the texture which indicates the top left position of the
// animation sequence. 2param start_frame Frame on which the animation starts / restarts to.
ecs::Entity CreateAnimation(
	ecs::Manager& manager, std::string_view texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, const V2_float& start_pixel = {},
	std::size_t start_frame = 0
);

struct Visible : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	Visible() : ArithmeticComponent{ true } {}
};

struct DisplaySize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct LineWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	LineWidth() : ArithmeticComponent{ 1.0f } {}
};

struct TextureKey : public ArithmeticComponent<std::size_t> {
	using ArithmeticComponent::ArithmeticComponent;

	TextureKey(std::string_view key) : ArithmeticComponent{ Hash(key) } {}

	TextureKey(const char* key) : ArithmeticComponent{ Hash(key) } {}
};

namespace callback {

// TODO: Change to scripts.
struct AnimationRepeat : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

struct AnimationStart : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

} // namespace callback

struct TextureCrop {
	void SetPosition(const V2_float& position);
	void SetSize(const V2_float& size);

	[[nodiscard]] V2_float GetPosition() const;
	[[nodiscard]] V2_float GetSize() const;

	bool operator==(const TextureCrop& other) const;
	bool operator!=(const TextureCrop& other) const;

private:
	// Top left position (in pixels) within the texture from which the crop starts.
	V2_float position_;

	// Size of the crop in pixels. Zero size will use full size of texture.
	V2_float size_;
};

namespace impl {

struct AnimationInfo {
	AnimationInfo(
		std::size_t frame_count, const V2_float& frame_size, const V2_float& start_pixel,
		std::size_t start_frame
	);

	// @return The number of repeats of the full animation sequence so far.
	[[nodiscard]] std::size_t GetSequenceRepeats() const;

	// @return The total number of repeats of individual animation frames so far.
	[[nodiscard]] std::size_t GetFrameRepeats() const;

	[[nodiscard]] std::size_t GetFrameCount() const;

	// new_frame is wrapped around frame_count using Mod().
	void SetCurrentFrame(std::size_t new_frame);

	void IncrementFrame();

	void ResetToStartFrame();

	[[nodiscard]] std::size_t GetCurrentFrame() const;

	[[nodiscard]] V2_float GetCurrentFramePosition() const;

	[[nodiscard]] V2_float GetFrameSize() const;

	[[nodiscard]] std::size_t GetStartFrame() const;

private:
	// Number of frames in the animation.
	std::size_t frame_count_{ 0 };

	// Size of an individual animation frame.
	V2_float frame_size_;

	// Pixel within the texture which indicates the top left position of the animation sequence.
	V2_float start_pixel_;

	// Starting frame of the animation.
	std::size_t start_frame_{ 0 };

	// Current frame of the animation.
	std::size_t current_frame_{ 0 };

	// Number of frames the animation has gone through. frame_repeats_ / frame_count_ gives the
	// number of repeats of the full animation sequence.
	std::size_t frame_repeats_{ 0 };
};

} // namespace impl

/*
// TODO: Fix.
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
*/

} // namespace ptgn