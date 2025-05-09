#include "core/entity.h"

#include <array>
#include <functional>
#include <utility>

#include "common/assert.h"
#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/game.h"
#include "core/manager.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "physics/rigid_body.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/text.h"
#include "rendering/resources/texture.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "utility/span.h"

namespace ptgn {

void Entity::Clear() const {
	ecs::Entity::Clear();
}

bool Entity::IsAlive() const {
	return ecs::Entity::IsAlive();
}

void Entity::Destroy() {
	ecs::Entity::Destroy();
}

Manager& Entity::GetManager() {
	return static_cast<Manager&>(ecs::Entity::GetManager());
}

const Manager& Entity::GetManager() const {
	return static_cast<const Manager&>(ecs::Entity::GetManager());
}

bool Entity::IsIdenticalTo(const Entity& e) const {
	return ecs::Entity::IsIdenticalTo(e);
}

bool Entity::HasDraw() const {
	return Has<IDrawable>();
}

Entity& Entity::RemoveDraw() {
	Remove<IDrawable>();
	return *this;
}

Entity& Entity::SetVisible(bool visible) {
	bool was_visible{ Has<Enabled>() && Get<Enabled>() };
	if (visible) {
		if (!was_visible) {
			Invoke<callback::Show>(*this);
		}
		Add<Visible>();
	} else {
		if (was_visible) {
			Invoke<callback::Hide>(*this);
		}
		Remove<Visible>();
	}
	return *this;
}

Entity& Entity::Show() {
	return SetVisible(true);
}

Entity& Entity::Hide() {
	return SetVisible(false);
}

Entity& Entity::SetEnabled(bool enabled) {
	bool was_enabled{ Has<Enabled>() && Get<Enabled>() };
	if (enabled) {
		if (!was_enabled) {
			Invoke<callback::Enable>(*this);
		}
		Add<Enabled>();
	} else {
		if (was_enabled) {
			Invoke<callback::Disable>(*this);
		}
		Remove<Enabled>();
	}
	return *this;
}

Entity& Entity::Disable() {
	return SetEnabled(false);
}

Entity& Entity::Enable() {
	return SetEnabled(true);
}

bool Entity::IsVisible() const {
	return Has<Visible>() ? Get<Visible>() : Visible{ false };
}

bool Entity::IsEnabled() const {
	return Has<Enabled>() ? Get<Enabled>() : Enabled{ false };
}

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>());
	return Get<UUID>();
}

float Entity::GetLowestY() const {
	const auto& transform{ GetAbsoluteTransform() };

	auto size{ GetSize() * transform.scale };

	auto center{ transform.position + GetOriginOffset(GetOrigin(), size) };

	return center.y + size.y * 0.5f;
}

V2_float Entity::GetSize() const {
	V2_float size;

	if (Has<TextureCrop>()) {
		size = Get<TextureCrop>().size;
	}
	if (Has<DisplaySize>()) {
		size = Get<DisplaySize>();
	}
	if (size.IsZero() && Has<TextureKey>()) {
		const auto& texture_key{ Get<TextureKey>() };
		const auto& texture{ game.texture.Get(texture_key) };
		size = texture.GetSize();
	}
	if (size.IsZero() && Has<Children>()) {
		const auto& children{ Get<Children>() };
		for (const auto& [name, child] : children.named_children_) {
			size = child.GetSize();
			if (!size.IsZero()) {
				break;
			}
		}
		for (const auto& child : children.children_) {
			size = child.GetSize();
			if (!size.IsZero()) {
				break;
			}
		}
	}

	return size;
}

const Transform& Entity::GetRootTransform() const {
	std::function<const Transform*(const Entity&)> get_transform;

	get_transform = [&get_transform](const Entity& e) {
		const Transform* transform{ nullptr };
		if (e.HasParent()) {
			transform = get_transform(e.GetParent());
		}
		if (transform == nullptr && e.Has<Transform>()) {
			transform = &e.Get<Transform>();
		}
		return transform;
	};

	auto transform{ get_transform(*this) };
	PTGN_ASSERT(
		transform != nullptr,
		"Game object does not have a root transform, i.e. neither this entity nor "
		"any of its parents have a transform component"
	);
	return *transform;
}

Transform& Entity::GetRootTransform() {
	return const_cast<Transform&>(std::as_const(*this).GetRootTransform());
}

Transform Entity::GetRelativeTransform() const {
	return Has<Transform>() ? Get<Transform>() : Transform{};
}

