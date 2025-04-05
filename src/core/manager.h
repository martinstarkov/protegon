#pragma once

#include <type_traits>

#include "core/entity.h"
#include "core/uuid.h"
#include "ecs/ecs.h"
#include "serialization/fwd.h"

namespace ptgn {

template <bool is_const>
using Entities = ecs::EntityContainer<Entity, is_const, ecs::impl::LoopCriterion::None>;

template <bool is_const, typename... TComponents>
using EntitiesWith = ecs::EntityContainer<
	Entity, is_const, ecs::impl::LoopCriterion::WithComponents, TComponents...>;

template <bool is_const, typename... TComponents>
using EntitiesWithout = ecs::EntityContainer<
	Entity, is_const, ecs::impl::LoopCriterion::WithoutComponents, TComponents...>;

class Manager : private ecs::Manager {
public:
	Manager()							   = default;
	Manager(const Manager&)				   = delete;
	Manager& operator=(const Manager&)	   = delete;
	Manager(Manager&&) noexcept			   = default;
	Manager& operator=(Manager&&) noexcept = default;
	~Manager()							   = default;

	friend bool operator==(const Manager& a, const Manager& b) {
		return &a == &b;
	}

	friend bool operator!=(const Manager& a, const Manager& b) {
		return !(a == b);
	}

	[[nodiscard]] Manager Clone() const;

	void Refresh();

	void Reserve(std::size_t capacity);

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
		ecs::Manager::CopyEntity<UUID>(from, to);
		ecs::Manager::CopyEntity<Ts...>(from, to);
	}

	// Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(const Entity& from) {
		auto entity{ ecs::Manager::CopyEntity<Ts...>(from) };
		entity.Add<UUID>();
		return entity;
	}

	template <typename... Ts>
	ptgn::EntitiesWith<true, Ts...> EntitiesWith() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, true, Ts...>{
					 ecs::Manager::GetPool<Ts>(ecs::Manager::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWith<false, Ts...> EntitiesWith() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, false, Ts...>{
					 ecs::Manager::GetPool<Ts>(ecs::Manager::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<true, Ts...> EntitiesWithout() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, true, Ts...>{
					 ecs::Manager::GetPool<Ts>(ecs::Manager::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<false, Ts...> EntitiesWithout() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, false, Ts...>{
					 ecs::Manager::GetPool<Ts>(ecs::Manager::GetId<Ts>())... } };
	}

	ptgn::Entities<true> Entities() const {
		return { this, next_entity_, ecs::impl::Pools<Entity, true>{} };
	}

	ptgn::Entities<false> Entities() {
		return { this, next_entity_, ecs::impl::Pools<Entity, false>{} };
	}

	[[nodiscard]] std::size_t Size() const;

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] std::size_t Capacity() const;

	void Clear();

	void Reset();

private:
	explicit Manager(ecs::Manager&& manager) : ecs::Manager{ std::move(manager) } {}

	friend class Entity;
};

} // namespace ptgn