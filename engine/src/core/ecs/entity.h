#pragma once

#include "core/ecs/components/component_utils.h"
#include "core/ecs/components/uuid.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/util/concepts.h"
#include "debug/runtime/assert.h"
#include "ecs/ecs.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"
#include "serialization/json/json_archiver.h"

namespace ptgn {

class Entity;
class Manager;
class Scene;
class Camera;
class SceneInput;

struct Transform;
struct Depth;
struct Interactive;
struct PostFX;
struct PreFX;

namespace impl {

class RenderData;
class IScript;
class EntityAccess;

} // namespace impl

class Entity : private ecs::impl::Entity<JSONArchiver> {
protected:
	using BaseEntity = ecs::impl::Entity<JSONArchiver>;

public:
	// Entity wrapper functionality.

	using BaseEntity::Entity;

	Entity() = default;
	Entity(const BaseEntity& e);

	explicit Entity(Scene& scene);

	[[nodiscard]] ecs::impl::Index GetId() const;

	explicit operator bool() const {
		return BaseEntity::operator bool();
	}

	friend bool operator<(const Entity& lhs, const Entity& rhs) {
		if (lhs == rhs) {
			return false;
		}
		return lhs.WasCreatedBefore(rhs);
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return static_cast<const BaseEntity&>(a) == static_cast<const BaseEntity&>(b);
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <impl::ModifiableComponent... Ts>
	[[nodiscard]] Entity Copy() {
		return BaseEntity::Copy<Ts...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <impl::ModifiableComponent T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return AddImpl<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <impl::ModifiableComponent T, typename... Ts>
	T& TryAdd(Ts&&... constructor_args) {
		return TryAddImpl<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <impl::ModifiableComponent... Ts>
	void Remove() {
		RemoveImpl<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return BaseEntity::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return BaseEntity::HasAny<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() const {
		return GetImpl<Ts...>();
	}

	template <impl::RetrievableComponent... Ts>
	[[nodiscard]] decltype(auto) Get() {
		return GetImpl<Ts...>();
	}

	template <typename T>
	[[nodiscard]] const T* TryGet() const {
		return TryGetImpl<T>();
	}

	template <impl::RetrievableComponent T>
	[[nodiscard]] T* TryGet() {
		return TryGetImpl<T>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	// Destroy the given entity and potentially its children.
	// @param orphan_children If false, destroys all the children (and their children). If true,
	// removes the parents of all the entity's children, orphaning them.
	Entity& Destroy(bool orphan_children = false);

	[[nodiscard]] const Scene& GetScene() const;
	[[nodiscard]] Scene& GetScene();

	[[nodiscard]] const Camera& GetCamera() const;
	[[nodiscard]] Camera& GetCamera();

	// @return If the entity has a non primary camera attached to it, return its address, otherwise
	// return nullptr.
	[[nodiscard]] const Camera* GetNonPrimaryCamera() const;

	[[nodiscard]] const Manager& GetManager() const;
	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Entity property functions.

	[[nodiscard]] UUID GetUUID() const;

	[[nodiscard]] std::size_t GetHash() const;

	// Serialization.

	friend void to_json(json& j, const Entity& entity);
	friend void from_json(const json& j, Entity& entity);

	// Converts the specified entity components to a JSON object.
	template <JsonSerializable... Ts>
	[[nodiscard]] json Serialize() const {
		PTGN_ASSERT(*this, "Cannot serialize a null entity");

		json j{};

		if constexpr (sizeof...(Ts) == 0) {
			SerializeAllImpl(j);
		} else {
			(SerializeImpl<Ts>(j), ...);
		}

		return j;
	}

	// Populates the entity's components based on a JSON object. Does not impact existing
	// components, unless they are specified as part of Ts, in which case they are replaced.
	template <JsonDeserializable... Ts>
	void Deserialize(const json& j) {
		if constexpr (sizeof...(Ts) == 0) {
			DeserializeAllImpl(j);
		} else {
			PTGN_ASSERT(*this, "Cannot deserialize to a null entity");
			(DeserializeImpl<Ts>(j), ...);
		}
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return GetImpl<T>();
		}
		return T{ std::forward<TArgs>(args)... };
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrParentOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return GetImpl<T>();
		}
		if (HasParent(*this)) {
			return GetParent(*this).GetOrParentOrDefault<T>(std::forward<TArgs>(args)...);
		}
		return T{ std::forward<TArgs>(args)... };
	}

	// @return True if *this was created before other.
	[[nodiscard]] bool WasCreatedBefore(const Entity& other) const;

	// Equivalent of setting the entity handle to {}
	void Invalidate();

private:
	friend class impl::EntityAccess;
	friend class Manager;
	friend class impl::RenderData;

	template <typename... Ts>
	void RemoveImpl() {
		BaseEntity::Remove<Ts...>();
	}

	template <typename T, typename... Ts>
	T& AddImpl(Ts&&... constructor_args) {
		return BaseEntity::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename T, typename... Ts>
	T& TryAddImpl(Ts&&... constructor_args) {
		return BaseEntity::TryAdd<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() const {
		return BaseEntity::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() {
		return BaseEntity::Get<Ts...>();
	}

	template <typename T>
	[[nodiscard]] const T* TryGetImpl() const {
		return BaseEntity::TryGet<T>();
	}

	template <typename T>
	[[nodiscard]] T* TryGetImpl() {
		return BaseEntity::TryGet<T>();
	}

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
};

template <typename T>
concept EntityBase = IsOrDerivedFrom<T, Entity>;

namespace impl {

class EntityAccess {
public:
	template <typename... Ts>
	static void Remove(Entity& e) {
		e.RemoveImpl<Ts...>();
	}

	template <typename T, typename... Ts>
	static T& Add(Entity& e, Ts&&... constructor_args) {
		return e.AddImpl<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename T, typename... Ts>
	static T& TryAdd(Entity& e, Ts&&... constructor_args) {
		return e.TryAddImpl<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	[[nodiscard]] static decltype(auto) Get(const Entity& e) {
		return e.GetImpl<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] static decltype(auto) Get(Entity& e) {
		return e.GetImpl<Ts...>();
	}

	template <typename T>
	[[nodiscard]] static const T* TryGet(const Entity& e) {
		return e.TryGetImpl<T>();
	}

	template <typename T>
	[[nodiscard]] static T* TryGet(Entity& e) {
		return e.TryGetImpl<T>();
	}
};

} // namespace impl

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	std::size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std