Transform Entity::GetTransform() const {
	return GetRelativeTransform().RelativeTo(
		HasParent() ? GetParent().GetTransform() : Transform{}
	);
}

Transform Entity::GetAbsoluteTransform() const {
	return GetTransform().RelativeTo(GetOffset());
}

Transform Entity::GetRelativeOffset() const {
	return Has<impl::Offsets>() ? Get<impl::Offsets>().GetTotal() : Transform{};
}

Transform Entity::GetOffset() const {
	return GetRelativeOffset().RelativeTo(
		HasParent() ? GetParent().GetRelativeOffset() : Transform{}
	);
}

V2_float Entity::GetRelativePosition() const {
	return Has<Transform>() ? Get<Transform>().position : V2_float{};
}

V2_float Entity::GetPosition() const {
	return GetRelativePosition() + (HasParent() ? GetParent().GetPosition() : V2_float{});
}

float Entity::GetRelativeRotation() const {
	return Has<Transform>() ? Get<Transform>().rotation : 0.0f;
}

float Entity::GetRotation() const {
	return GetRelativeRotation() + (HasParent() ? GetParent().GetRotation() : 0.0f);
}

V2_float Entity::GetRelativeScale() const {
	return Has<Transform>() ? Get<Transform>().scale : V2_float{ 1.0f, 1.0f };
}

V2_float Entity::GetScale() const {
	return GetRelativeScale() * (HasParent() ? GetParent().GetScale() : V2_float{ 1.0f, 1.0f });
}

Depth Entity::GetDepth() const {
	Depth parent_depth{};
	if (HasParent()) {
		auto parent{ GetParent() };
		if (parent.Has<Depth>()) {
			parent_depth = parent.Get<Depth>();
		}
	}
	return parent_depth + (Has<Depth>() ? Get<Depth>() : Depth{});
}

BlendMode Entity::GetBlendMode() const {
	return Has<BlendMode>() ? Get<BlendMode>()
							: (HasParent() ? GetParent().GetBlendMode() : BlendMode::Blend);
}

Origin Entity::GetOrigin() const {
	return Has<Origin>() ? Get<Origin>() : (HasParent() ? GetParent().GetOrigin() : Origin::Center);
}

Color Entity::GetTint() const {
	return Has<Tint>() ? Get<Tint>() : (HasParent() ? GetParent().GetTint() : Tint{});
}

Entity Entity::GetParent() const {
	return HasParent() ? Get<Entity>() : *this;
}

bool Entity::HasParent() const {
	return Has<Entity>();
}

std::size_t Entity::GetHash() const {
	return std::hash<ecs::Entity>()(*this);
}

bool Entity::IsImmovable() const {
	return (Has<RigidBody>() && Get<RigidBody>().immovable) ||
		   (HasParent() ? GetParent().IsImmovable() : false);
}

Entity& Entity::SetTint(const Color& color) {
	if (color != Tint{}) {
		Add<Tint>(color);
	} else {
		Remove<Tint>();
	}
	return *this;
}

std::array<V2_float, 4> Entity::GetTextureCoordinates(bool flip_vertically) const {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	if (*this == Entity{} && !IsAlive()) {
		return tex_coords;
	}

	V2_int texture_size;

	if (Has<TextureKey>()) {
		texture_size = game.texture.GetSize(Get<TextureKey>());
	} else if (Has<Text>()) {
		texture_size = Get<Text>().GetTexture().GetSize();
	} else if (Has<RenderTarget>()) {
		texture_size = Get<RenderTarget>().GetTexture().GetSize();
	}

	if (texture_size.IsZero()) {
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

	if (Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, Get<Flip>());
	}

	if (flip_vertically) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	return tex_coords;
}

Entity& Entity::SetPosition(const V2_float& position) {
	if (Has<Transform>()) {
		Get<Transform>().position = position;
	} else {
		Add<Transform>(position);
	}
	return *this;
}

Entity& Entity::SetRotation(float rotation) {
	if (Has<Transform>()) {
		Get<Transform>().rotation = rotation;
	} else {
		Add<Transform>(V2_float{}, rotation);
	}
	return *this;
}

