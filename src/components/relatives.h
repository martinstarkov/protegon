#pragma once

#include <string_view>
#include <vector>

#include "components/generic.h"
#include "core/entity.h"
#include "serialization/serializable.h"

namespace ptgn::impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;
};

struct Parent : public Entity {
	using Entity::Entity;

	Parent(const Entity& entity);
};

struct Children {
	Children() = default;

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

	PTGN_SERIALIZER_REGISTER_NAMED(Children, KeyValue("children", children_))

private:
	friend void ptgn::ClearChildren(Entity& entity);
	friend const std::vector<Entity>& ptgn::GetChildren(const Entity& entity);

	std::vector<Entity> children_;
};

} // namespace ptgn::impl