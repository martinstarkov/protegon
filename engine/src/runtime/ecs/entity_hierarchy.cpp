#include "ecs/entity_hierarchy.h"

#include <string_view>
#include <vector>

#include "core/assert.h"
#include "ecs/components/relatives.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "ecs/manager.h"

namespace ptgn {

namespace impl {

void AddChildImpl(Entity entity, Entity child, std::string_view name) {
	PTGN_ASSERT(child, "Cannot add an null entity as a child");
	PTGN_ASSERT(entity != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		entity.GetManager() == child.GetManager(),
		"Cannot set cross manager parent-child relationships"
	);
	auto& children{ entity.TryAdd<Children>() };
	children.Add(child, name);
}

void RemoveParentImpl(Entity entity) {
	entity.Remove<Parent>();
}

void SetParentImpl(Entity entity, Entity parent) {
	if (!parent || parent == entity) {
		RemoveParent(entity);
		return;
	}
	entity.Add<Parent>(parent);
}

} // namespace impl

Entity GetRootEntity(Entity entity) {
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		return GetRootEntity(parent);
	}
	return entity;
}

Entity GetParent(Entity entity) {
	return HasParent(entity) ? entity.Get<impl::Parent>() : entity;
}

bool HasParent(Entity entity) {
	return entity.Has<impl::Parent>();
}

void RemoveParent(Entity entity) {
	if (entity.Has<impl::Parent>()) {
		if (auto& parent{ entity.Get<impl::Parent>() }; parent.Has<impl::Children>()) {
			auto& children{ parent.Get<impl::Children>() };
			children.Remove(entity);
		}
		impl::RemoveParentImpl(entity);
	}
}

void IgnoreParentTransform(Entity entity, bool ignore_parent_transform) {
	if (ignore_parent_transform) {
		entity.Add<impl::IgnoreParentTransform>(ignore_parent_transform);
	} else {
		entity.Remove<impl::IgnoreParentTransform>();
	}
}

void SetParent(Entity entity, Entity parent, bool ignore_parent_transform) {
	IgnoreParentTransform(entity, ignore_parent_transform);
	impl::SetParentImpl(entity, parent);
	if (parent && parent != entity) {
		impl::AddChildImpl(parent, entity, {});
	}
}

void AddChild(Entity entity, Entity child, std::string_view name) {
	impl::AddChildImpl(entity, child, name);
	impl::SetParentImpl(child, entity);
}

void ClearChildren(Entity entity) {
	if (!entity.Has<impl::Children>()) {
		return;
	}

	auto& children{ entity.Get<impl::Children>() };
	// Cannot use reference here due to unordered_set const iterator.
	for (Entity child : children.children_) {
		child.Remove<impl::Parent>();
	}
	children.Clear();
}

void RemoveChild(Entity entity, Entity child) {
	PTGN_ASSERT(GetParent(child) == entity);
	RemoveParent(child);
}

void RemoveChild(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Children>()) {
		return;
	}
	auto& children{ entity.Get<impl::Children>() };
	if (children.Has(name)) {
		auto child{ children.Get(name) };
		RemoveParent(child);
	}
}

bool HasChild(Entity entity, std::string_view name) {
	if (!entity.Has<impl::Children>()) {
		return false;
	}
	const auto& children{ entity.Get<impl::Children>() };
	return children.Has(name);
}

bool HasChild(Entity entity, Entity child) {
	if (!entity.Has<impl::Children>()) {
		return false;
	}
	const auto& children{ entity.Get<impl::Children>() };
	return children.Has(child);
}

Entity GetChild(Entity entity, std::string_view name) {
	PTGN_ASSERT(HasChildren(entity));
	const auto& children{ entity.Get<impl::Children>() };
	return children.Get(name);
}

bool HasChildren(Entity entity) {
	return entity.Has<impl::Children>();
}

const std::vector<Entity>& GetChildren(Entity entity) {
	PTGN_ASSERT(HasChildren(entity));
	const auto& children{ entity.Get<impl::Children>().children_ };
	return children;
}

} // namespace ptgn