Entity& Entity::SetScale(const V2_float& scale) {
	if (Has<Transform>()) {
		Get<Transform>().scale = scale;
	} else {
		Add<Transform>(V2_float{}, 0.0f, scale);
	}
	return *this;
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

void Entity::AddChild(Entity& child) {
	if (Has<Children>()) {
		Get<Children>().Add(child);
	} else {
		Add<Children>(child);
	}
	// Set child's parent.
	if (child.HasParent()) {
		child.Get<Entity>() = *this;
	} else {
		child.Add<Entity>(*this);
	}
}

void Entity::AddChild(std::string_view name, Entity& child) {
	if (Has<Children>()) {
		Get<Children>().Add(name, child);
	} else {
		Add<Children>(name, child);
	}
	// Set child's parent.
	if (child.HasParent()) {
		child.Get<Entity>() = *this;
	} else {
		child.Add<Entity>(*this);
	}
}

Entity Entity::GetChild(std::string_view name) {
	if (!Has<Children>()) {
		return {};
	}
	auto& children{ Get<Children>() };
	return children.Get(name);
}

bool Entity::HasChild(const Entity& child) const {
	if (!Has<Children>()) {
		return false;
	}
	auto& children{ Get<Children>() };
	return children.Has(child);
}

void Entity::RemoveChild(Entity& child) {
	if (!Has<Children>()) {
		return;
	}
	auto& children{ Get<Children>() };
	children.Remove(child);
	if (children.IsEmpty()) {
		Remove<Children>();
	}
	// Remove child's parent.
	if (child.Has<Entity>()) {
		child.Remove<Entity>();
	}
}

void Entity::RemoveChild(std::string_view name) {
	if (!Has<Children>()) {
		return;
	}
	auto& children{ Get<Children>() };
	auto child = children.Get(name);
	children.Remove(name);
	if (children.IsEmpty()) {
		Remove<Children>();
	}
	// Remove child's parent.
	if (child.Has<Entity>()) {
		child.Remove<Entity>();
	}
}

void Entity::SetParent(Entity& o) {
	PTGN_ASSERT(*this != o, "Cannot add game object as its own parent");
	PTGN_ASSERT(o != Entity{}, "Cannot add null game object as its own parent");
	// Add child to parent's children.
	if (o.Has<Children>()) {
		o.Get<Children>().Add(*this);
	} else {
		o.Add<Children>(*this);
	}
	if (HasParent()) {
		Get<Entity>() = o;
	} else {
		Add<Entity>(o);
	}
}

Children::Children(std::string_view name, const Entity& child) {
	Add(name, child);
}

Children::Children(const Entity& child) {
	Add(child);
}

void Children::Add(const Entity& child) {
	children_.emplace(child);
}

void Children::Remove(const Entity& child) {
	children_.erase(child);
}

void Children::Add(std::string_view name, const Entity& child) {
	named_children_.emplace(Hash(name), child);
}

void Children::Remove(std::string_view name) {
	named_children_.erase(Hash(name));
}

Entity Children::Get(std::string_view name) {
	if (auto it{ named_children_.find(Hash(name)) }; it != named_children_.end()) {
		return it->second;
	}
	return {};
}

bool Children::Has(const Entity& child) const {
	if (children_.count(child) > 0) {
		return true;
	}
	return MapContains(named_children_, child);
}

bool Children::IsEmpty() const {
	return children_.empty() && named_children_.empty();
}

namespace impl {

template <typename T>
void to_json_single(json& j, const Entity& e) {
	if (e.Has<T>()) {
		j[type_name_without_namespaces<T>()] = e.Get<T>();
	}
}

template <typename... Ts>
void to_json(json& j, const Entity& e) {
	(to_json_single<Ts>(j, e), ...);
}

template <typename T>
void from_json_single(const json& j, Entity& e) {
	j[type_name_without_namespaces<T>()].get_to(e.Has<T>() ? e.Get<T>() : e.Add<T>());
}

template <typename... Ts>
void from_json(const json& j, Entity& e) {
	(from_json_single<Ts>(j, e), ...);
}

} // namespace impl

void to_json(json& j, const Entity& e) {
	j = json{};
	impl::to_json<UUID>(j, e);
	impl::to_json<
		Draggable, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureKey,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Lifetime,
		PointLight>(j, e);
}

void from_json(const json& j, Entity& e) {
	PTGN_ASSERT(e != Entity{}, "Cannot read JSON into null entity");
	impl::from_json<UUID>(j, e);
	impl::from_json<
		Draggable, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureKey,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Lifetime,
		PointLight>(j, e);
}

} // namespace ptgn