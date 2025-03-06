#include "core/game_object.h"

#include <array>
#include <type_traits>

#include "components/draw.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "utility/assert.h"
#include "utility/utility.h"

namespace ptgn {

namespace impl {

Transform* GetAbsoluteTransformImpl(const ecs::Entity& e) {
	Transform* transform{ nullptr };
	if (HasParent(e)) {
		transform = impl::GetAbsoluteTransformImpl(GetParent(e));
	}
	if (transform == nullptr && e.Has<Transform>()) {
		transform = &e.Get<Transform>();
	}
	return transform;
}

} // namespace impl

Transform& GetAbsoluteTransform(const ecs::Entity& e) {
	auto transform{ impl::GetAbsoluteTransformImpl(e) };
	PTGN_ASSERT(
		transform != nullptr, "Game object does not have an absolute, i.e. neither this entity nor "
							  "any of its parents have a transform component"
	);
	return *transform;
}

bool IsVisible(const ecs::Entity& e) {
	return e.Has<Visible>() ? e.Get<Visible>() : Visible{ false };
}

bool IsEnabled(const ecs::Entity& e) {
	return e.Has<Enabled>() ? e.Get<Enabled>() : Enabled{ false };
}

Transform GetLocalTransform(const ecs::Entity& e) {
	return e.Has<Transform>() ? e.Get<Transform>() : Transform{};
}

Transform GetTransform(const ecs::Entity& e) {
	return GetLocalTransform(e).RelativeTo(HasParent(e) ? GetTransform(GetParent(e)) : Transform{});
}

V2_float GetLocalPosition(const ecs::Entity& e) {
	return e.Has<Transform>() ? e.Get<Transform>().position : V2_float{};
}

V2_float GetPosition(const ecs::Entity& e) {
	return GetLocalPosition(e) + (HasParent(e) ? GetPosition(GetParent(e)) : V2_float{});
}

Transform GetLocalOffsetTransform(const ecs::Entity& e) {
	return e.Has<impl::Offsets>() ? e.Get<impl::Offsets>().GetTotal() : Transform{};
}

Transform GetOffsetTransform(const ecs::Entity& e) {
	return GetLocalOffsetTransform(e).RelativeTo(
		HasParent(e) ? GetOffsetTransform(GetParent(e)) : Transform{}
	);
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
	Depth parent_depth{};
	if (HasParent(e)) {
		auto parent{ GetParent(e) };
		if (parent.Has<Depth>()) {
			parent_depth = parent.Get<Depth>();
		}
	}
	return parent_depth + (e.Has<Depth>() ? e.Get<Depth>() : Depth{});
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

bool IsImmovable(const ecs::Entity& e) {
	return e.Has<RigidBody>() && e.Get<RigidBody>().immovable ||
		   (HasParent(e) ? IsImmovable(GetParent(e)) : false);
}

std::array<V2_float, 4> GetTextureCoordinates(const ecs::Entity& e, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	if (!e.IsAlive()) {
		return tex_coords;
	}

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

GameObject::GameObject(ecs::Entity&& entity) : ecs::Entity{ std::move(entity) } {}

GameObject::GameObject(ecs::Manager& manager) : ecs::Entity{ manager.CreateEntity() } {}

GameObject::~GameObject() {
	Destroy();
}

GameObject::operator ecs::Entity() const {
	return *this;
}

ecs::Entity GameObject::GetEntity() const {
	return *this;
}

GameObject& GameObject::SetVisible(bool visible) {
	bool was_visible{ Has<Enabled>() && Get<Enabled>() };
	if (visible) {
		if (!was_visible) {
			Invoke<callback::Show>(GetEntity());
		}
		Add<Visible>();
	} else {
		if (was_visible) {
			Invoke<callback::Hide>(GetEntity());
		}
		Remove<Visible>();
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
	bool was_enabled{ Has<Enabled>() && Get<Enabled>() };
	if (enabled) {
		if (!was_enabled) {
			Invoke<callback::Enable>(GetEntity());
		}
		Add<Enabled>();
	} else {
		if (was_enabled) {
			Invoke<callback::Disable>(GetEntity());
		}
		Remove<Enabled>();
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
	return ptgn::IsVisible(*this);
}

bool GameObject::IsEnabled() const {
	return ptgn::IsEnabled(*this);
}

Transform GameObject::GetLocalTransform() const {
	return ptgn::GetLocalTransform(*this);
}

Transform GameObject::GetTransform() const {
	return ptgn::GetTransform(*this);
}

V2_float GameObject::GetLocalPosition() const {
	return ptgn::GetLocalPosition(*this);
}

V2_float GameObject::GetPosition() const {
	return ptgn::GetPosition(*this);
}

float GameObject::GetLocalRotation() const {
	return ptgn::GetLocalRotation(*this);
}

float GameObject::GetRotation() const {
	return ptgn::GetRotation(*this);
}

V2_float GameObject::GetLocalScale() const {
	return ptgn::GetLocalScale(*this);
}

V2_float GameObject::GetScale() const {
	return ptgn::GetScale(*this);
}

Depth GameObject::GetDepth() const {
	return ptgn::GetDepth(*this);
}

BlendMode GameObject::GetBlendMode() const {
	return ptgn::GetBlendMode(*this);
}

ecs::Entity GameObject::GetParent() const {
	return ptgn::GetParent(*this);
}

bool GameObject::HasParent() const {
	return ptgn::HasParent(*this);
}

GameObject& GameObject::SetTint(const Color& color) {
	if (color != Tint{}) {
		Add<Tint>(color);
	} else {
		Remove<Tint>();
	}
	return *this;
}

[[nodiscard]] Color GameObject::GetTint() const {
	return ptgn::GetTint(*this);
}

std::array<V2_float, 4> GameObject::GetTextureCoordinates(bool flip_vertically) const {
	return ptgn::GetTextureCoordinates(*this, flip_vertically);
}

GameObject& GameObject::SetPosition(const V2_float& position) {
	if (Has<Transform>()) {
		Get<Transform>().position = position;
	} else {
		Add<Transform>(position);
	}
	return *this;
}

GameObject& GameObject::SetRotation(float rotation) {
	if (Has<Transform>()) {
		Get<Transform>().rotation = rotation;
	} else {
		Add<Transform>(V2_float{}, rotation);
	}
	return *this;
}

GameObject& GameObject::SetScale(const V2_float& scale) {
	if (Has<Transform>()) {
		Get<Transform>().scale = scale;
	} else {
		Add<Transform>(V2_float{}, 0.0f, scale);
	}
	return *this;
}

GameObject& GameObject::SetDepth(const Depth& depth) {
	if (Has<Depth>()) {
		Get<Depth>() = depth;
	} else {
		Add<Depth>(depth);
	}
	return *this;
}

GameObject& GameObject::SetBlendMode(BlendMode blend_mode) {
	if (Has<BlendMode>()) {
		Get<BlendMode>() = blend_mode;
	} else {
		Add<BlendMode>(blend_mode);
	}
	return *this;
}

GameObject& GameObject::AddChild(const ecs::Entity& o) {
	if (Has<Children>()) {
		Get<Children>().Add(o);
	} else {
		Add<Children>(o);
	}
	return *this;
}

GameObject& GameObject::RemoveChild(const ecs::Entity& o) {
	if (!Has<Children>()) {
		return *this;
	}
	auto& children{ Get<Children>() };
	children.Remove(o);
	if (children.IsEmpty()) {
		Remove<Children>();
	}
	return *this;
}

GameObject& GameObject::SetParent(const ecs::Entity& o) {
	PTGN_ASSERT(*this != o, "Cannot add game object as its own parent");
	PTGN_ASSERT(o != ecs::Entity{}, "Cannot add null game object as its own parent");
	if (HasParent()) {
		Get<ecs::Entity>() = o;
	} else {
		Add<ecs::Entity>(o);
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