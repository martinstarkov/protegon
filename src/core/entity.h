#pragma once

#include <array>
#include <string_view>
#include <unordered_set>

#include "common/type_info.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "serialization/fwd.h"
#include "serialization/serializable.h"

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class Manager;

class Entity : private ecs::Entity {
public:
	// Interface functions.

	virtual void Draw(impl::RenderData& ctx) {}

	// Entity wrapper functionality.

	using ecs::Entity::Entity;

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	~Entity()							 = default;

	explicit operator bool() const {
		return ecs::Entity::operator bool();
	}

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
	[[nodiscard]] Entity Copy() {
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

	Entity& Destroy();

	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Component manipulation functions.

private:
	friend class Manager;

	explicit Entity(const ecs::Entity& e) : ecs::Entity{ e } {}
};

namespace impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;

	PTGN_SERIALIZER_REGISTER_NAMELESS(ChildKey, value_)
};

struct Children {
	Children(const Entity& child, std::string_view name = {});

	void Add(const Entity& child, std::string_view name = {});
	void Remove(const Entity& child);
	void Remove(std::string_view name);

	// @return Entity with given name, or null entity is no such entity exists.
	[[nodiscard]] Entity Get(std::string_view name);

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] bool Has(const Entity& child) const;
	[[nodiscard]] bool Has(std::string_view name) const;

private:
	friend class Entity;

	std::unordered_set<Entity> children_;
};

} // namespace impl

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std