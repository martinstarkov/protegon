#pragma once

#include <string_view>
#include <vector>

#include "core/ecs/components/generic.h"
#include "core/ecs/entity.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Manager;

namespace impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;
};

} // namespace impl

struct Parent : public Entity {
	using Entity::Entity;

	Parent(const Entity& entity);
};

struct Children {
	Children() = default;

	PTGN_SERIALIZER_REGISTER_NAMED(Children, KeyValue("children", children_))
private:
	void Clear();

	void Add(Entity& child, std::string_view name = {});

	void Remove(const Entity& child);
	void Remove(std::string_view name);

	// @return Entity with given name. Assertion called if no such entity exists.
	[[nodiscard]] const Entity& Get(std::string_view name) const;
	[[nodiscard]] Entity& Get(std::string_view name);

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] bool Has(const Entity& child) const;
	[[nodiscard]] bool Has(std::string_view name) const;

private:
	friend class Manager;
	friend class Entity;
	friend void ptgn::ClearChildren(Entity& entity);
	friend const std::vector<Entity>& ptgn::GetChildren(const Entity& entity);
	friend const Entity& ptgn::GetChild(const Entity& entity, std::string_view name);
	friend bool ptgn::HasChild(const Entity& entity, const Entity& child);
	friend bool ptgn::HasChild(const Entity& entity, std::string_view name);
	friend void ptgn::RemoveChild(Entity& entity, std::string_view name);
	friend void ptgn::RemoveParent(Entity& entity);
	friend void ptgn::impl::AddChildImpl(Entity& entity, Entity& child, std::string_view name);

	std::vector<Entity> children_;
};

} // namespace ptgn