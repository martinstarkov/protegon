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

namespace {

bool IsInParentChain(Entity entity, Entity possible_parent) {
	bool found{ false };

	ForEachParent(
		entity, [](auto) { return false; },
		[&found, possible_parent](Entity parent) {
			if (parent == possible_parent) {
				found = true;
				return false;
			}

			return true;
		}
	);

	return found;
}

void AttachChild(Entity parent, Entity child, std::optional<std::string_view> name) {
	PTGN_ASSERT(parent, "Cannot add a child to a null parent entity");
	PTGN_ASSERT(child, "Cannot add a null entity as a child");
	PTGN_ASSERT(parent != child, "Cannot add an entity as its own child");
	PTGN_ASSERT(
		parent.GetManager() == child.GetManager(),
		"Cannot set cross manager parent-child relationships"
	);
	PTGN_ASSERT(
		!IsInParentChain(parent, child), "Cannot parent an entity to one of its descendants"
	);

	RemoveParent(child);

	child.Add<impl::Parent>(parent);

	auto& children{ parent.TryAdd<impl::Children>() };
	children.Add(child, name);
}

} // namespace

namespace impl {

void OrphanChildren(Scene& scene) {
	for (auto [entity, orphan] : scene.EntitiesWith<impl::Orphan>()) {
		RemoveParent(entity);
		entity.Remove<impl::Orphan>();
	}
}

void ClearDeadChildren(Scene& scene) {
	for (auto [entity, children] : scene.EntitiesWith<impl::Children>()) {
		std::erase_if(children.children_, [](Entity child) { return !child; });
	}
}

void OrphanChild(Entity entity) {
	PTGN_ASSERT(entity, "Cannot orphan null entity child");

	entity.Add<Orphan>();
}

} // namespace impl

Entity GetRootEntity(Entity entity) {
	Entity root{ entity };

	ForEachParent(
		entity, [](auto) { return false; },
		[&root](Entity parent) {
			root = parent;
			return true;
		}
	);

	return root;
}

Entity GetParent(Entity entity) {
	return entity && HasParent(entity) ? entity.Get<impl::Parent>() : Entity{};
}

bool HasParent(Entity entity) {
	return entity && entity.Has<impl::Parent>();
}

void RemoveParent(Entity entity) {
	if (!HasParent(entity)) {
		return;
	}

	if (impl::Parent parent{ entity.Get<impl::Parent>() }; parent && parent.Has<impl::Children>()) {
		auto& children{ parent.Get<impl::Children>() };
		children.Remove(entity);
	}

	entity.Remove<impl::Parent>();
}

void SetParent(Entity entity, Entity parent, bool ignore_parent_transform) {
	PTGN_ASSERT(entity, "Cannot set parent of null entity");

	IgnoreParentTransform(entity, ignore_parent_transform);

	if (!parent || parent == entity) {
		RemoveParent(entity);
		return;
	}

	AttachChild(parent, entity, {});
}

void AddChild(Entity entity, Entity child, std::optional<std::string_view> name) {
	AttachChild(entity, child, name);
}

void ClearChildren(Entity entity) {
	if (!entity.Has<impl::Children>()) {
		return;
	}

	auto children{ entity.Get<impl::Children>().children_ };

	for (Entity child : children) {
		if (child && HasParent(child) && GetParent(child) == entity) {
			child.Remove<impl::Parent>();
		}
	}

	entity.Get<impl::Children>().Clear();
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
	PTGN_ASSERT(HasChildren(entity), "Entity has no children");
	const auto& children{ entity.Get<impl::Children>() };
	return children.Get(name);
}

bool HasChildren(Entity entity) {
	return entity.Has<impl::Children>() && !entity.Get<impl::Children>().children_.empty();
}

const std::vector<Entity>& GetChildren(Entity entity) {
	PTGN_ASSERT(HasChildren(entity));
	const auto& children{ entity.Get<impl::Children>().children_ };
	return children;
}

} // namespace ptgn