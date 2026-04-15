#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn::impl {

struct ChildKey : public ArithmeticComponent<std::size_t> {
	using ArithmeticComponent::ArithmeticComponent;

	explicit ChildKey(std::string_view key);
};

struct Parent : public Entity {
	using Entity::Entity;

	explicit Parent(Entity entity);
};

struct Children {
	Children() = default;

	void Clear();

	void Add(Entity child, std::optional<std::string_view> name = {});

	void Remove(Entity child);
	void Remove(std::string_view name);

	// @return Entity with given name. Assertion called if no such entity exists.
	Entity Get(std::string_view name) const;

	bool IsEmpty() const;

	bool Has(Entity child) const;
	bool Has(std::string_view name) const;

	PTGN_REFLECT(Children, children_)

	std::vector<Entity> children_;
};

} // namespace ptgn::impl