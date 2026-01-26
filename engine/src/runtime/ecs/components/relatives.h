#pragma once

#include <string_view>
#include <vector>

#include "ecs/components/generic.h"
#include "ecs/entity.h"
#include "serialization/json/serialize.h"

namespace ptgn::impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;
};

struct Parent : public Entity {
	using Entity::Entity;

	Parent(Entity entity);
};

struct Children {
	Children() = default;

	Children(Entity first_child);

	void Clear();

	void Add(Entity child, std::string_view name = {});

	void Remove(Entity child);
	void Remove(std::string_view name);

	// @return Entity with given name. Assertion called if no such entity exists.
	Entity Get(std::string_view name) const;

	bool IsEmpty() const;

	bool Has(Entity child) const;
	bool Has(std::string_view name) const;

	PTGN_SERIALIZER_REGISTER_NAMED(Children, KeyValue("children", children_))

	std::vector<Entity> children_;
};

} // namespace ptgn::impl