#pragma once

#include <cstdint>
#include <ostream>

#include "core/assert.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "ecs/ecs.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Manager;
class Scene;

class UUID {
public:
	UUID();
	explicit UUID(std::uint64_t uuid);

	operator std::uint64_t() const;

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(UUID, uuid_)

private:
	std::uint64_t uuid_{ 0 };
};

class Entity {
public:
	// Entity wrapper functionality.

	Entity() = default;

	Entity(ecs::impl::EntityHandle<JsonArchiver> entity) : entity_{ entity } {}

	Entity(ecs::impl::EntityHandle<JsonArchiver> entity, const Scene* scene) :
		entity_{ entity }, scene_{ const_cast<Scene*>(scene) } {}

	Entity(Entity entity, const Scene* scene) :
		entity_{ entity.entity_ }, scene_{ const_cast<Scene*>(scene) } {}

	explicit Entity(Scene& scene);

	explicit operator bool() const {
		return entity_.operator bool();
	}

	bool operator==(const Entity&) const = default;

	friend bool operator<(const Entity& lhs, const Entity& rhs) {
		if (lhs == rhs) {
			return false;
		}
		return lhs.WasCreatedBefore(rhs);
	}

	friend std::ostream& operator<<(std::ostream& o, const Entity& lhs) {
		o << "[id=";
		o << lhs.entity_.GetId();
		o << ",manager=";
		o << &lhs.entity_.GetManager();
		o << "]";
		return o;
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... TComponents>
	[[nodiscard]] Entity Copy() {
		return entity_.Copy<TComponents...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <typename TComponent, typename... TArgs>
	TComponent& Add(TArgs&&... constructor_args) {
		return entity_.Add<TComponent, TArgs...>(std::forward<TArgs>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <typename TComponent, typename... TArgs>
	TComponent& TryAdd(TArgs&&... constructor_args) {
		return entity_.TryAdd<TComponent, TArgs...>(std::forward<TArgs>(constructor_args)...);
	}

	template <typename... TComponents>
	void Remove() {
		entity_.Remove<TComponents...>();
	}

	template <typename... TComponents>
	bool Has() const {
		return entity_.Has<TComponents...>();
	}

	template <typename... TComponents>
	bool HasAny() const {
		return entity_.HasAny<TComponents...>();
	}

	template <typename... TComponents>
	decltype(auto) Get() const {
		return entity_.Get<TComponents...>();
	}

	template <typename... TComponents>
	decltype(auto) Get() {
		return entity_.Get<TComponents...>();
	}

	template <typename T>
	const T* TryGet() const {
		return entity_.TryGet<T>();
	}

	template <typename T>
	T* TryGet() {
		return entity_.TryGet<T>();
	}

	// void Clear() const;

	// Destroy the given entity and potentially its children.
	// @param orphan_children If false, destroys all the children (and their children). If true,
	// removes the parents of all the entity's children, orphaning them.
	// @return *this, allowing for it to be set to {} if needed.
	Entity& Destroy(bool orphan_children = false);

	const Scene& GetScene() const;
	Scene& GetScene();

	/// @return True if the entity is part of a scene, false otherwise.
	bool HasScene() const;

	const Manager& GetManager() const;
	Manager& GetManager();

	bool IsIdenticalTo(Entity entity) const;

	// Entity property functions.

	UUID GetUUID() const;

	std::size_t GetHash() const;

	// Serialization.

	friend void to_json(json& j, const Entity& entity);
	friend void from_json(const json& j, Entity& entity);

	// Converts the specified entity components to a JSON object.
	template <JsonSerializable... TComponents>
	[[nodiscard]] json Serialize() const {
		PTGN_ASSERT(*this, "Cannot serialize a null entity");

		json j{};

		if constexpr (sizeof...(TComponents) == 0) {
			SerializeAllImpl(j);
		} else {
			(SerializeImpl<TComponents>(j), ...);
		}

		return j;
	}

	// Populates the entity's components based on a JSON object. Does not impact existing
	// components, unless they are specified as part of TComponents, in which case they are
	// replaced.
	template <JsonDeserializable... TComponents>
	void Deserialize(const json& j) {
		if constexpr (sizeof...(TComponents) == 0) {
			DeserializeAllImpl(j);
		} else {
			PTGN_ASSERT(*this, "Cannot deserialize to a null entity");
			(DeserializeImpl<TComponents>(j), ...);
		}
	}

	template <typename TComponent, typename... TArgs>
	TComponent GetOrDefault(TArgs&&... args) const {
		if (Has<TComponent>()) {
			return Get<TComponent>();
		}
		return TComponent{ std::forward<TArgs>(args)... };
	}

	template <typename TComponent, typename... TArgs>
	TComponent GetOrParentOrDefault(TArgs&&... args) const {
		if (Has<TComponent>()) {
			return Get<TComponent>();
		}
		if (HasParent(*this)) {
			return GetParent(*this).GetOrParentOrDefault<TComponent>(std::forward<TArgs>(args)...);
		}
		return TComponent{ std::forward<TArgs>(args)... };
	}

	// @return True if *this was created before other.
	bool WasCreatedBefore(Entity other) const;

	// Equivalent of setting the entity handle to {}
	void Invalidate();

private:
	friend class Manager;
	friend class Scene;

	template <JsonSerializable T>
	void SerializeImpl(json& j) const {
		PTGN_ASSERT(Has<T>(), "Entity must have component which is being serialized");
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		j[component_name] = GetImpl<T>();
	}

	void SerializeAllImpl(json& j) const;

	template <JsonDeserializable T>
	void DeserializeImpl(const json& j) {
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		PTGN_ASSERT(j.contains(component_name), "JSON does not contain ", component_name);
		j[component_name].get_to(TryAdd<T>());
	}

	void DeserializeAllImpl(const json& j);

	ecs::impl::EntityHandle<JsonArchiver> entity_;
	Scene* scene_{ nullptr };
};

[[nodiscard]] std::size_t Hash(Entity entity);

// template <typename T>
// concept EntityWrapper = std::derived_from<T, Entity>;

namespace impl {

struct IgnoreParentTransform {};

} // namespace impl

/// @return The transform of the entity.
Transform GetTransform(Entity entity);

/// @return The transform of the entity with respect to its parent entity.
Transform GetWorldTransform(Entity entity);

/// @return The transform of the entity with respect to its parent entity and including any
/// temporary offsets (e.g., shake or bounce).
Transform GetDrawTransform(Entity entity);

V2_float GetPosition(Entity entity);
V2_float GetWorldPosition(Entity entity);

float GetRotation(Entity entity);
float GetWorldRotation(Entity entity);

V2_float GetScale(Entity entity);
V2_float GetWorldScale(Entity entity);

/// Set the transform of the entity with respect to its parent entity.
void SetTransform(Entity entity, Transform transform);

void SetPosition(Entity entity, V2_float position);
void SetPositionX(Entity entity, float position_x);
void SetPositionY(Entity entity, float position_y);

void Translate(Entity entity, V2_float position_difference);
void TranslateX(Entity entity, float position_x_difference);
void TranslateY(Entity entity, float position_y_difference);

/// Set 2D rotation angle in radians.
/// Range: (-3.14159, 3.14159].
/// (clockwise positive).
///            -1.5708
///               |
///    3.14159 ---o--- 0
///               |
///             1.5708
void SetRotation(Entity entity, float rotation);
void Rotate(Entity entity, float angle_difference);

void SetScale(Entity entity, V2_float scale);
void SetScale(Entity entity, float scale);
void SetScaleX(Entity entity, float scale_x);
void SetScaleY(Entity entity, float scale_y);

void Scale(Entity entity, V2_float scale_multiplier);
void ScaleX(Entity entity, float scale_x_multiplier);
void ScaleY(Entity entity, float scale_y_multiplier);

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	std::size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

template <>
struct hash<ptgn::UUID> {
	std::size_t operator()(const ptgn::UUID& uuid) const {
		return static_cast<std::uint64_t>(uuid);
	}
};

} // namespace std