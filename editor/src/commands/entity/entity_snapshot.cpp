#include "commands/entity/entity_snapshot.h"

#include <utility>

#include "core/assert.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/scene/scene.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/graphics/draw.h"

namespace ptgn::editor {

namespace {

EntitySnapshot CaptureNode(Entity entity) {
	PTGN_ASSERT(entity);
	PTGN_ASSERT(entity.Has<UUID>());
	PTGN_ASSERT(entity.Has<Tag>());

	EntitySnapshot snapshot{
		.uuid = entity.Get<UUID>(),
		.tag = entity.Get<Tag>(),
		.components = SerializeEntityComponents(entity),
	};

	if (HasParent(entity)) {
		snapshot.parent_uuid = GetParent(entity).Get<UUID>();
	}

	if (HasChildren(entity)) {
		auto children{ GetChildren(entity) };
		SortByLocalDepth(children);
		snapshot.children.reserve(children.size());

		for (Entity child : children) {
			snapshot.children.push_back(CaptureNode(child));
		}
	}

	return snapshot;
}

void CreateNodes(Scene& scene, const EntitySnapshot& snapshot) {
	if (!scene.GetEntity(snapshot.uuid)) {
		(void)scene.CreateEntity(snapshot.tag, snapshot.uuid);
	}

	for (const auto& child : snapshot.children) {
		CreateNodes(scene, child);
	}
}

void RestoreComponents(Scene& scene, const EntitySnapshot& snapshot) {
	Entity entity{ scene.GetEntity(snapshot.uuid) };
	PTGN_ASSERT(entity);
	DeserializeEntityComponents(snapshot.components, entity);

	for (const auto& child : snapshot.children) {
		RestoreComponents(scene, child);
	}
}

void RestoreChildren(Scene& scene, const EntitySnapshot& snapshot) {
	Entity parent{ scene.GetEntity(snapshot.uuid) };
	PTGN_ASSERT(parent);

	for (const auto& child_snapshot : snapshot.children) {
		Entity child{ scene.GetEntity(child_snapshot.uuid) };
		PTGN_ASSERT(child);
		SetParent(child, parent);
		RestoreChildren(scene, child_snapshot);
	}
}

void DestroyNode(Scene& scene, const EntitySnapshot& snapshot) {
	for (const auto& child : snapshot.children) {
		DestroyNode(scene, child);
	}

	if (Entity entity{ scene.GetEntity(snapshot.uuid) }) {
		if (HasParent(entity)) {
			RemoveParent(entity);
		}
		entity.Destroy();
	}
}

} // namespace

EntitySnapshot CaptureEntitySnapshot(Entity entity) {
	return CaptureNode(entity);
}

Entity RestoreEntitySnapshot(Scene& scene, const EntitySnapshot& snapshot) {
	CreateNodes(scene, snapshot);
	scene.Refresh();

	RestoreComponents(scene, snapshot);
	scene.Refresh();

	RestoreChildren(scene, snapshot);

	if (snapshot.parent_uuid) {
		Entity root{ scene.GetEntity(snapshot.uuid) };
		Entity parent{ scene.GetEntity(*snapshot.parent_uuid) };
		if (root && parent) {
			SetParent(root, parent);
		}
	}

	scene.Refresh();
	return scene.GetEntity(snapshot.uuid);
}

void DestroyEntitySnapshot(Scene& scene, const EntitySnapshot& snapshot) {
	DestroyNode(scene, snapshot);
	scene.Refresh();
}

} // namespace ptgn::editor
