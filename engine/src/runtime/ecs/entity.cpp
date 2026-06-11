#include "runtime/ecs/entity.h"

#include <ecs/ecs.h>

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scripting/script.h"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

namespace {

Transform GetWorldTransformImpl(Entity entity, std::size_t search_depth) {
	auto transform{ GetTransform(entity) };

	if (entity.Has<impl::IgnoreParentTransform>()) {
		return transform;
	}

	if (!HasParent(entity)) {
		return transform;
	}

	if (search_depth >= kMaxParentDepth) {
		PTGN_ASSERT(false, "Maximum parent depth exceeded while resolving world transform");
		return transform;
	}

	auto parent{ GetParent(entity) };

	if (parent == entity) {
		PTGN_ASSERT(false, "Entity cannot be its own parent while resolving world transform");
		return transform;
	}

	auto relative_to{ GetWorldTransformImpl(parent, search_depth + 1) };
	auto world_transform{ transform.RelativeTo(relative_to) };

	if (entity.Has<impl::IgnoreParentPosition>()) {
		world_transform.position = transform.position;
	}

	if (entity.Has<impl::IgnoreParentScale>()) {
		world_transform.scale = transform.scale;
		world_transform.ClampScale();
	}

	if (entity.Has<impl::IgnoreParentRotation>()) {
		world_transform.rotation = transform.rotation;
	}

	return world_transform;
}

Transform GetTransformImpl(Entity entity, Transform world_transform, std::size_t search_depth) {
	if (entity.Has<impl::IgnoreParentTransform>()) {
		return world_transform;
	}

	if (!HasParent(entity)) {
		return world_transform;
	}

	if (search_depth >= kMaxParentDepth) {
		PTGN_ASSERT(false, "Maximum parent depth exceeded while resolving local transform");
		return world_transform;
	}

	auto parent{ GetParent(entity) };

	if (parent == entity) {
		PTGN_ASSERT(false, "Entity cannot be its own parent while resolving local transform");
		return world_transform;
	}

	auto parent_world_transform{ GetWorldTransformImpl(parent, search_depth + 1) };
	auto local_transform{ world_transform.InverseRelativeTo(parent_world_transform) };

	if (entity.Has<impl::IgnoreParentPosition>()) {
		local_transform.position = world_transform.position;
	}

	if (entity.Has<impl::IgnoreParentScale>()) {
		local_transform.scale = world_transform.scale;
		local_transform.ClampScale();
	}

	if (entity.Has<impl::IgnoreParentRotation>()) {
		local_transform.rotation = world_transform.rotation;
	}

	return local_transform;
}

} // namespace

namespace impl {

void AddMandatoryComponents(
	Entity entity, std::optional<std::string_view> tag, std::optional<std::uint64_t> uuid
) {
	entity.Add<impl::Tag>(tag.value_or(impl::kDefaultTag));
	entity.Add<impl::UUID>(uuid.value_or(impl::UUID{}));
}

} // namespace impl

Entity& Entity::Destroy(bool orphan_children) {
	if (!*this) {
		return *this;
	}

	if (HasChildren(*this)) {
		// Prevent iterator invalidation by copying the children list.
		std::vector<Entity> children{ GetChildren(*this) };

		if (orphan_children) {
			for (Entity child : children) {
				PTGN_ASSERT(child != *this, "Entity cannot be its own child");
				impl::OrphanChild(child);
			}
		} else {
			for (Entity child : children) {
				PTGN_ASSERT(child != *this, "Entity cannot be its own child");
				child.Destroy();
			}
		}
	}

	entity_.Destroy();
	return *this;
}

Manager& Entity::GetManager() {
	PTGN_ASSERT(entity_, "Cannot get manager of a null entity");
	return static_cast<Manager&>(entity_.GetManager());
}

const Manager& Entity::GetManager() const {
	PTGN_ASSERT(entity_, "Cannot get manager of a null entity");
	return static_cast<const Manager&>(entity_.GetManager());
}

