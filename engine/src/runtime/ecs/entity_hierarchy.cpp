#include "runtime/ecs/entity_hierarchy.h"

#include <optional>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/relatives.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void OrphanChild(Entity entity) {
	PTGN_ASSERT(entity, "Cannot orphan null entity child");

	entity.Add<Orphan>();
}

void OrphanChildren(Scene& scene) {
	for (auto [entity, orphan] : scene.EntitiesWith<Orphan>()) {
		entity.Remove<Parent>();
		entity.Remove<Orphan>();
	}
}

void ClearDeadChildren(Scene& scene) {
	for (auto [entity, children] : scene.EntitiesWith<impl::Children>()) {
		std::erase_if(children.children_, [](const Entity& child) { return !child; });
	}
}

void AddChildImpl(Entity entity, Entity child, std::optional<std::string_view> name) {
	PTGN_ASSERT(child, "Cannot add an null entity as a child");
	PTGN_ASSERT(entity != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		entity.GetManager() == child.GetManager(),
		"Cannot set cross manager parent-child relationships"
	);
	auto& children{ entity.TryAdd<Children>() };
	children.Add(child, name);
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
		entity.Remove<impl::Parent>();
	}
}

void IgnoreParentTransform(Entity entity, bool ignore_parent_transform) {
	if (ignore_parent_transform) {
		entity.Add<impl::IgnoreParentTransform>();
	} else {
		entity.Remove<impl::IgnoreParentTransform>();
	}
}

void IgnoreParentPosition(Entity entity, bool ignore_parent_position) {
	if (ignore_parent_position) {
		entity.Add<impl::IgnoreParentPosition>();
	} else {
		entity.Remove<impl::IgnoreParentPosition>();
	}
}

void IgnoreParentRotation(Entity entity, bool ignore_parent_rotation) {
	if (ignore_parent_rotation) {
		entity.Add<impl::IgnoreParentRotation>();
	} else {
		entity.Remove<impl::IgnoreParentRotation>();
	}
}

void IgnoreParentScale(Entity entity, bool ignore_parent_scale) {
	if (ignore_parent_scale) {
		entity.Add<impl::IgnoreParentScale>();
	} else {
		entity.Remove<impl::IgnoreParentScale>();
	}
}

void SetParent(Entity entity, Entity parent, bool ignore_parent_transform) {
	IgnoreParentTransform(entity, ignore_parent_transform);
	impl::SetParentImpl(entity, parent);
	if (parent && parent != entity) {
		impl::AddChildImpl(parent, entity, {});
	}
}

void AddChild(Entity entity, Entity child, std::optional<std::string_view> name) {
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
	const auto& children{ entity.Get<impl::Children>() };
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