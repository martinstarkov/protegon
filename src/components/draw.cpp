#include "components/draw.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/resource_manager.h"
#include "core/time.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/api/flip.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

Sprite CreateSprite(Scene& scene, const TextureHandle& texture_key) {
	Sprite sprite{ scene.CreateEntity() };
	sprite.SetDraw<Sprite>();
	sprite.SetTextureKey(texture_key);
	sprite.Show();
	return sprite;
}

Animation CreateAnimation(
	Scene& scene, const TextureHandle& texture_key, milliseconds animation_duration,
	std::size_t frame_count, const V2_int& frame_size, std::int64_t play_count,
	const V2_int& start_pixel
) {
	PTGN_ASSERT(
		play_count == -1 || play_count >= 0,
		"Play count must be -1 (infinite) or otherwise non-negative"
	);

	Animation animation{ CreateSprite(scene, texture_key) };

	const auto& anim = animation.Add<impl::AnimationInfo>(
		animation_duration, frame_count, frame_size, play_count, start_pixel
	);
	auto& crop = animation.Add<TextureCrop>();

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.frame_size;

	return animation;
}

Sprite::Sprite(const Entity& entity) : Entity{ entity } {}

void Sprite::Draw(impl::RenderData& ctx, const Entity& entity) {
	Sprite sprite{ entity };

	const auto& texture{ sprite.GetTexture() };
	auto display_size{ sprite.GetDisplaySize() };
	auto texture_coordinates{ sprite.GetTextureCoordinates(false) };

	auto transform{ entity.GetDrawTransform() };
	auto origin{ entity.GetOrigin() };
	auto tint{ entity.GetTint() };
	auto depth{ entity.GetDepth() };

	impl::RenderState state;
	state.blend_mode  = entity.GetBlendMode();
	state.shader_pass = game.shader.Get<ShapeShader::Quad>();
	state.camera	  = entity.GetOrParentOrDefault<Camera>();
	state.post_fx	  = entity.GetOrDefault<impl::PostFX>();
	auto pre_fx{ entity.GetOrDefault<impl::PreFX>() };

	ctx.AddTexturedQuad(
		texture, transform, display_size, origin, tint, depth, texture_coordinates, state, pre_fx
	);
}

Sprite& Sprite::SetTextureKey(const TextureHandle& texture_key) {
	if (Has<TextureHandle>()) {
		Get<TextureHandle>() = texture_key;
	} else {
		Add<TextureHandle>(texture_key);
	}
	return *this;
}

const impl::Texture& Sprite::GetTexture() const {
	PTGN_ASSERT(Has<TextureHandle>(), "Every sprite must have a texture handle component");

	const auto& texture_handle{ Get<TextureHandle>() };
	return texture_handle.GetTexture(*this);
}

impl::Texture& Sprite::GetTexture() {
	return const_cast<impl::Texture&>(std::as_const(*this).GetTexture());
}

V2_int Sprite::GetTextureSize() const {
	if (Has<TextureHandle>()) {
		return Get<TextureHandle>().GetSize(*this);
	}

	PTGN_ERROR("Texture does not have a valid size");
}

V2_int Sprite::GetDisplaySize() const {
	if (Has<TextureCrop>()) {
		const auto& crop{ Get<TextureCrop>() };
		return crop.size;
	}

	return GetTextureSize();
}

std::array<V2_float, 4> Sprite::GetTextureCoordinates(bool flip_vertically) const {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}
	};

	if (!*this) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize() };

	if (texture_size.IsZero()) {
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	if (Has<TextureCrop>()) {
		const auto& crop{ Get<TextureCrop>() };
		if (crop != TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
		}
	}

	auto scale{ GetScale() };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, Get<Flip>());
	}

	std::invoke(check_vertical_flip);

	return tex_coords;
}

void Animation::Start(bool force) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	bool started{ anim.frame_timer.Start(force) };
	if (started) {
		InvokeScript<&impl::IScript::OnAnimationStart>();
	}
}

void Animation::Reset() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	PTGN_ASSERT(Has<TextureCrop>(), "Animation must have TextureCrop component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.current_frame = 0;
	anim.frames_played = 0;
	auto& crop		   = Get<TextureCrop>();
	crop.position	   = anim.GetCurrentFramePosition();
	crop.size		   = anim.frame_size;
	InvokeScript<&impl::IScript::OnAnimationStop>();
	anim.frame_timer.Reset();
}

void Animation::Stop() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	InvokeScript<&impl::IScript::OnAnimationStop>();
	anim.frame_timer.Stop();
}

void Animation::Toggle() {
	if (IsPlaying()) {
		Stop();
	} else {
		Start();
	}
}

void Animation::Pause() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	InvokeScript<&impl::IScript::OnAnimationPause>();
	anim.frame_timer.Pause();
}

void Animation::Resume() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	InvokeScript<&impl::IScript::OnAnimationResume>();
	anim.frame_timer.Resume();
}

bool Animation::IsPaused() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_timer.IsPaused();
}

