#include "runtime/ecs/entity.h"

#include <ecs/ecs.h>

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/component.h"
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

Entity& Entity::Destroy(bool orphan_children) {
	if (!*this) {
		return *this;
	}

	if (HasChildren(*this)) {
		const auto& children{ GetChildren(*this) };
		if (orphan_children) {
			for (const auto& child : children) {
				impl::OrphanChild(child);
			}
		} else {
			for (Entity child : children) {
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
	return scene_ != nullptr;
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
	return Get<impl::Tag>();
}

Entity& Entity::SetTag(std::string_view tag) {
	if (Has<impl::Tag>()) {
		Get<impl::Tag>().GetValue() = tag;
	} else {
		Add<impl::Tag>(tag);
	}
	return *this;
}

std::size_t Entity::GetECSId() const {
	return entity_.GetId();
}

std::size_t Entity::GetHash() const {
	return GetUUID();
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
		j["tag"].get_to(tag.GetValue());
		entity.Add<impl::Tag>(std::move(tag));
	}

	if (j.contains("scene")) {
		std::string scene_tag{ 0 };
		j["scene"].get_to(scene_tag);
		PTGN_ASSERT(entity.GetScene().GetTag() == scene_tag, "Entity scene tag mismatch");
	}
}

std::size_t Hash(Entity entity) {
	return std::hash<Entity>()(entity);
}

Transform GetTransform(Entity entity) {
	return entity.template TryAdd<Transform>();
}

Transform GetWorldTransform(Entity entity) {
	const auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>()) {
		return transform;
	}
	Transform relative_to;
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		relative_to = GetWorldTransform(parent);
	}
	auto world_transform{ transform.RelativeTo(relative_to) };
	if (entity.Has<impl::IgnoreParentPosition>()) {
		world_transform.SetPosition(transform.GetPosition());
	}
	if (entity.Has<impl::IgnoreParentScale>()) {
		world_transform.SetScale(transform.GetScale());
	}
	if (entity.Has<impl::IgnoreParentRotation>()) {
		world_transform.SetRotation(transform.GetRotation());
	}
	return world_transform;
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
	return GetTransform(entity).GetPosition();
}

V2_float GetWorldPosition(Entity entity) {
	return GetWorldTransform(entity).GetPosition();
}

Degrees GetRotation(Entity entity) {
	return GetTransform(entity).GetRotation();
}

Degrees GetWorldRotation(Entity entity) {
	return GetWorldTransform(entity).GetRotation();
}

V2_float GetScale(Entity entity) {
	return GetTransform(entity).GetScale();
}

V2_float GetWorldScale(Entity entity) {
	return GetWorldTransform(entity).GetScale();
}

void SetTransform(Entity entity, Transform transform) {
	entity.template Add<Transform>(transform);
}

void SetPosition(Entity entity, V2_float position) {
	auto transform{ GetTransform(entity) };
	transform.SetPosition(position);
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
	transform.SetRotation(rotation);
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
	transform.SetScale(scale);
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

} // namespace ptgn