const Scene& Entity::GetScene() const {
	PTGN_ASSERT(entity_, "Cannot get scene of a null entity");
	PTGN_ASSERT(HasScene(), "Scene of each valid entity must be set upon construction");
	return *scene_;
}

Scene& Entity::GetScene() {
	return const_cast<Scene&>(std::as_const(*this).GetScene()); // NOSONAR
}

bool Entity::HasScene() const {
	return scene_;
}

bool Entity::IsIdenticalTo(Entity entity) const {
	return entity_.IsIdenticalTo(entity.entity_);
}

int Entity::GetUUID() const {
	PTGN_ASSERT(Has<impl::UUID>(), "Every entity must have a UUID");
	return static_cast<int>(Get<impl::UUID>());
}

std::string Entity::GetTag() const {
	PTGN_ASSERT(Has<impl::Tag>(), "Every entity must have a tag");
	return Get<impl::Tag>().value;
}

Entity& Entity::SetTag(std::string_view tag) {
	if (Has<impl::Tag>()) {
		Get<impl::Tag>() = tag;
	} else {
		Add<impl::Tag>(tag);
	}
	return *this;
}

std::size_t Entity::GetECSId() const {
	return entity_.GetId();
}

bool Entity::WasCreatedBefore(Entity other) const {
	PTGN_ASSERT(other != *this, "Cannot check if an entity was created before itself");
	auto version{ entity_.GetVersion() };
	if (auto other_version{ other.entity_.GetVersion() }; version != other_version) {
		return version < other_version;
	}
	return GetECSId() < other.GetECSId();
}

void Entity::Invalidate() {
	*this = {};
}

void Entity::OnEvent(const Event& event) {
	if (!*this) {
		return;
	}

	if (auto scripts{ TryGet<impl::Scripts>() }) {
		scripts->OnEvent(event);
	}

	// OnEvent may have resulted in this entity being destroyed, or the scripts component being
	// removed.

	if (!*this) {
		return;
	}

	if (auto scripts{ TryGet<impl::Scripts>() }) {
		scripts->ApplyPending();
	}
}

void Entity::SerializeAllImpl(json& j) const {
	JsonArchiver archiver;

	const auto& pools{ GetManager().pools_ };

	for (const auto& pool : pools) {
		if (!pool) {
			continue;
		}
		pool->Serialize(archiver, entity_.GetId());
	}

	j = archiver.j;
}

void Entity::DeserializeAllImpl(const json& j) {
	JsonArchiver archiver;
	archiver.j = j;

	impl::ComponentRegistry::AddTypes(GetManager());

	const auto& manager{ GetManager() };

	for (const auto& pool : manager.pools_) {
		if (!pool) {
			continue;
		}
		pool->Deserialize(archiver, manager, entity_.GetId());
	}
}

void to_json(json& j, const Entity& entity) {
	j = json{};

	if (!entity) {
		return;
	}

	j["uuid"]  = entity.GetUUID();
	j["tag"]   = entity.GetTag();
	j["scene"] = entity.GetScene().GetTag();
}

void from_json(const json& j, Entity& entity) {
	PTGN_ASSERT(entity, "Cannot read JSON into null entity");

	if (j.contains("uuid")) {
		impl::UUID uuid;
		j["uuid"].get_to(uuid);
		entity.Add<impl::UUID>(uuid);
	}

	if (j.contains("tag")) {
		impl::Tag tag;
		j["tag"].get_to(tag.value);
		entity.Add<impl::Tag>(std::move(tag));
	}

	if (j.contains("scene")) {
		std::string scene_tag;
		j["scene"].get_to(scene_tag);
		PTGN_ASSERT(entity.GetScene().GetTag() == scene_tag, "Entity scene tag mismatch");
	}
}

