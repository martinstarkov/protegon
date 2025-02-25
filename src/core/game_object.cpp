#include "core/game_object.h"

#include <array>
#include <utility>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/render_target.h"
#include "renderer/text.h"
#include "renderer/texture.h"

namespace ptgn {

bool IsVisible(const ecs::Entity& e) {
	return e.Has<Visible>() ? e.Get<Visible>() : Visible{ false };
}

bool IsEnabled(const ecs::Entity& e) {
	return e.Has<Enabled>() ? e.Get<Enabled>() : Enabled{ false };
}

V2_float GetLocalPosition(const ecs::Entity& e) {
	return e.Has<Transform>() ? e.Get<Transform>().position : V2_float{};
}

V2_float GetPosition(const ecs::Entity& e) {
	return GetLocalPosition(e) + (HasParent(e) ? GetPosition(GetParent(e)) : V2_float{});
}

float GetLocalRotation(const ecs::Entity& e) {
	return e.Has<Transform>() ? e.Get<Transform>().rotation : 0.0f;
}

float GetRotation(const ecs::Entity& e) {
	return GetLocalRotation(e) + (HasParent(e) ? GetRotation(GetParent(e)) : 0.0f);
}

V2_float GetLocalScale(const ecs::Entity& e) {
	return e.Has<Transform>() ? e.Get<Transform>().scale : V2_float{ 1.0f, 1.0f };
}

V2_float GetScale(const ecs::Entity& e) {
	return GetLocalScale(e) * (HasParent(e) ? GetScale(GetParent(e)) : V2_float{ 1.0f, 1.0f });
}

Depth GetDepth(const ecs::Entity& e) {
	return e.Has<Depth>() ? e.Get<Depth>() : Depth{};
}

BlendMode GetBlendMode(const ecs::Entity& e) {
	return e.Has<BlendMode>() ? e.Get<BlendMode>() : BlendMode::Blend;
}

Origin GetOrigin(const ecs::Entity& e) {
	return e.Has<Origin>() ? e.Get<Origin>() : Origin::Center;
}

Color GetTint(const ecs::Entity& e) {
	return e.Has<Tint>() ? e.Get<Tint>() : Tint{};
}

ecs::Entity GetParent(const ecs::Entity& e) {
	return HasParent(e) ? e.Get<ecs::Entity>() : e;
}

bool HasParent(const ecs::Entity& e) {
	return e.Has<ecs::Entity>();
}

V2_float GetRotationCenter(const ecs::Entity& e) {
	return e.Has<RotationCenter>() ? e.Get<RotationCenter>() : RotationCenter{};
}

std::array<V2_float, 4> GetTextureCoordinates(const ecs::Entity& e, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	V2_int texture_size;

	if (e.Has<TextureKey>()) {
		texture_size = game.texture.GetSize(e.Get<TextureKey>());
	} else if (e.Has<Text>()) {
		texture_size = e.Get<Text>().GetTexture().GetSize();
	} else if (e.Has<RenderTarget>()) {
		texture_size = e.Get<RenderTarget>().GetTexture().GetSize();
	}

	if (texture_size.IsZero()) {
		return tex_coords;
	}

	if (e.Has<TextureCrop>()) {
		const auto& crop{ e.Get<TextureCrop>() };
		if (crop != TextureCrop{}) {
			tex_coords =
				impl::GetTextureCoordinates(crop.GetPosition(), crop.GetSize(), texture_size);
		}
	}

	auto scale{ GetScale(e) };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	if (e.Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, e.Get<Flip>());
	}

	if (flip_vertically) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	return tex_coords;
}

GameObject::GameObject(const ecs::Entity& e) : entity{ e } {}

GameObject::~GameObject() {
	entity.Destroy();
}

bool GameObject::operator==(const ecs::Entity& o) const {
	return entity == o;
}

bool GameObject::operator!=(const ecs::Entity& o) const {
	return !(*this == o);
}

GameObject::operator ecs::Entity() const {
	return entity;
}

GameObject& GameObject::SetVisible(bool visible) {
	if (visible) {
		entity.Add<Visible>();
	} else {
		entity.Remove<Visible>();
	}
	return *this;
}

GameObject& GameObject::Show() {
	return SetVisible(true);
}

