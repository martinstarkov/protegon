#pragma once

#include <string_view>
#include <unordered_set>

#include "components/generic.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "rendering/api/origin.h"
#include "serialization/fwd.h"
#include "serialization/serializable.h"

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class Manager;

class Entity {
public:
	// Interface functions.

	virtual void Draw(impl::RenderData& ctx) const {
		/* Optional user implementation */
	}

	// Entity wrapper functionality.

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	virtual ~Entity()					 = default;

	explicit Entity(Manager& manager);

	explicit operator bool() const {
		return entity_.operator bool();
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return a.entity_ == b.entity_;
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	[[nodiscard]] Entity Copy() {
		return entity_.Copy<Ts...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return entity_.Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <typename T, typename... Ts>
	T& TryAdd(Ts&&... constructor_args) {
		if (!Has<T>()) {
			return Add<T>(std::forward<Ts>(constructor_args)...);
		}
		return Get<T>();
	}

	// Get a component of the entity, and if it does not exist add it.
	template <typename T, typename... Ts>
	T& GetOrAdd(Ts&&... constructor_args) {
		if (Has<T>()) {
			return Get<T>();
		}
		return Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		entity_.Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return entity_.Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return entity_.HasAny<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() const {
		return entity_.Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() {
		return entity_.Get<Ts...>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	Entity& Destroy();

	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Entity property functions.

	[[nodiscard]] UUID GetUUID() const;

	[[nodiscard]] std::size_t GetHash() const;

	// Entity hierarchy functions.

	// @return The parent most entity, or *this if no parent exists.
	[[nodiscard]] Entity GetRootEntity() const;

	// @return Parent entity of the object. If object has no parent, returns *this.
	[[nodiscard]] Entity GetParent() const;

	[[nodiscard]] bool HasParent() const;

	void SetParent(Entity& parent);

	void RemoveParent();

	// Creates a child in the same manager as the parent entity.
	// @param name Optional name to retrieve the child handle by later. Note: If no name is
	// provided, the returned handle will be the only way to access the child directly.
	[[nodiscard]] Entity CreateChild(std::string_view name = {});

	void AddChild(Entity& child, std::string_view name = {});

	void RemoveChild(Entity& child);
	void RemoveChild(std::string_view name);

	// @return True if the entity has the given child, false otherwise.
	[[nodiscard]] bool HasChild(const Entity& child) const;
	[[nodiscard]] bool HasChild(std::string_view name) const;

	// @return Child entity with the given name, or null entity is no such child exists.
	[[nodiscard]] Entity GetChild(std::string_view name) const;

	// @return All childs entities tied to the object
	[[nodiscard]] std::unordered_set<Entity> GetChildren() const;

	// Entity activation functions.

	// If not enabled, removes the entity from the scene update list.
	// @return *this.
	Entity& SetEnabled(bool enabled);

	// Removes the entity from the scene update list.
	// @return *this.
	Entity& Disable();

	// Add the entity to the scene update list (if not already there).
	// @return *this.
	Entity& Enable();

	// @return True if the entity is in the scene update list.
	[[nodiscard]] bool IsEnabled() const;

	// Entity transform functions.

	// Set the relative transform of the entity with respect to its parent entity, camera, or
	// scene camera transform.
	// @return *this.
	Entity& SetTransform(const Transform& transform);

	// @return The relative transform of the entity with respect to its parent entity, camera, or
	// scene camera transform.
	[[nodiscard]] Transform GetTransform() const;

	// @return The absolute transform of the entity with respect to its parent scene camera
	// transform.
	[[nodiscard]] Transform GetAbsoluteTransform() const;

	// Set the relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	// @return *this.
	Entity& SetPosition(const V2_float& position);

	// @return The relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	[[nodiscard]] V2_float GetPosition() const;

	// @return The absolute position of the entity with respect to its parent scene camera position.
	[[nodiscard]] V2_float GetAbsolutePosition() const;

	// Set the relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	// @return *this.
	Entity& SetRotation(float rotation);

	// @return The relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	[[nodiscard]] float GetRotation() const;

	// @return The absolute rotation of the entity with respect to its parent scene camera rotation.
	// Clockwise positive. Unit: Radians.
	[[nodiscard]] float GetAbsoluteRotation() const;

	// Set the relative scale of the entity with respect to its parent entity, camera, or
	// scene camera scale.
	// @return *this.
	Entity& SetScale(const V2_float& scale);

	// @return The relative scale of the entity with respect to its parent entity, camera, or
	// scene camera scale.
	[[nodiscard]] V2_float GetScale() const;

	// @return The absolute scale of the entity with respect to its parent scene camera scale.
	[[nodiscard]] V2_float GetAbsoluteScale() const;

	Entity& SetOrigin(Origin origin);

	[[nodiscard]] Origin GetOrigin() const;

	friend void to_json(json& j, const Entity& entity);
	friend void from_json(const json& j, Entity& entity);

protected:
	template <typename T, typename... TArgs>
	Entity& AddOrRemove(bool condition, TArgs&&... args) {
		if (condition) {
			Add<T>(std::forward<TArgs>(args)...);
		} else {
			Remove<T>();
		}
		return *this;
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return Get<T>();
		}
		return T{ std::forward<TArgs>(args)... };
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrParentOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return Get<T>();
		}
		if (HasParent()) {
			return GetParent().GetOrDefault<T>(std::forward<TArgs>(args)...);
		}
		return T{ std::forward<TArgs>(args)... };
	}

private:
	friend class Manager;

	void AddChildImpl(Entity& child, std::string_view name = {});

	void SetParentImpl(Entity& parent);

	void RemoveParentImpl();

	// TODO: Convert back to inheriting from ecs::Entity...

	explicit Entity(
		ecs::impl::Index entity, ecs::impl::Version version, const ecs::Manager* manager
	) :
		entity_{ entity, version, manager } {}

	explicit Entity(const ecs::Entity& e);

	ecs::Entity entity_;
};

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	std::size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;

	PTGN_SERIALIZER_REGISTER_NAMELESS(ChildKey, value_)
};

struct Parent : public Entity {
	using Entity::Entity;

	Parent(const Entity& entity) : Entity{ entity } {}
};

struct Children {
	Children() = default;

	void Add(Entity& child, std::string_view name = {});

	void Remove(const Entity& child);
	void Remove(std::string_view name);

	// @return Entity with given name, or null entity is no such entity exists.
	[[nodiscard]] Entity Get(std::string_view name) const;

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] bool Has(const Entity& child) const;
	[[nodiscard]] bool Has(std::string_view name) const;

private:
	friend class Entity;

	std::unordered_set<Entity> children_;
};

} // namespace impl

} // namespace ptgn