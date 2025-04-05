#pragma once

#include <array>
#include <unordered_set>

#include "components/common.h"
#include "core/transform.h"
#include "core/uuid.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "serialization/fwd.h"

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class Manager;

class Entity : private ecs::Entity {
public:
	using ecs::Entity::Entity;

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	~Entity()							 = default;

	friend bool operator==(const Entity& a, const Entity& b) {
		return ecs::Entity{ a } == ecs::Entity{ b };
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return Entity{}.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	Entity Copy() {
		return ecs::Entity::Copy<Ts...>();
	}

	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return ecs::Entity::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		ecs::Entity::Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return ecs::Entity::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return ecs::Entity::HasAny<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() const {
		return ecs::Entity::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() {
		return ecs::Entity::Get<Ts...>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	void Destroy();

	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Component manipulation functions.

	// @return *this.
	Entity& SetVisible(bool visible);

	// @return *this.
	Entity& Show();

	// @return *this.
	Entity& Hide();

	// @return *this.
	Entity& SetEnabled(bool enabled);

	// @return *this.
	Entity& Disable();

	// @return *this.
	Entity& Enable();

	[[nodiscard]] bool IsVisible() const;
	[[nodiscard]] bool IsEnabled() const;

	[[nodiscard]] UUID GetUUID() const;

	// @return Reference to the transform of the top most parent entity. If none of the entities
	// have a transform component, an assertion is called.
	[[nodiscard]] const Transform& GetRootTransform() const;
	[[nodiscard]] Transform& GetRootTransform();

	[[nodiscard]] Transform GetRelativeTransform() const;

	[[nodiscard]] Transform GetTransform() const;

	[[nodiscard]] V2_float GetRelativePosition() const;

	[[nodiscard]] V2_float GetPosition() const;

	[[nodiscard]] Transform GetRelativeOffset() const;

	[[nodiscard]] Transform GetOffset() const;

	// @return Rotation of the object in radians relative to { 1, 0 }, clockwise positive.
	[[nodiscard]] float GetRelativeRotation() const;

	// @return Rotation of the object in radians relative to its parent object and { 1, 0 },
	// clockwise positive.
	[[nodiscard]] float GetRotation() const;

	[[nodiscard]] V2_float GetRelativeScale() const;

	[[nodiscard]] V2_float GetScale() const;

	// Set the local position of this game object.
	// @return *this.
	Entity& SetPosition(const V2_float& position);

	// Set the local rotation of this game object.
	// @return *this.
	Entity& SetRotation(float rotation);

	// Set the local scale of this game object.
	// @return *this.
	Entity& SetScale(const V2_float& scale);

	Entity& SetDepth(const Depth& depth);

	[[nodiscard]] Depth GetDepth() const;

	Entity& SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] BlendMode GetBlendMode() const;

	Entity& SetOrigin(Origin origin);

	[[nodiscard]] Origin GetOrigin() const;

	Entity& SetTint(const Color& color);

	[[nodiscard]] Color GetTint() const;

	[[nodiscard]] bool IsImmovable() const;

	// @return *this.
	Entity& AddChild(const Entity& o);

	// @return *this.
	Entity& RemoveChild(const Entity& o);

	Entity& SetParent(const Entity& o);

	// If object has no parent, returns *this.
	[[nodiscard]] Entity GetParent() const;

	[[nodiscard]] bool HasParent() const;

	[[nodiscard]] std::size_t GetHash() const;

	friend void to_json(json& j, const Entity& e);
	friend void from_json(const json& j, Entity& e);

protected:
	friend class impl::RenderData;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;

private:
	friend class Manager;

	explicit Entity(const ecs::Entity& e) : ecs::Entity{ e } {}
};

template <typename TCallback, typename... TArgs>
static void Invoke(const Entity& e, TArgs&&... args) {
	if (e.Has<TCallback>()) {
		const auto& callback{ e.Get<TCallback>() };
		Invoke(callback, std::forward<TArgs>(args)...);
	}
}

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std

namespace ptgn {

struct Children {
	Children(const Entity& o);

	void Add(const Entity& o);
	void Remove(const Entity& o);

	[[nodiscard]] bool IsEmpty() const;

private:
	std::unordered_set<Entity> children_;
};

} // namespace ptgn