#include "core/manager.h"

#include "common/assert.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "ecs/ecs.h"
#include "manager.h"
#include "serialization/json.h"

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
	for (Entity e : entities) {
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

void to_json(json& j, const Manager& manager) {
	j["next_entity"]	  = manager.next_entity_;
	j["count"]			  = manager.count_;
	j["refresh_required"] = manager.refresh_required_;
	j["entities"]		  = manager.entities_;
	j["refresh"]		  = manager.refresh_;
	j["free_entities"]	  = manager.free_entities_;
	j["versions"]		  = manager.versions_;

	// TODO: Serialize component pools.
	// std::vector<std::unique_ptr<impl::AbstractPool>> pools_;
}

void from_json(const json& j, Manager& manager) {
	j.at("next_entity").get_to(manager.next_entity_);
	j.at("count").get_to(manager.count_);
	j.at("refresh_required").get_to(manager.refresh_required_);
	j.at("entities").get_to(manager.entities_);
	j.at("refresh").get_to(manager.refresh_);
	j.at("free_entities").get_to(manager.free_entities_);
	j.at("versions").get_to(manager.versions_);

	// TODO: Deserialize component pools.
	// std::vector<std::unique_ptr<impl::AbstractPool>> pools_;
}

} // namespace ptgn

namespace ecs::impl {

void to_json(json& j, const DynamicBitset& bitset) {
	j["bit_count"] = bitset.GetBitCount();
	j["data"]	   = bitset.GetData();
}

void from_json(const json& j, DynamicBitset& bitset) {
	std::vector<std::uint8_t> data;
	std::size_t bit_count{ 0 };
	j.at("bit_count").get_to(bit_count);
	j.at("data").get_to(data);
	bitset = { bit_count, data };
}

} // namespace ecs::impl