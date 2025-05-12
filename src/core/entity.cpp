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

Entity& Entity::Destroy() {
	ecs::Entity::Destroy();
	return *this;
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

UUID Entity::GetUUID() const {
	PTGN_ASSERT(Has<UUID>(), "Every entity must have a UUID");
	return Get<UUID>();
}

Entity Entity::GetRootEntity() const {
	return HasParent() ? GetParent().GetRootEntity() : *this;
}

Entity Entity::GetParent() const {
	return HasParent() ? Get<impl::Parent>() : *this;
}

bool Entity::HasParent() const {
	return Has<impl::Parent>();
}

void Entity::RemoveParentImpl() {
	Remove<impl::Parent>();
}

void Entity::RemoveParent() {
	if (Has<impl::Parent>()) {
		if (auto& parent{ Get<impl::Parent>() }; parent.Has<impl::Children>()) {
			auto& children{ parent.Get<impl::Children>() };
			children.Remove(*this);
		}
		RemoveParentImpl();
	}
}

void Entity::SetParentImpl(Entity& parent) {
	if (!parent || parent == *this) {
		RemoveParent();
		return;
	}
	if (HasParent()) {
		Get<impl::Parent>() = parent;
	} else {
		Add<impl::Parent>(parent);
	}
}

void Entity::SetParent(Entity& parent) {
	SetParentImpl(parent);
	if (parent && parent != *this) {
		parent.AddChildImpl(*this);
	}
}

Entity Entity::CreateChild(std::string_view name) {
	auto entity = GetManager().CreateEntity();
	AddChild(entity, name);
	return entity;
}

void Entity::AddChildImpl(Entity& child, std::string_view name) {
	PTGN_ASSERT(child, "Cannot add an null entity as a child");
	PTGN_ASSERT(*this != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		GetManager() == child.GetManager(), "Cannot set cross manager parent-child relationships"
	);
	auto& children = GetOrAdd<impl::Children>();
	children.Add(child, name);
}

void Entity::AddChild(Entity& child, std::string_view name) {
	AddChildImpl(child, name);
	child.SetParentImpl(*this);
}

void Entity::RemoveChild(Entity& child) {
	child.RemoveParent(*this);
}

void Entity::RemoveChild(std::string_view name) {
	if (!Has<impl::Children>()) {
		return;
	}
	auto& children{ Get<impl::Children>() };
	auto child{ children.Get(name) };
	child.RemoveParent(*this);
}

bool Entity::HasChild(std::string_view name) const {
	if (!Has<impl::Children>()) {
		return false;
	}
	const auto& children{ Get<impl::Children>() };
	return children.Has(name);
}

bool Entity::HasChild(const Entity& child) const {
	if (!Has<impl::Children>()) {
		return false;
	}
	const auto& children{ Get<impl::Children>() };
	return children.Has(child);
}

Entity Entity::GetChild(std::string_view name) {
	if (!Has<impl::Children>()) {
		return {};
	}
	const auto& children{ Get<impl::Children>() };
	return children.Get(name);
}

Entity& Entity::SetEnabled(bool enabled) {
	return AddOrRemove<Enabled>(*this, enabled);
}

Entity& Entity::Disable() {
	return SetEnabled(false);
}

Entity& Entity::Enable() {
	return SetEnabled(true);
}

bool Entity::IsEnabled() const {
	return GetOrParentOrDefault<Enabled>(*this, false);
}

Entity& Entity::SetTransform(const Transform& transform) {
	if (Has<Transform>()) {
		Get<Transform>() = transform;
	} else {
		Add<Transform>(transform);
	}
	return *this;
}

Transform Entity::GetTransform() const {
	return GetOrDefault<Transform>(*this);
}

Transform Entity::GetAbsoluteTransform() const {
	return GetTransform().RelativeTo(
		HasParent() ? GetParent().GetAbsoluteTransform() : Transform{}
	);
}

Entity& Entity::SetPosition(const V2_float& position) {
	if (Has<Transform>()) {
		Get<Transform>().position = position;
	} else {
		Add<Transform>(position);
	}
	return *this;
}

V2_float Entity::GetPosition() const {
	return GetTransform().position;
}

V2_float Entity::GetAbsolutePosition() const {
	return GetAbsoluteTransform().position;
}

Entity& Entity::SetRotation(float rotation) {
	if (Has<Transform>()) {
		Get<Transform>().rotation = rotation;
	} else {
		Add<Transform>(V2_float{}, rotation);
	}
	return *this;
}

float Entity::GetRotation() const {
	return GetTransform().rotation;
}

float Entity::GetAbsoluteRotation() const {
	return GetAbsoluteTransform().rotation;
}

Entity& Entity::SetScale(const V2_float& scale) {
	if (Has<Transform>()) {
		Get<Transform>().scale = scale;
	} else {
		Add<Transform>(V2_float{}, 0.0f, scale);
	}
	return *this;
}

V2_float Entity::GetScale() const {
	return GetTransform().scale;
}

V2_float Entity::GetAbsoluteScale() const {
	return GetAbsoluteTransform().scale;
}

namespace impl {

void Children::Add(const Entity& child, std::string_view name) {
	auto [it, _] = children_.emplace(child);
	if (name) {
		it->Add<ChildKey>(name);
	}
}

void Children::Remove(const Entity& child) {
	children_.erase(child);
	// TODO: Consider adding a use count to ChildKey so it can be removed once an entity is no
	// longer a child of any other entity.
}

void Children::Remove(std::string_view name) {
	ChildKey k{ name };
	for (auto it = children_.begin(); it != children_.end();) {
		if (it->Has<ChildKey>() && it->Get<ChildKey>() == k) {
			it = children_.erase(it);
		} else {
			++it;
		}
	}
}

[[nodiscard]] Entity Children::Get(std::string_view name) {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (it->Has<ChildKey>() && it->Get<ChildKey>() == k) {
			return entity;
		}
	}
	return {};
}

[[nodiscard]] bool Children::IsEmpty() const {
	return children_.empty();
}

[[nodiscard]] bool Children::Has(const Entity& child) const {
	return children_.count(child) > 0;
}

[[nodiscard]] bool Children::Has(std::string_view name) const {
	ChildKey k{ name };
	for (const auto& entity : children_) {
		if (it->Has<ChildKey>() && it->Get<ChildKey>() == k) {
			return true;
		}
	}
	return false;
}

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
		Draggable, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureHandle,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Lifetime,
		PointLight>(j, e);
}

void from_json(const json& j, Entity& e) {
	PTGN_ASSERT(e != Entity{}, "Cannot read JSON into null entity");
	impl::from_json<UUID>(j, e);
	impl::from_json<
		Draggable, Transform, Enabled, Depth, Visible, DisplaySize, Tint, LineWidth, TextureHandle,
		impl::AnimationInfo, TextureCrop, RigidBody, Interactive, impl::Offsets, Lifetime,
		PointLight>(j, e);
}

} // namespace ptgn