#include "commands/entity/entity_snapshot.h"

#include "core/assert.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/relatives.h"
#include "runtime/ecs/tag.h"
#include "runtime/scene/scene.h"

namespace ptgn::editor {

namespace {

void CreateNodes(
	Scene& scene,
	const SerializedEntity& serialized
) {
	const UUID uuid{
		GetSerializedEntityUUID(serialized)
	};

	if (!scene.GetEntity(uuid)) {
		scene.CreateEntity(
			Tag{ serialized.tag },
			uuid
		);
	}

	for (const auto& child :
		 serialized.children) {
		CreateNodes(
			scene,
			child
		);
	}
}

void RestoreEntityData(
	Scene& scene,
	const SerializedEntity& serialized
) {
	Entity entity{
		scene.GetEntity(
			GetSerializedEntityUUID(serialized)
		)
	};

	PTGN_ASSERT(
		entity,
		"Failed to find entity created for snapshot"
	);

	DeserializeEntity(
		serialized,
		entity
	);

	for (const auto& child :
		 serialized.children) {
		RestoreEntityData(
			scene,
			child
		);
	}
}

void RestoreChildren(
	Scene& scene,
	const SerializedEntity& serialized
) {
	Entity parent{
		scene.GetEntity(
			GetSerializedEntityUUID(serialized)
		)
	};

	PTGN_ASSERT(
		parent,
		"Failed to find snapshot hierarchy parent"
	);

	for (const auto& child_serialized :
		 serialized.children) {
		Entity child{
			scene.GetEntity(
				GetSerializedEntityUUID(
					child_serialized
				)
			)
		};

		PTGN_ASSERT(
			child,
			"Failed to find snapshot hierarchy child"
		);

		SetParent(
			child,
			parent
		);

		RestoreChildren(
			scene,
			child_serialized
		);
	}
}

void DestroyNode(
	Scene& scene,
	const SerializedEntity& serialized
) {
	for (const auto& child :
		 serialized.children) {
		DestroyNode(
			scene,
			child
		);
	}

	Entity entity{
		scene.GetEntity(
			GetSerializedEntityUUID(serialized)
		)
	};

	if (!entity) {
		return;
	}

	if (HasParent(entity)) {
		RemoveParent(entity);
	}

	entity.Destroy();
}

} // namespace

EntitySnapshot CaptureEntitySnapshot(
	Entity entity
) {
	PTGN_ASSERT(
		entity,
		"Cannot capture a null entity snapshot"
	);

	EntitySnapshot snapshot{
		.root = SerializeEntity(
			entity,
			{
				.include_uuid = true,
				.include_children = true,
			}
		),
	};

	if (HasParent(entity)) {
		Entity parent{
			GetParent(entity)
		};

		PTGN_ASSERT(
			parent.Has<UUID>(),
			"Snapshot parent must have a UUID"
		);

		snapshot.parent_uuid =
			parent.Get<UUID>();
	}

	return snapshot;
}

Entity RestoreEntitySnapshot(
	Scene& scene,
	const EntitySnapshot& snapshot
) {
	CreateNodes(
		scene,
		snapshot.root
	);

	// All persistent UUIDs must resolve before component deserializers run.
	scene.Refresh();

	RestoreEntityData(
		scene,
		snapshot.root
	);

	scene.Refresh();

	RestoreChildren(
		scene,
		snapshot.root
	);

	if (snapshot.parent_uuid) {
		Entity root{
			scene.GetEntity(
				GetSerializedEntityUUID(
					snapshot.root
				)
			)
		};

		Entity parent{
			scene.GetEntity(
				*snapshot.parent_uuid
			)
		};

		if (root && parent) {
			SetParent(
				root,
				parent
			);
		}
	}

	scene.Refresh();

	return scene.GetEntity(
		GetSerializedEntityUUID(
			snapshot.root
		)
	);
}

void DestroyEntitySnapshot(
	Scene& scene,
	const EntitySnapshot& snapshot
) {
	DestroyNode(
		scene,
		snapshot.root
	);

	scene.Refresh();
}

} // namespace ptgn::editor