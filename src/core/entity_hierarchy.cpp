#include "core/entity_hierarchy.h"

#include <string_view>
#include <vector>

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
	auto& children = entity.TryAdd<impl::Children>();
	children.Add(child, name);
}

void RemoveParentImpl(Entity& entity) {
	entity.Remove<impl::Parent>();
}

void SetParentImpl(Entity& entity, Entity& parent) {
	if (!parent || parent == entity) {
		RemoveParent(entity);
		return;
	}
	if (HasParent(entity)) {
		entity.Get<impl::Parent>() = parent;
	} else {
		entity.Add<impl::Parent>(parent);
	}
}

} // namespace impl

const Entity& GetRootEntity(const Entity& entity) {
	if (HasParent(entity)) {
		auto parent{ GetParent(entity) };
		return GetRootEntity(parent);
	}
	return entity;
}

Entity& GetRootEntity(Entity& entity) {
	return const_cast<Entity&>(GetRootEntity(const_cast<const Entity&>(entity)));
}

const Entity& GetParent(const Entity& entity) {
	return HasParent(entity) ? entity.Get<impl::Parent>() : entity;
}

Entity& GetParent(Entity& entity) {
	return const_cast<Entity&>(GetParent(const_cast<const Entity&>(entity)));
}

bool HasParent(const Entity& entity) {
	return entity.Has<impl::Parent>();
}

void RemoveParent(Entity& entity) {
	if (entity.Has<impl::Parent>()) {
		if (auto& parent{ entity.Get<impl::Parent>() }; parent.Has<impl::Children>()) {
			auto& children{ parent.Get<impl::Children>() };
			children.Remove(entity);
		}
		impl::RemoveParentImpl(entity);
	}
}

void IgnoreParentTransform(Entity& entity, bool ignore_parent_transform) {
	if (ignore_parent_transform) {
		entity.Add<impl::IgnoreParentTransform>(ignore_parent_transform);
	} else {
		entity.Remove<impl::IgnoreParentTransform>();
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

void RemoveChild(Entity& entity, Entity& child) {
	PTGN_ASSERT(GetParent(child) == entity);
	RemoveParent(child);
}

void RemoveChild(Entity& entity, std::string_view name) {
	if (!entity.Has<impl::Children>()) {
		return;
	}
	const auto& children{ entity.Get<impl::Children>() };
	auto child{ children.Get(name) };
	RemoveParent(child);
}

bool HasChild(const Entity& entity, std::string_view name) {
	if (!entity.Has<impl::Children>()) {
		return false;
	}
	const auto& children{ entity.Get<impl::Children>() };
	return children.Has(name);
}

bool HasChild(const Entity& entity, const Entity& child) {
	if (!entity.Has<impl::Children>()) {
		return false;
	}
	const auto& children{ entity.Get<impl::Children>() };
	return children.Has(child);
}

Entity GetChild(const Entity& entity, std::string_view name) {
	if (!entity.Has<impl::Children>()) {
		return {};
	}
	const auto& children{ entity.Get<impl::Children>() };
	return children.Get(name);
}

const std::vector<Entity>& GetChildren(const Entity& entity) {
	if (!entity.Has<impl::Children>()) {
		return {};
	}
	const auto& children{ entity.Get<impl::Children>().children_ };
	return children;
}

} // namespace ptgn