GameObject& GameObject::Hide() {
	return SetVisible(false);
}

GameObject& GameObject::SetEnabled(bool enabled) {
	if (enabled) {
		entity.Add<Enabled>();
	} else {
		entity.Remove<Enabled>();
	}
	return *this;
}

GameObject& GameObject::Disable() {
	return SetEnabled(false);
}

GameObject& GameObject::Enable() {
	return SetEnabled(true);
}

bool GameObject::IsVisible() const {
	return ptgn::IsVisible(entity);
}

bool GameObject::IsEnabled() const {
	return ptgn::IsEnabled(entity);
}

V2_float GameObject::GetLocalPosition() const {
	return ptgn::GetLocalPosition(entity);
}

V2_float GameObject::GetPosition() const {
	return ptgn::GetPosition(entity);
}

float GameObject::GetLocalRotation() const {
	return ptgn::GetLocalRotation(entity);
}

float GameObject::GetRotation() const {
	return ptgn::GetRotation(entity);
}

V2_float GameObject::GetLocalScale() const {
	return ptgn::GetLocalScale(entity);
}

V2_float GameObject::GetScale() const {
	return ptgn::GetScale(entity);
}

Depth GameObject::GetDepth() const {
	return ptgn::GetDepth(entity);
}

BlendMode GameObject::GetBlendMode() const {
	return ptgn::GetBlendMode(entity);
}

ecs::Entity GameObject::GetParent() const {
	return ptgn::GetParent(entity);
}

bool GameObject::HasParent() const {
	return ptgn::HasParent(entity);
}

V2_float GameObject::GetRotationCenter() const {
	return ptgn::GetRotationCenter(entity);
}

std::array<V2_float, 4> GameObject::GetTextureCoordinates(bool flip_vertically) const {
	return ptgn::GetTextureCoordinates(entity, flip_vertically);
}

GameObject& GameObject::SetPosition(const V2_float& position) {
	if (entity.Has<Transform>()) {
		entity.Get<Transform>().position = position;
	} else {
		entity.Add<Transform>(position);
	}
	return *this;
}

GameObject& GameObject::SetRotation(float rotation) {
	if (entity.Has<Transform>()) {
		entity.Get<Transform>().rotation = rotation;
	} else {
		entity.Add<Transform>(V2_float{}, rotation);
	}
	return *this;
}

GameObject& GameObject::SetScale(const V2_float& scale) {
	if (entity.Has<Transform>()) {
		entity.Get<Transform>().scale = scale;
	} else {
		entity.Add<Transform>(V2_float{}, 0.0f, scale);
	}
	return *this;
}

GameObject& GameObject::SetDepth(const Depth& depth) {
	if (entity.Has<Depth>()) {
		entity.Get<Depth>() = depth;
	} else {
		entity.Add<Depth>(depth);
	}
	return *this;
}

GameObject& GameObject::SetBlendMode(BlendMode blend_mode) {
	if (entity.Has<BlendMode>()) {
		entity.Get<BlendMode>() = blend_mode;
	} else {
		entity.Add<BlendMode>(blend_mode);
	}
	return *this;
}

GameObject& GameObject::AddChild(const ecs::Entity& o) {
	if (entity.Has<Children>()) {
		entity.Get<Children>().Add(o);
	} else {
		entity.Add<Children>(o);
	}
	return *this;
}

GameObject& GameObject::RemoveChild(const ecs::Entity& o) {
	if (!entity.Has<Children>()) {
		return *this;
	}
	auto& children{ entity.Get<Children>() };
	children.Remove(o);
	if (children.IsEmpty()) {
		entity.Remove<Children>();
	}
	return *this;
}

GameObject& GameObject::SetParent(const ecs::Entity& o) {
	PTGN_ASSERT(entity != o, "Cannot add game object as its own parent");
	if (HasParent()) {
		entity.Get<ecs::Entity>() = o;
	} else {
		entity.Add<ecs::Entity>(o);
	}
	return *this;
}

Children::Children(const ecs::Entity& o) {
	children_.emplace(o);
}

void Children::Add(const ecs::Entity& o) {
	children_.emplace(o);
}

void Children::Remove(const ecs::Entity& o) {
	children_.erase(o);
}

bool Children::IsEmpty() const {
	return children_.empty();
}

} // namespace ptgn