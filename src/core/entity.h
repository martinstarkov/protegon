#pragma once

#include "common/assert.h"
#include "common/type_traits.h"
#include "components/uuid.h"
#include "core/entity_hierarchy.h"
#include "ecs/ecs.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "serialization/json_archiver.h"

namespace ptgn {

class Entity;
class Manager;
class Scene;
class Camera;
struct Transform;
struct Interactive;
class SceneInput;
struct Depth;
struct Visible;
class IDrawable;

Entity& SetDepth(Entity& entity, const Depth& depth);

namespace impl {

class RenderData;
class IScript;
const ptgn::Interactive& GetInteractive(const Entity& entity);

template <typename T>
inline constexpr bool is_retrievable_component_v{
	!tt::is_any_of_v<T, Transform, Depth, Visible, Interactive, IDrawable>
};

} // namespace impl

class Entity : private ecs::impl::Entity<JSONArchiver> {
private:
	using Parent = ecs::impl::Entity<JSONArchiver>;

public:
	// Entity wrapper functionality.

	using Parent::Entity;

	Entity() = default;
	Entity(const Parent& e);

	explicit Entity(Scene& scene);

	[[nodiscard]] ecs::impl::Index GetId() const;

	explicit operator bool() const {
		return Parent::operator bool();
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return static_cast<const Parent&>(a) == static_cast<const Parent&>(b);
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	[[nodiscard]] Entity Copy() {
		return Parent::Copy<Ts...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return Parent::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <typename T, typename... Ts>
	T& TryAdd(Ts&&... constructor_args) {
		return Parent::TryAdd<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		Parent::Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return Parent::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return Parent::HasAny<Ts...>();
	}

	template <typename... Ts> //, tt::enable<(impl::is_retrievable_component_v<Ts> && ...)> = true>
	[[nodiscard]] decltype(auto) Get() const {
		return GetImpl<Ts...>();
	}

	template <typename... Ts, tt::enable<(impl::is_retrievable_component_v<Ts> && ...)> = true>
	[[nodiscard]] decltype(auto) Get() {
		return GetImpl<Ts...>();
	}

	template <typename T> //, tt::enable<impl::is_retrievable_component_v<T>> = true>
	[[nodiscard]] const T* TryGet() const {
		return TryGetImpl<T>();
	}

	template <typename T, tt::enable<impl::is_retrievable_component_v<T>> = true>
	[[nodiscard]] T* TryGet() {
		return TryGetImpl<T>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	// Destroy the given entity and potentially its children.
	// @param orphan_children If false, destroys all the children (and their children). If true,
	// sets the parent of all the entity's children to null, orphaning them.
	Entity& Destroy(bool orphan_children = false);

	[[nodiscard]] const Scene& GetScene() const;
	[[nodiscard]] Scene& GetScene();

	[[nodiscard]] const Camera& GetCamera() const;
	[[nodiscard]] Camera& GetCamera();

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
	template <typename... Ts>
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
	template <typename... Ts>
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

private:
	friend Entity& SetTransform(Entity& entity, const Transform& transform);
	friend const Interactive& impl::GetInteractive(const Entity& entity);
	friend Entity& SetDepth(Entity& entity, const Depth& depth);
	friend class Manager;
	friend class impl::RenderData;

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() const {
		return Parent::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() {
		return Parent::Get<Ts...>();
	}

	template <typename T>
	[[nodiscard]] const T* TryGetImpl() const {
		return Parent::TryGet<T>();
	}

	template <typename T>
	[[nodiscard]] T* TryGetImpl() {
		return Parent::TryGet<T>();
	}

	template <typename T>
	void SerializeImpl(json& j) const {
		static_assert(
			tt::has_to_json_v<T>, "Component has not been registered with the serializer"
		);
		PTGN_ASSERT(Has<T>(), "Entity must have component which is being serialized");
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		j[component_name] = GetImpl<T>();
	}

	void SerializeAllImpl(json& j) const;

	template <typename T>
	void DeserializeImpl(const json& j) {
		static_assert(
			tt::has_from_json_v<T>, "Component has not been registered with the serializer"
		);
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		PTGN_ASSERT(j.contains(component_name), "JSON does not contain ", component_name);
		j[component_name].get_to(TryAdd<T>());
	}

	void DeserializeAllImpl(const json& j);
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