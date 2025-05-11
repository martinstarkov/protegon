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

namespace impl {

Children::Children(const Entity& child, std::string_view name) {
	Add(child, name);
}

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