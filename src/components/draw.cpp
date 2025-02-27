#include "components/draw.h"

#include <string_view>
#include <type_traits>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/time.h"
#include "utility/tween.h"
#include "utility/utility.h"

namespace ptgn {

ecs::Entity CreateSprite(ecs::Manager& manager, std::string_view texture_key) {
	PTGN_ASSERT(
		game.texture.Has(texture_key), "Sprite texture key must be loaded in the texture manager"
	);

	auto entity{ manager.CreateEntity() };

	entity.Add<TextureKey>(Hash(texture_key));
	entity.Add<Visible>();

	return entity;
}

ecs::Entity CreateAnimation(
	ecs::Manager& manager, std::string_view texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, const V2_float& start_pixel,
	std::size_t start_frame
) {
	PTGN_ASSERT(start_frame < frame_count, "Start frame must be within animation frame count");

	PTGN_ASSERT(
		game.texture.Has(texture_key), "Animation texture key must be loaded in the texture manager"
	);

	auto entity{ manager.CreateEntity() };

	entity.Add<TextureKey>(Hash(texture_key));
	entity.Add<Visible>();
	entity.Add<TextureCrop>();
	entity.Add<impl::AnimationInfo>(frame_count, frame_size, start_pixel, start_frame);

	milliseconds frame_duration{ animation_duration / frame_count };

	auto update_crop = [](TextureCrop& crop, const impl::AnimationInfo& anim) {
		crop.SetPosition(anim.GetCurrentFramePosition());
		crop.SetSize(anim.GetFrameSize());
	};

	// TODO: Consider breaking this up into individual tween points using a for loop.
	entity.Add<Tween>()
		.During(frame_duration)
		.Repeat(-1)
		.OnStart([=]() mutable {
			auto [anim, crop] = entity.Get<impl::AnimationInfo, TextureCrop>();
			anim.ResetToStartFrame();
			std::invoke(update_crop, crop, anim);
			if (entity.Has<callback::AnimationStart>()) {
				Invoke(entity.Get<callback::AnimationStart>());
			}
		})
		.OnRepeat([=]() mutable {
			auto [anim, crop] = entity.Get<impl::AnimationInfo, TextureCrop>();
			anim.IncrementFrame();
			std::invoke(update_crop, crop, anim);
			if (entity.Has<callback::AnimationRepeat>() &&
				anim.GetFrameRepeats() % anim.GetFrameCount() == 0) {
				Invoke(entity.Get<callback::AnimationRepeat>());
			}
		})
		.OnReset([=]() mutable {
			auto [anim, crop] = entity.Get<impl::AnimationInfo, TextureCrop>();
			anim.ResetToStartFrame();
			std::invoke(update_crop, crop, anim);
		});

	return entity;
}

void TextureCrop::SetSize(const V2_float& size) {
	size_ = size;
}

V2_float TextureCrop::GetSize() const {
	return size_;
}

void TextureCrop::SetPosition(const V2_float& position) {
	position_ = position;
}

V2_float TextureCrop::GetPosition() const {
	return position_;
}

bool TextureCrop::operator==(const TextureCrop& other) const {
	return position_ == other.position_ && size_ == other.size_;
}

bool TextureCrop::operator!=(const TextureCrop& other) const {
	return !(*this == other);
}

namespace impl {

AnimationInfo::AnimationInfo(
	std::size_t frame_count, const V2_float& frame_size, const V2_float& start_pixel,
	std::size_t start_frame
) :
	frame_count_{ frame_count },
	frame_size_{ frame_size },
	start_pixel_{ start_pixel },
	start_frame_{ start_frame } {}

std::size_t AnimationInfo::GetSequenceRepeats() const {
	return frame_repeats_ / frame_count_;
}

std::size_t AnimationInfo::GetFrameRepeats() const {
	return frame_repeats_;
}

std::size_t AnimationInfo::GetFrameCount() const {
	return frame_count_;
}

void AnimationInfo::SetCurrentFrame(std::size_t new_frame) {
	current_frame_ = Mod(new_frame, frame_count_);
}

void AnimationInfo::IncrementFrame() {
	SetCurrentFrame(++current_frame_);
}

void AnimationInfo::ResetToStartFrame() {
	current_frame_ = start_frame_;
}

std::size_t AnimationInfo::GetCurrentFrame() const {
	return current_frame_;
}

V2_float AnimationInfo::GetCurrentFramePosition() const {
	return { start_pixel_.x + frame_size_.x * current_frame_, start_pixel_.y };
}

V2_float AnimationInfo::GetFrameSize() const {
	return frame_size_;
}

std::size_t AnimationInfo::GetStartFrame() const {
	return start_frame_;
}

} // namespace impl

} // namespace ptgn