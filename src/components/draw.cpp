#include "components/draw.h"

#include <string_view>
#include <type_traits>
#include <unordered_map>

#include "common/assert.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/tween.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/texture.h"

namespace ptgn {

template <typename T, typename... TArgs>
inline Entity& AddOrRemove(Entity& e, bool condition, TArgs&&... args) {
	if (condition) {
		e.Add<T>(std::forward<TArgs>(args)...);
	} else {
		e.Remove<T>();
	}
	return e;
}

template <typename T>
inline T GetOrDefault(const Entity& e, TArgs&&... args) {
	return e.Has<T>() ? e.Get<T>() : T{ std::forward<TArgs>(args)... };
}

template <typename T>
inline T GetOrParentOrDefault(const Entity& e, TArgs&&... args) {
	return e.Has<T>()
			 ? e.Get<T>()
			 : (e.HasParent() ? e.GetParent().GetOrDefault<T>(e, std::forward<TArgs>(args)...)
							  : T{ std::forward<TArgs>(args)... });
}

Entity& Entity::SetVisible(bool visible) {
	return AddOrRemove<Visible>(*this, visible);
}

Entity& Entity::Show() {
	return SetVisible(true);
}

Entity& Entity::Hide() {
	return SetVisible(false);
}

bool Entity::IsVisible() const {
	return GetOrParentOrDefault<Visible>(*this, false);
}

Depth Entity::GetDepth() const {
	return GetOrDefault<Depth>(*this);
}

BlendMode Entity::GetBlendMode() const {
	return GetOrDefault<BlendMode>(*this, BlendMode::Blend);
}

Origin Entity::GetBlendMode() const {
	return GetOrDefault<Origin>(*this, Origin::Center);
}

Color Entity::GetTint() const {
	return GetOrDefault<Tint>(*this);
}

std::size_t Entity::GetHash() const {
	return std::hash<ecs::Entity>()(*this);
}

bool Entity::IsImmovable() const {
	return (Has<RigidBody>() && Get<RigidBody>().immovable) ||
		   (HasParent() ? GetParent().IsImmovable() : false);
}

Entity& Entity::SetTint(const Color& color) {
	return AddOrRemove(*this, color != Tint{}, color);
}

V2_int Entity::GetTextureSize() const {
	return Has<TextureHandle>() ? Get<TextureHandle>().GetSize(*this) : V2_int{};
}

std::array<V2_float, 4> Entity::GetTextureCoordinates(bool flip_vertically) const {
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

Entity& Entity::SetDepth(const Depth& depth) {
	if (Has<Depth>()) {
		Get<Depth>() = depth;
	} else {
		Add<Depth>(depth);
	}
	return *this;
}

Entity& Entity::SetBlendMode(BlendMode blend_mode) {
	if (Has<BlendMode>()) {
		Get<BlendMode>() = blend_mode;
	} else {
		Add<BlendMode>(blend_mode);
	}
	return *this;
}

Entity& Entity::SetOrigin(Origin origin) {
	if (Has<Origin>()) {
		Get<Origin>() = origin;
	} else {
		Add<Origin>(origin);
	}
	return *this;
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
	V2_float frame_pos{ start_pixel_.x + frame_size_.x * current_frame_, start_pixel_.y };
	return frame_pos;
}

V2_float AnimationInfo::GetFrameSize() const {
	return frame_size_;
}

std::size_t AnimationInfo::GetStartFrame() const {
	return start_frame_;
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
	active.Get<Tween>().Pause();
	ActiveMapManager::SetActive(key);
	auto& new_active{ GetActive() };
	new_active.Show();
	return true;
}

Sprite::Sprite(Manager& manager, const TextureHandle& texture_key) : GameObject{ manager } {
	SetDraw<Sprite>();

	PTGN_ASSERT(
		game.texture.Has(texture_key), "Sprite texture key must be loaded in the texture manager"
	);

	Add<TextureHandle>(texture_key);
	Add<Visible>();
}

void Sprite::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto transform{ entity.GetAbsoluteTransform() };
	auto depth{ entity.GetDepth() };
	auto blend_mode{ entity.GetBlendMode() };
	auto tint{ entity.GetTint().Normalized() };
	auto origin{ entity.GetOrigin() };

	PTGN_ASSERT(entity.Has<TextureHandle>());

	const auto& texture_key{ entity.Get<TextureHandle>() };
	const auto& texture{ game.texture.Get(texture_key) };
	auto size{ entity.GetSize() };
	auto coords{ entity.GetTextureCoordinates(false) };
	auto vertices{ impl::GetVertices(transform, size, origin) };

	ctx.AddTexturedQuad(vertices, coords, texture, depth, blend_mode, tint, false);
}

Animation::Animation(
	Manager& manager, std::string_view texture_key, std::size_t frame_count,
	const V2_float& frame_size, milliseconds animation_duration, std::int64_t repeats,
	const V2_float& start_pixel, std::size_t start_frame
) :
	Sprite{ manager, texture_key } {
	PTGN_ASSERT(start_frame < frame_count, "Start frame must be within animation frame count");

	auto& crop = Add<TextureCrop>();
	auto& anim = Add<impl::AnimationInfo>(frame_count, frame_size, start_pixel, start_frame);

	crop.position = anim.GetCurrentFramePosition();
	crop.size	  = anim.GetFrameSize();

	milliseconds frame_duration{ animation_duration / frame_count };

	std::int64_t frame_repeats{ repeats == -1 ? -1
											  : repeats * static_cast<std::int64_t>(frame_count) };

	// TODO: Consider breaking this up into individual tween points using a for loop.
	// TODO: Switch to using a system.
	Add<Tween>()
		.During(frame_duration)
		.Repeat(frame_repeats)
		.OnStart([entity = GetEntity()]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			Invoke<callback::AnimationStart>(entity, entity);
		})
		.OnRepeat([entity = GetEntity()](Tween& tween) mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.IncrementFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
			auto anim_frame_count{ a.GetFrameCount() };
			auto tween_repeats{ tween.GetRepeats() };
			if (tween_repeats != -1 &&
				static_cast<std::size_t>(tween_repeats) == anim_frame_count) {
				Invoke<callback::AnimationComplete>(entity, entity);
			} else if (a.GetFrameRepeats() % anim_frame_count == 0) {
				Invoke<callback::AnimationRepeat>(entity, entity);
			}
		})
		.OnReset([entity = GetEntity()]() mutable {
			auto [a, c] = entity.Get<impl::AnimationInfo, TextureCrop>();
			a.ResetToStartFrame();
			c.position = a.GetCurrentFramePosition();
			c.size	   = a.GetFrameSize();
		});
}

void Animation::Draw(impl::RenderData& ctx, const Entity& entity) {
	Sprite::Draw(ctx, entity);
}

} // namespace ptgn