Transform GetTransform(Entity entity) {
	return entity.template TryAdd<Transform>();
}

Transform GetWorldTransform(Entity entity) {
	return GetWorldTransformImpl(entity, 0uz);
}

Transform GetTransform(Entity entity, Transform world_transform) {
	return GetTransformImpl(entity, world_transform, 0uz);
}

void SetWorldTransform(Entity entity, Transform world_transform) {
	SetTransform(entity, GetTransform(entity, world_transform));
}

Transform GetDrawTransform(Entity entity) {
	auto offset_transform{ GetOffset(entity) };
	PTGN_ASSERT(
		!entity.Has<impl::CameraData>(), "GetDrawTransform is not meant to be used on scene cameras"
	);
	auto transform{ GetWorldTransform(entity) };
	transform = transform.RelativeTo(offset_transform);
	return transform;
}

V2_float GetPosition(Entity entity) {
	return GetTransform(entity).position;
}

V2_float GetWorldPosition(Entity entity) {
	return GetWorldTransform(entity).position;
}

Degrees GetRotation(Entity entity) {
	return GetTransform(entity).rotation.ToDeg();
}

Degrees GetWorldRotation(Entity entity) {
	return GetWorldTransform(entity).rotation.ToDeg();
}

V2_float GetScale(Entity entity) {
	return GetTransform(entity).scale;
}

V2_float GetWorldScale(Entity entity) {
	return GetWorldTransform(entity).scale;
}

void SetTransform(Entity entity, Transform transform) {
	entity.template Add<Transform>(transform);
}

void SetPosition(Entity entity, V2_float position) {
	auto transform{ GetTransform(entity) };
	transform.position = position;
	SetTransform(entity, transform);
}

void SetPositionX(Entity entity, float position_x) {
	SetPosition(entity, V2_float{ position_x, GetPosition(entity).y });
}

void SetPositionY(Entity entity, float position_y) {
	SetPosition(entity, V2_float{ GetPosition(entity).x, position_y });
}

void Translate(Entity entity, V2_float position_difference) {
	SetPosition(entity, GetPosition(entity) + position_difference);
}

void TranslateX(Entity entity, float position_x_difference) {
	Translate(entity, V2_float{ position_x_difference, 0.0f });
}

void TranslateY(Entity entity, float position_y_difference) {
	Translate(entity, V2_float{ 0.0f, position_y_difference });
}

void SetRotation(Entity entity, Radians rotation) {
	auto transform{ GetTransform(entity) };
	transform.rotation = rotation;
	SetTransform(entity, transform);
}

void SetRotation(Entity entity, Degrees rotation) {
	SetRotation(entity, rotation.ToRad());
}

void Rotate(Entity entity, Radians angle_difference) {
	SetRotation(entity, GetRotation(entity).ToRad() + angle_difference);
}

void Rotate(Entity entity, Degrees angle_difference) {
	Rotate(entity, angle_difference.ToRad());
}

void SetScale(Entity entity, V2_float scale) {
	auto transform{ GetTransform(entity) };
	transform.scale = scale;
	transform.ClampScale();
	SetTransform(entity, transform);
}

void SetScale(Entity entity, float scale) {
	SetScale(entity, V2_float{ scale });
}

void SetScaleX(Entity entity, float scale_x) {
	SetScale(entity, V2_float{ scale_x, GetScale(entity).y });
}

void SetScaleY(Entity entity, float scale_y) {
	SetScale(entity, V2_float{ GetScale(entity).x, scale_y });
}

void Scale(Entity entity, V2_float scale_multiplier) {
	SetScale(entity, GetScale(entity) * scale_multiplier);
}

void ScaleX(Entity entity, float scale_x_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.x *= scale_x_multiplier;
	SetScale(entity, scale);
}

void ScaleY(Entity entity, float scale_y_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.y *= scale_y_multiplier;
	SetScale(entity, scale);
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

} // namespace ptgn