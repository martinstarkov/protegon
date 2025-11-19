#include "ecs/manager.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "ecs/component_registry.h"
#include "ecs/components/uuid.h"
#include "ecs/ecs.h"
#include "ecs/entity.h"
#include "nlohmann/json.hpp"
#include "serialization/json/archiver.h"
#include "serialization/json/fwd.h"

namespace ptgn {

void Manager::Refresh() {
	ManagerBase::Refresh();
}

void Manager::Reserve(std::size_t capacity) {
	ManagerBase::Reserve(capacity);
}

Entity Manager::GetEntityByUUID(const UUID& uuid) const {
	auto entities{ Entities() };
	for (Entity e : entities) {
		PTGN_ASSERT(e.Has<UUID>(), "Entity does not have a valid UUID component");
		if (e.Get<UUID>() == uuid) {
			return e;
		}
	}
	return {};
}

Entity Manager::CreateEntity(const json& j) {
	Entity entity{ Manager::CreateEntity() };
	PTGN_ASSERT(entity, "Failed to create entity");
	entity.Deserialize(j);
	PTGN_ASSERT(entity.Has<UUID>(), "Entity created from json must have a UUID");
	return entity;
}

Entity Manager::CreateEntity(UUID uuid) {
	Entity entity{ ManagerBase::CreateEntity() };
	impl::EntityAccess::Add<UUID>(entity, uuid);
	return entity;
}

Entity Manager::CreateEntity() {
	return CreateEntity(UUID{});
}

std::size_t Manager::Size() const {
	return ManagerBase::Size();
}

bool Manager::IsEmpty() const {
	return ManagerBase::IsEmpty();
}

std::size_t Manager::Capacity() const {
	return ManagerBase::Capacity();
}

void Manager::ClearEntities() {
	for (auto entity : Entities()) {
		entity.Destroy();
	}
}

void Manager::Clear() {
	return ManagerBase::Clear();
}

void Manager::Reset() {
	return ManagerBase::Reset();
}

Manager::Manager(ManagerBase&& manager) : ManagerBase{ std::move(manager) } {}

void to_json(json& j, const Manager& manager) {
	j["next_entity"]	  = manager.next_entity_;
	j["count"]			  = manager.count_;
	j["refresh_required"] = manager.refresh_required_;
	j["entities"]		  = manager.entities_;
	j["refresh"]		  = manager.refresh_;
	j["free_entities"]	  = manager.free_entities_;
	j["versions"]		  = manager.versions_;

	JsonArchiver archiver;

	for (const auto& pool : manager.pools_) {
		if (pool == nullptr) {
			continue;
		}
		pool->Serialize(archiver);
	}

	j["pools"] = archiver.j;
}

void from_json(const json& j, Manager& manager) {
	j.at("next_entity").get_to(manager.next_entity_);
	j.at("count").get_to(manager.count_);
	j.at("refresh_required").get_to(manager.refresh_required_);
	j.at("entities").get_to(manager.entities_);
	j.at("refresh").get_to(manager.refresh_);
	j.at("free_entities").get_to(manager.free_entities_);
	j.at("versions").get_to(manager.versions_);

	JsonArchiver archiver;
	archiver.j = j.at("pools");

	impl::ComponentRegistry::AddTypes(manager);

	PTGN_ASSERT(!manager.pools_.empty(), "Failed to create any valid manager component pool types");

	for (auto& pool : manager.pools_) {
		if (pool == nullptr) {
			continue;
		}
		pool->Deserialize(archiver);
	}
}

} // namespace ptgn

namespace ecs::impl {

void to_json(ptgn::json& j, const DynamicBitset& bitset) {
	j["bit_count"] = bitset.Size();
	j["data"]	   = bitset.GetData();
}

void from_json(const ptgn::json& j, DynamicBitset& bitset) {
	std::vector<std::uint8_t> data;
	std::size_t bit_count{ 0 };
	j.at("bit_count").get_to(bit_count);
	j.at("data").get_to(data);
	bitset = { bit_count, data };
}

} // namespace ecs::impl