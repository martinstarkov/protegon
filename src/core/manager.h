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
	~Manager() override					   = default;

	friend bool operator==(const Manager& a, const Manager& b) {
		return &a == &b;
	}

	friend bool operator!=(const Manager& a, const Manager& b) {
		return !(a == b);
	}

	template <typename T>
	void RegisterType() {
		ecs::Manager<JSONArchiver>::GetOrAddPool<T>(ecs::Manager<JSONArchiver>::GetId<T>());
	}

	void Refresh();

	void Reserve(std::size_t capacity);

	// @return {} if no entity with the given uuid exists in the manager.
	[[nodiscard]] Entity GetEntityByUUID(const UUID& uuid) const;

	// Make sure to call Refresh() after this function.
	virtual Entity CreateEntity();

	// Make sure to call Refresh() after this function.
	// Creates an entity with a specific uuid.
	virtual Entity CreateEntity(UUID uuid);

	// Make sure to call Refresh() after this function.
	// Creates an entity from a json object.
	virtual Entity CreateEntity(const json& j);

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
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(
						 ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWith<JSONArchiver, false, Ts...> EntitiesWith() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(
						 ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<JSONArchiver, true, Ts...> EntitiesWithout() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, true, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(
						 ecs::Manager<JSONArchiver>::GetId<Ts>()
					 )... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<JSONArchiver, false, Ts...> EntitiesWithout() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 ecs::Manager<JSONArchiver>::GetPool<Ts>(
						 ecs::Manager<JSONArchiver>::GetId<Ts>()
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

	/**
	 * @brief Adds a construct hook for the specified component type.
	 *
	 * This hook is invoked whenever a component of type `T` is constructed.
	 * Note: Discarding the returned hook instance will make it impossible to remove the hook later.
	 *
	 * @tparam T The component type to attach the construct hook to.
	 * @return Reference to the newly added hook, which can be configured or stored for later
	 * removal.
	 */
	template <typename T>
	[[nodiscard]] Hook& OnConstruct() {
		return ecs::Manager<JSONArchiver>::OnConstruct<T>();
	}

	/**
	 * @brief Adds a destruct hook for the specified component type.
	 *
	 * This hook is invoked whenever a component of type `T` is destroyed.
	 * Note: Discarding the returned hook instance will make it impossible to remove the hook later.
	 *
	 * @tparam T The component type to attach the destruct hook to.
	 * @return Reference to the newly added hook, which can be configured or stored for later
	 * removal.
	 */
	template <typename T>
	[[nodiscard]] Hook& OnDestruct() {
		return ecs::Manager<JSONArchiver>::OnDestruct<T>();
	}

	/**
	 * @brief Adds an update hook for the specified component type.
	 *
	 * This hook is invoked during update operations on a component of type `T`.
	 * Note: Discarding the returned hook instance will make it impossible to remove the hook later.
	 *
	 * @tparam T The component type to attach the update hook to.
	 * @return Reference to the newly added hook, which can be configured or stored for later
	 * removal.
	 */
	template <typename T>
	[[nodiscard]] Hook& OnUpdate() {
		return ecs::Manager<JSONArchiver>::OnUpdate<T>();
	}

	/**
	 * @brief Checks if a specific construct hook exists for the given component type.
	 *
	 * This function allows you to verify whether the provided hook is currently registered
	 * as a construct hook for the specified component type `T`.
	 *
	 * @tparam T The component type to check.
	 * @param hook The hook to search for in the construct hook list.
	 * @return true if the hook is registered; false otherwise.
	 */
	template <typename T>
	[[nodiscard]] bool HasOnConstruct(const Hook& hook) const {
		return ecs::Manager<JSONArchiver>::HasOnConstruct<T>(hook);
	}

	/**
	 * @brief Checks if a specific destruct hook exists for the given component type.
	 *
	 * This function allows you to verify whether the provided hook is currently registered
	 * as a destruct hook for the specified component type `T`.
	 *
	 * @tparam T The component type to check.
	 * @param hook The hook to search for in the destruct hook list.
	 * @return true if the hook is registered; false otherwise.
	 */
	template <typename T>
	[[nodiscard]] bool HasOnDestruct(const Hook& hook) const {
		return ecs::Manager<JSONArchiver>::HasOnDestruct<T>(hook);
	}

	/**
	 * @brief Checks if a specific update hook exists for the given component type.
	 *
	 * This function allows you to verify whether the provided hook is currently registered
	 * as an update hook for the specified component type `T`.
	 *
	 * @tparam T The component type to check.
	 * @param hook The hook to search for in the update hook list.
	 * @return true if the hook is registered; false otherwise.
	 */
	template <typename T>
	[[nodiscard]] bool HasOnUpdate(const Hook& hook) const {
		return ecs::Manager<JSONArchiver>::HasOnUpdate<T>(hook);
	}

	/**
	 * @brief Removes a previously added construct hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnConstruct(const Hook& hook) {
		ecs::Manager<JSONArchiver>::RemoveOnConstruct<T>(hook);
	}

	/**
	 * @brief Removes a previously added destruct hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnDestruct(const Hook& hook) {
		ecs::Manager<JSONArchiver>::RemoveOnDestruct<T>(hook);
	}

	/**
	 * @brief Removes a previously added update hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnUpdate(const Hook& hook) {
		ecs::Manager<JSONArchiver>::RemoveOnUpdate<T>(hook);
	}

	friend void to_json(json& j, const Manager& manager);
	friend void from_json(const json& j, Manager& manager);

private:
	friend class Entity;

	void ClearEntities() final;

	explicit Manager(ecs::Manager<JSONArchiver>&& manager) :
		ecs::Manager<JSONArchiver>{ std::move(manager) } {}
};

} // namespace ptgn

namespace ecs::impl {

void to_json(json& j, const DynamicBitset& bitset);

void from_json(const json& j, DynamicBitset& bitset);

} // namespace ecs::impl