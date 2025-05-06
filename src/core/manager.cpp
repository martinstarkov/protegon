#include "core/manager.h"

#include "core/entity.h"
#include "components/uuid.h"
#include "ecs/ecs.h"
#include "serialization/json.h"
#include "common/assert.h"

namespace ptgn {

Manager Manager::Clone() const {
	return Manager{ ecs::Manager::Clone() };
}

void Manager::Refresh() {
	ecs::Manager::Refresh();
}

void Manager::Reserve(std::size_t capacity) {
	ecs::Manager::Reserve(capacity);
}

Entity Manager::GetEntityByUUID(const UUID& uuid) const {
	auto entities{ Entities() };
	for (auto e : entities) {
		if (e.Get<UUID>() == uuid) {
			return e;
		}
	}
	return {};
}

Entity Manager::CreateEntity(const json& j) {
	Entity entity{ ecs::Manager::CreateEntity() };
	j.get_to(entity);
	PTGN_ASSERT(entity.Has<UUID>(), "Entity created from json must have a UUID");
	return entity;
}

Entity Manager::CreateEntity(UUID uuid) {
	Entity entity{ ecs::Manager::CreateEntity() };
	entity.Add<UUID>(uuid);
	return entity;
}

Entity Manager::CreateEntity() {
	return CreateEntity(UUID{});
}

std::size_t Manager::Size() const {
	return ecs::Manager::Size();
}

bool Manager::IsEmpty() const {
	return ecs::Manager::IsEmpty();
}

std::size_t Manager::Capacity() const {
	return ecs::Manager::Capacity();
}

void Manager::Clear() {
	return ecs::Manager::Clear();
}

void Manager::Reset() {
	return ecs::Manager::Reset();
}

} // namespace ptgn