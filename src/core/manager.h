#pragma once

#include <type_traits>

#include "components/uuid.h"
#include "core/entity.h"
#include "ecs/ecs.h"
#include "serialization/fwd.h"
#include "serialization/json_archiver.h"

namespace ptgn {

template <typename Archiver, bool is_const>
using Entities = ecs::EntityContainer<Entity, Archiver, is_const, ecs::impl::LoopCriterion::None>;

template <typename Archiver, bool is_const, typename... TComponents>
using EntitiesWith = ecs::EntityContainer<
	Entity, Archiver, is_const, ecs::impl::LoopCriterion::WithComponents, TComponents...>;

template <typename Archiver, bool is_const, typename... TComponents>
using EntitiesWithout = ecs::EntityContainer<
	Entity, Archiver, is_const, ecs::impl::LoopCriterion::WithoutComponents, TComponents...>;

class Manager : private ecs::Manager<JSONArchiver> {
public:
	Manager()							   = default;
	Manager(const Manager&)				   = default;
	Manager& operator=(const Manager&)	   = default;
	Manager(Manager&&) noexcept			   = default;
	Manager& operator=(Manager&&) noexcept = default;
	~Manager()							   = default;

	friend bool operator==(const Manager& a, const Manager& b) {
		return &a == &b;
	}

	friend bool operator!=(const Manager& a, const Manager& b) {
		return !(a == b);
	}

	void Refresh();

	void Reserve(std::size_t capacity);

	// @return {} if no entity with the given uuid exists in the manager.
	Entity GetEntityByUUID(const UUID& uuid) const;

	// Make sure to call Refresh() after this function.
	Entity CreateEntity();

	// Make sure to call Refresh() after this function.
	// Creates an entity with a specific uuid.
	Entity CreateEntity(UUID uuid);

	// Make sure to call Refresh() after this function.
	// Creates an entity from a json object.
	Entity CreateEntity(const json& j);

	template <typename... Ts>
	void CopyEntity(const Entity& from, Entity& to) {
		ecs::Manager<JSONArchiver>::CopyEntity<UUID>(from, to);
		ecs::Manager<JSONArchiver>::CopyEntity<Ts...>(from, to);
	}

	// Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(const Entity& from) {
		auto entity{ ecs::Manager<JSONArchiver>::CopyEntity<Ts...>(from) };
		entity.template Add<UUID>();
		return entity;
	}

	template <typename... Ts>
	ptgn::EntitiesWith<JSONArchiver, true, Ts...> EntitiesWith() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, true, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWith<JSONArchiver, false, Ts...> EntitiesWith() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<JSONArchiver, true, Ts...> EntitiesWithout() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, true, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<JSONArchiver, false, Ts...> EntitiesWithout() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	ptgn::Entities<JSONArchiver, true> Entities() const {
		return { this, next_entity_, ecs::impl::Pools<Entity, JSONArchiver, true>{} };
	}

	ptgn::Entities<JSONArchiver, false> Entities() {
		return { this, next_entity_, ecs::impl::Pools<Entity, JSONArchiver, false>{} };
	}

	[[nodiscard]] std::size_t Size() const;

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] std::size_t Capacity() const;

	void Clear();

	void Reset();

	friend void to_json(json& j, const Manager& manager);
	friend void from_json(const json& j, Manager& manager);

private:
	explicit Manager(ecs::Manager<JSONArchiver>&& manager) :
		ecs::Manager<JSONArchiver>{ std::move(manager) } {}

	friend class Entity;
};

} // namespace ptgn

namespace ecs::impl {

void to_json(json& j, const DynamicBitset& bitset);

void from_json(const json& j, DynamicBitset& bitset);

} // namespace ecs::impl