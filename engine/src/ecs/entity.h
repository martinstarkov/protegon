#pragma once

#include "core/assert.h"
#include "core/util/concepts.h"
#include "ecs/components/uuid.h"
#include "ecs/ecs.h"
#include "ecs/entity_hierarchy.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"

namespace ptgn {

class Entity;
class Manager;
class Scene;
class Camera;
class SceneInput;

struct Transform;

class Scene; // forward

/*

class Entity {
public:
	using Native = ecs::Entity;

	Entity() = default;

	Entity(Native native, Scene* scene) : native_(native), scene_(scene) {}

	explicit operator bool() const {
		return static_cast<bool>(native_);
	}

	ecs::impl::Id Id() const {
		return native_.GetId();
	}

	ecs::impl::Version Version() const {
		return native_.GetVersion();
	}

	Scene* GetScene() const {
		return scene_;
	}

	Native& NativeHandle() {
		return native_;
	}

	const Native& NativeHandle() const {
		return native_;
	}

	// You can forward ECS operations if you like:
	template <typename T, typename... Args>
	T& Add(Args&&... args) {
		return native_.template Add<T>(std::forward<Args>(args)...);
	}

	template <typename... TComponents>
	void Remove() {
		native_.template Remove<TComponents...>();
	}

	template <typename... TComponents>
	decltype(auto) Get() {
		return native_.template Get<TComponents...>();
	}

	template <typename... TComponents>
	decltype(auto) Get() const {
		return native_.template Get<TComponents...>();
	}

	void Destroy() {
		native_.Destroy();
	}

private:
	Native native_;
	Scene* scene_{ nullptr };
};

*/

class Entity {
public:
	// Entity wrapper functionality.

	Entity() = default;

	Entity(ecs::impl::EntityHandle<JsonArchiver> entity) : entity_{ entity } {}

	explicit Entity(Scene& scene);

	explicit operator bool() const {
		return entity_.operator bool();
	}

	friend bool operator<(const Entity& lhs, const Entity& rhs) {
		if (lhs == rhs) {
			return false;
		}
		return lhs.WasCreatedBefore(rhs);
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return a.entity_ == b.entity_;
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

	void Clear() const;

	bool IsAlive() const;

	// Destroy the given entity and potentially its children.
	// @param orphan_children If false, destroys all the children (and their children). If true,
	// removes the parents of all the entity's children, orphaning them.
	// @return *this, allowing for it to be set to {} if needed.
	Entity& Destroy(bool orphan_children = false);

	const Scene& GetScene() const;
	Scene& GetScene();

	// const Camera& GetCamera() const;
	// Camera& GetCamera();

	// @return If the entity has a non primary camera attached to it, return its address, otherwise
	// return nullptr.
	// const Camera* GetNonPrimaryCamera() const;

	const Manager& GetManager() const;
	Manager& GetManager();

	bool IsIdenticalTo(const Entity& e) const;

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
	bool WasCreatedBefore(const Entity& other) const;

	// Equivalent of setting the entity handle to {}
	void Invalidate();

private:
	friend class Manager;

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

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	std::size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std