bool Animation::IsPlaying() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_timer.IsRunning();
}

std::size_t Animation::GetPlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.GetPlayCount();
}

std::size_t Animation::GetFramePlayCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frames_played;
}

milliseconds Animation::GetDuration() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.duration;
}

milliseconds Animation::GetFrameDuration() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	milliseconds frame_duration{ anim.duration / anim.frame_count };
	return frame_duration;
}

std::size_t Animation::GetFrameCount() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_count;
}

void Animation::SetCurrentFrame(std::size_t new_frame) {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.SetCurrentFrame(new_frame);
}

void Animation::IncrementFrame() {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	auto& anim{ Get<impl::AnimationInfo>() };
	anim.IncrementFrame();
}

std::size_t Animation::GetCurrentFrame() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.current_frame;
}

V2_int Animation::GetCurrentFramePosition() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.GetCurrentFramePosition();
}

V2_int Animation::GetFrameSize() const {
	PTGN_ASSERT(Has<impl::AnimationInfo>(), "Animation must have AnimationInfo component");
	const auto& anim{ Get<impl::AnimationInfo>() };
	return anim.frame_size;
}

namespace impl {

AnimationInfo::AnimationInfo(
	milliseconds animation_duration, std::size_t animation_frame_count,
	const V2_float& animation_frame_size, std::int64_t animation_play_count,
	const V2_float& animation_start_pixel
) :
	duration{ animation_duration },
	frame_count{ animation_frame_count },
	frame_size{ animation_frame_size },
	play_count{ animation_play_count },
	start_pixel{ animation_start_pixel } {}

milliseconds AnimationInfo::GetFrameDuration() const {
	return duration / frame_count;
}

V2_int AnimationInfo::GetCurrentFramePosition() const {
	return { start_pixel.x + frame_size.x * static_cast<int>(current_frame), start_pixel.y };
}

std::size_t AnimationInfo::GetPlayCount() const {
	return frames_played / frame_count;
}

void AnimationInfo::SetCurrentFrame(std::size_t new_frame) {
	current_frame = new_frame % frame_count;
	frame_dirty	  = true;
}

void AnimationInfo::IncrementFrame() {
	SetCurrentFrame(current_frame + 1);
}

void AnimationSystem::Update(Scene& scene) {
	for (auto [entity, anim, crop] : scene.EntitiesWith<AnimationInfo, TextureCrop>()) {
		if (anim.frame_dirty) {
			crop.size	  = anim.frame_size;
			crop.position = anim.GetCurrentFramePosition();

			anim.frame_dirty = false;
		}

		if (anim.frame_count == 0 || anim.duration <= milliseconds{ 0 } ||
			!anim.frame_timer.IsRunning() || anim.frame_timer.IsPaused()) {
			// Timer is not active or animation has no frames / duration.
			continue;
		}

		if (bool infinite_playback{ anim.play_count == -1 };
			!infinite_playback &&
			anim.frames_played >= static_cast<std::size_t>(anim.play_count) * anim.frame_count) {
			entity.InvokeScript<&impl::IScript::OnAnimationComplete>();
			// Reset animation to start frame after it finishes.
			anim.SetCurrentFrame(0);
			entity.InvokeScript<&impl::IScript::OnAnimationFrameChange>(anim.current_frame);
			crop.size	  = anim.frame_size;
			crop.position = anim.GetCurrentFramePosition();
			anim.frame_timer.Stop();
			entity.InvokeScript<&impl::IScript::OnAnimationStop>();
			continue;
		}

		entity.InvokeScript<&impl::IScript::OnAnimationUpdate>();

		if (auto frame_duration{ anim.GetFrameDuration() };
			!anim.frame_timer.Completed(frame_duration)) {
			continue;
		}

		// Frame completed.

		anim.frames_played++;

		anim.IncrementFrame();

		entity.InvokeScript<&impl::IScript::OnAnimationFrameChange>(anim.current_frame);

		crop.size	  = anim.frame_size;
		crop.position = anim.GetCurrentFramePosition();

		if (anim.frames_played % anim.frame_count == 0) {
			entity.InvokeScript<&impl::IScript::OnAnimationRepeat>(anim.GetPlayCount());
		}

		anim.frame_timer.Start(true);
	}

	scene.Refresh();
}

} // namespace impl

Animation& AnimationMap::Load(const ActiveMapManager::Key& key, Animation&& entity, bool hide) {
	auto [it, inserted] = GetMap().try_emplace(GetInternalKey(key), std::move(entity));
	if (hide) {
		it->second.Hide();
	}
	return it->second;
}

bool AnimationMap::SetActive(const ActiveMapManager::Key& key) {
	if (auto internal_key{ GetInternalKey(key) }; internal_key == active_key_) {
		return false;
	}
	auto& active{ GetActive() };
	active.Hide();
	active.Pause();
	ActiveMapManager::SetActive(key);
	auto& new_active{ GetActive() };
	new_active.Show();
	return true;
}

} // namespace ptgn