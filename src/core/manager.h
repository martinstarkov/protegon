#pragma once

#include <type_traits>

#include "components/utility.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "ecs/ecs.h"
#include "serialization/fwd.h"
#include "serialization/json_archiver.h"

namespace ptgn {

class Scene;
class SceneInput;
class Physics;

namespace impl {

class RenderData;

} // namespace impl

template <bool is_const>
using Entities =
	ecs::impl::EntityContainer<Entity, JSONArchiver, is_const, ecs::impl::LoopCriterion::None>;

template <bool is_const, typename... TComponents>
using EntitiesWith = ecs::impl::EntityContainer<
	Entity, JSONArchiver, is_const, ecs::impl::LoopCriterion::WithComponents, TComponents...>;

template <bool is_const, typename... TComponents>
using EntitiesWithout = ecs::impl::EntityContainer<
	Entity, JSONArchiver, is_const, ecs::impl::LoopCriterion::WithoutComponents, TComponents...>;

class Manager : private ecs::impl::Manager<JSONArchiver> {
private:
	using Parent = ecs::impl::Manager<JSONArchiver>;

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
		Parent::GetOrAddPool<T>(Parent::GetId<T>());
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
		Parent::CopyEntity<UUID>(from, to);
		Parent::CopyEntity<Ts...>(from, to);
	}

	// Make sure to call Refresh() after this function.
	template <typename... Ts>
	Entity CopyEntity(const Entity& from) {
		auto entity{ Parent::CopyEntity<Ts...>(from) };
		entity.template Add<UUID>();
		return entity;
	}

	template <typename... Ts>
	ptgn::EntitiesWith<true, Ts...> EntitiesWith() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, true, Ts...>{
					 Parent::GetOrAddPool<Ts>(Parent::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWith<false, Ts...> EntitiesWith() {
		static_assert(
			(impl::is_retrievable_component_v<Ts> && ...),
			"Cannot retrieve entities with a component that cannot be retrieved manually"
		);
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 Parent::GetOrAddPool<Ts>(Parent::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<true, Ts...> EntitiesWithout() const {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, true, Ts...>{
					 Parent::GetOrAddPool<Ts>(Parent::GetId<Ts>())... } };
	}

	template <typename... Ts>
	ptgn::EntitiesWithout<false, Ts...> EntitiesWithout() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 Parent::GetOrAddPool<Ts>(Parent::GetId<Ts>())... } };
	}

	ptgn::Entities<true> Entities() const {
		return { this, next_entity_, ecs::impl::Pools<Entity, JSONArchiver, true>{} };
	}

	ptgn::Entities<false> Entities() {
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
	[[nodiscard]] ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& OnConstruct() {
		return Parent::OnConstruct<T>();
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
	[[nodiscard]] ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& OnDestruct() {
		return Parent::OnDestruct<T>();
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
	[[nodiscard]] ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& OnUpdate() {
		return Parent::OnUpdate<T>();
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
	[[nodiscard]] bool HasOnConstruct(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook
	) const {
		return Parent::HasOnConstruct<T>(hook);
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
	[[nodiscard]] bool HasOnDestruct(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook
	) const {
		return Parent::HasOnDestruct<T>(hook);
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
	[[nodiscard]] bool HasOnUpdate(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook
	) const {
		return Parent::HasOnUpdate<T>(hook);
	}

	/**
	 * @brief Removes a previously added construct hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnConstruct(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook) {
		Parent::RemoveOnConstruct<T>(hook);
	}

	/**
	 * @brief Removes a previously added destruct hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnDestruct(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook) {
		Parent::RemoveOnDestruct<T>(hook);
	}

	/**
	 * @brief Removes a previously added update hook for the specified component type.
	 *
	 * @tparam T The component type the hook was registered to.
	 * @param hook The hook instance to remove.
	 */
	template <typename T>
	void RemoveOnUpdate(const ecs::Hook<void, ecs::impl::Entity<JSONArchiver>>& hook) {
		Parent::RemoveOnUpdate<T>(hook);
	}

	friend void to_json(json& j, const Manager& manager);
	friend void from_json(const json& j, Manager& manager);

private:
	friend class Entity;
	friend class Scene;
	friend class SceneInput;
	friend class impl::RenderData;
	friend class Physics;

	// Same as EntitiesWith except allows non-retrievable components to be retrieved. Used for
	// internal engine systems.
	template <typename... Ts>
	ptgn::EntitiesWith<false, Ts...> InternalEntitiesWith() {
		return { this, next_entity_,
				 ecs::impl::Pools<Entity, JSONArchiver, false, Ts...>{
					 Parent::GetOrAddPool<Ts>(Parent::GetId<Ts>())... } };
	}

	void ClearEntities() final;

	explicit Manager(Parent&& manager) : Parent{ std::move(manager) } {}
};

} // namespace ptgn

namespace ecs::impl {

void to_json(json& j, const DynamicBitset& bitset);

void from_json(const json& j, DynamicBitset& bitset);

} // namespace ecs::impl