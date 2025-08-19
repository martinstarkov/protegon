#include "core/entity_hierarchy.h"

#include <string_view>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/relatives.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"

namespace ptgn {

namespace impl {

void AddChildImpl(Entity& entity, Entity& child, std::string_view name) {
	PTGN_ASSERT(child, "Cannot add an null entity as a child");
	PTGN_ASSERT(entity != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		entity.GetManager() == child.GetManager(),
		"Cannot set cross manager parent-child relationships"
	);
	auto& children{ EntityAccess::TryAdd<Children>(entity) };
	children.Add(child, name);
}

void RemoveParentImpl(Entity& entity) {
	EntityAccess::Remove<Parent>(entity);
}

void SetParentImpl(Entity& entity, Entity& parent) {
	if (!parent || parent == entity) {
		RemoveParent(entity);
		return;
	}
	if (HasParent(entity)) {
		entity.Get<Parent>() = parent;
	} else {
		EntityAccess::Add<Parent>(entity, parent);
	}
}

} // namespace impl

const Entity& GetRootEntity(const Entity& entity) {
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		return GetRootEntity(parent);
	}
	return entity;
}

Entity& GetRootEntity(Entity& entity) {
	return const_cast<Entity&>(GetRootEntity(std::as_const(entity)));
}

const Entity& GetParent(const Entity& entity) {
	return HasParent(entity) ? entity.Get<Parent>() : entity;
}

Entity& GetParent(Entity& entity) {
	return const_cast<Entity&>(GetParent(std::as_const(entity)));
}

bool HasParent(const Entity& entity) {
	return entity.Has<Parent>();
}

void RemoveParent(Entity& entity) {
	if (entity.Has<Parent>()) {
		if (auto& parent{ entity.Get<Parent>() }; parent.Has<Children>()) {
			auto& children{ parent.Get<Children>() };
			children.Remove(entity);
		}
		impl::RemoveParentImpl(entity);
	}
}

void IgnoreParentTransform(Entity& entity, bool ignore_parent_transform) {
	if (ignore_parent_transform) {
		impl::EntityAccess::Add<impl::IgnoreParentTransform>(entity, ignore_parent_transform);
	} else {
		impl::EntityAccess::Remove<impl::IgnoreParentTransform>(entity);
	}
}

void SetParent(Entity& entity, Entity& parent, bool ignore_parent_transform) {
	IgnoreParentTransform(entity, ignore_parent_transform);
	impl::SetParentImpl(entity, parent);
	if (parent && parent != entity) {
		impl::AddChildImpl(parent, entity);
	}
}

void AddChild(Entity& entity, Entity& child, std::string_view name) {
	impl::AddChildImpl(entity, child, name);
	impl::SetParentImpl(child, entity);
}

void ClearChildren(Entity& entity) {
	if (!entity.Has<Children>()) {
		return;
	}

	auto& children{ entity.Get<Children>() };
	// Cannot use reference here due to unordered_set const iterator.
	for (Entity child : children.children_) {
		impl::EntityAccess::Remove<Parent>(child);
	}
	children.Clear();
}

void RemoveChild(Entity& entity, Entity& child) {
	PTGN_ASSERT(GetParent(child) == entity);
	RemoveParent(child);
}

void RemoveChild(Entity& entity, std::string_view name) {
	if (!entity.Has<Children>()) {
		return;
	}
	auto& children{ entity.Get<Children>() };
	if (children.Has(name)) {
		auto& child{ children.Get(name) };
		RemoveParent(child);
	}
}

bool HasChild(const Entity& entity, std::string_view name) {
	if (!entity.Has<Children>()) {
		return false;
	}
	const auto& children{ entity.Get<Children>() };
	return children.Has(name);
}

bool HasChild(const Entity& entity, const Entity& child) {
	if (!entity.Has<Children>()) {
		return false;
	}
	const auto& children{ entity.Get<Children>() };
	return children.Has(child);
}

const Entity& GetChild(const Entity& entity, std::string_view name) {
	PTGN_ASSERT(HasChildren(entity));
	const auto& children{ entity.Get<Children>() };
	return children.Get(name);
}

Entity& GetChild(Entity& entity, std::string_view name) {
	return const_cast<Entity&>(GetChild(std::as_const(entity), name));
}

bool HasChildren(const Entity& entity) {
	return entity.Has<Children>();
}

const std::vector<Entity>& GetChildren(const Entity& entity) {
	PTGN_ASSERT(HasChildren(entity));
	const auto& children{ entity.Get<Children>().children_ };
	return children;
}

} // namespace ptgn