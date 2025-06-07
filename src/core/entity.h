#pragma once

#include <string_view>
#include <unordered_set>

#include "common/function.h"
#include "common/type_info.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/script.h"
#include "ecs/ecs.h"
#include "events/key.h"
#include "events/mouse.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/origin.h"
#include "serialization/fwd.h"
#include "serialization/json_archiver.h"
#include "serialization/serializable.h"

// TODO: Add tests for entity hierarchy functions.

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class Manager;

class Entity : private ecs::Entity<JSONArchiver> {
public:
	// Entity wrapper functionality.

	using ecs::Entity<JSONArchiver>::Entity;

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	virtual ~Entity()					 = default;

	explicit Entity(Manager& manager);

	explicit operator bool() const {
		return ecs::Entity<JSONArchiver>::operator bool();
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return static_cast<const ecs::Entity<JSONArchiver>&>(a) ==
			   static_cast<const ecs::Entity<JSONArchiver>&>(b);
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	[[nodiscard]] Entity Copy() {
		return ecs::Entity<JSONArchiver>::Copy<Ts...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return ecs::Entity<JSONArchiver>::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <typename T, typename... Ts>
	T& TryAdd(Ts&&... constructor_args) {
		if (!Has<T>()) {
			return Add<T>(std::forward<Ts>(constructor_args)...);
		}
		return Get<T>();
	}

	// Get a component of the entity, and if it does not exist add it.
	template <typename T, typename... Ts>
	T& GetOrAdd(Ts&&... constructor_args) {
		if (Has<T>()) {
			return Get<T>();
		}
		return Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		ecs::Entity<JSONArchiver>::Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return ecs::Entity<JSONArchiver>::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return ecs::Entity<JSONArchiver>::HasAny<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() const {
		return ecs::Entity<JSONArchiver>::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() {
		return ecs::Entity<JSONArchiver>::Get<Ts...>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	Entity& Destroy();

	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Entity property functions.

	[[nodiscard]] UUID GetUUID() const;

	[[nodiscard]] std::size_t GetHash() const;

	template <typename T, tt::enable<tt::has_static_draw_v<T>> = true>
	Entity& SetDraw() {
		Add<IDrawable>(type_name<T>());
		return *this;
	}

	[[nodiscard]] bool HasDraw() const;

	Entity& RemoveDraw();

	// Entity hierarchy functions.

	// @return The parent most entity, or *this if no parent exists.
	[[nodiscard]] Entity GetRootEntity() const;

	// @return Parent entity of the object. If object has no parent, returns *this.
	[[nodiscard]] Entity GetParent() const;

	[[nodiscard]] bool HasParent() const;

	void SetParent(Entity& parent);

	void RemoveParent();

	// Creates a child in the same manager as the parent entity.
	// @param name Optional name to retrieve the child handle by later. Note: If no name is
	// provided, the returned handle will be the only way to access the child directly.
	[[nodiscard]] Entity CreateChild(std::string_view name = {});

	void ClearChildren();

	void AddChild(Entity& child, std::string_view name = {});

	void RemoveChild(Entity& child);
	void RemoveChild(std::string_view name);

	// @return True if the entity has the given child, false otherwise.
	[[nodiscard]] bool HasChild(const Entity& child) const;
	[[nodiscard]] bool HasChild(std::string_view name) const;

	// @return Child entity with the given name, or null entity is no such child exists.
	[[nodiscard]] Entity GetChild(std::string_view name) const;

	// @return All childs entities tied to the object
	[[nodiscard]] std::unordered_set<Entity> GetChildren() const;

	// Entity activation functions.

	// If not enabled, removes the entity from the scene update list.
	// @return *this.
	Entity& SetEnabled(bool enabled);

	// Removes the entity from the scene update list.
	// @return *this.
	Entity& Disable();

	// Add the entity to the scene update list (if not already there).
	// @return *this.
	Entity& Enable();

	// @return True if the entity is in the scene update list.
	[[nodiscard]] bool IsEnabled() const;

	// Entity transform functions.

	// Set the relative transform of the entity with respect to its parent entity, camera, or
	// scene camera transform.
	// @return *this.
	Entity& SetTransform(const Transform& transform);

	// @return The relative transform of the entity with respect to its parent entity, camera, or
	// scene camera transform.
	[[nodiscard]] Transform GetTransform() const;

	// @return The absolute transform of the entity with respect to its parent scene camera
	// transform.
	[[nodiscard]] Transform GetAbsoluteTransform() const;

	// Set the relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	// @return *this.
	Entity& SetPosition(const V2_float& position);

	// @return The relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	[[nodiscard]] V2_float GetPosition() const;

	// @return The absolute position of the entity with respect to its parent scene camera position.
	[[nodiscard]] V2_float GetAbsolutePosition() const;

	// Set the relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	// @return *this.
	Entity& SetRotation(float rotation);

	// @return The relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	[[nodiscard]] float GetRotation() const;

	// @return The absolute rotation of the entity with respect to its parent scene camera rotation.
	// Clockwise positive. Unit: Radians.
	[[nodiscard]] float GetAbsoluteRotation() const;

	// Set the relative scale of the entity with respect to its parent entity, camera, or
	// scene camera scale.
	// @return *this.
	Entity& SetScale(const V2_float& scale);
	Entity& SetScale(float scale);

	// @return The relative scale of the entity with respect to its parent entity, camera, or
	// scene camera scale.
	[[nodiscard]] V2_float GetScale() const;

	// @return The absolute scale of the entity with respect to its parent scene camera scale.
	[[nodiscard]] V2_float GetAbsoluteScale() const;

	Entity& SetOrigin(Origin origin);

	[[nodiscard]] Origin GetOrigin() const;

	// Draw functions.

	// @return *this.
	Entity& SetVisible(bool visible);

	// @return *this.
	Entity& Show();

	// @return *this.
	Entity& Hide();

	[[nodiscard]] bool IsVisible() const;

	Entity& SetDepth(const Depth& depth);

	[[nodiscard]] Depth GetDepth() const;

	Entity& SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] BlendMode GetBlendMode() const;

	// color::White will clear tint.
	Entity& SetTint(const Color& color = color::White);

	[[nodiscard]] Color GetTint() const;

	// Scripting.

	/**
	 * @brief Adds a script of type T to the entity.
	 *
	 * Constructs and attaches a script of the specified type using the provided constructor
	 * arguments.
	 *
	 * @tparam T The script type to be added.
	 * @tparam TArgs The types of arguments to pass to the script constructor.
	 * @param args Arguments forwarded to the script's constructor.
	 * @return A reference to the newly added script.
	 */
	template <typename T, typename... TArgs>
	T& AddScript(TArgs&&... args);

	/**
	 * @brief Adds a script that executes continuously for a specified duration.
	 *
	 * The script will be updated over the given time duration, then automatically stopped and
	 * removed.
	 *
	 * @tparam T The script type to be added.
	 * @tparam TArgs The types of arguments to pass to the script constructor.
	 * @param execution_duration The total duration (in milliseconds) the script should be executed
	 * for.
	 * @param args Arguments forwarded to the script's constructor.
	 * @return A reference to the newly added timed script.
	 */
	template <typename T, typename... TArgs>
	T& AddTimerScript(milliseconds execution_duration, TArgs&&... args);

	/**
	 * @brief Adds a script that executes repeatedly with a fixed delay between executions.
	 *
	 * The script will be called a specified number of times with a fixed delay between each call,
	 * then automatically stopped and removed. Optionally, it can be triggered immediately upon
	 * adding.
	 *
	 * @tparam T The script type to be added.
	 * @tparam TArgs The types of arguments to pass to the script constructor.
	 * @param execution_delay The delay (in milliseconds) between each script execution.
	 * @param execution_count The total number of times the script will be executed (-1 for
	 * indefinite execution, which can be stopped with RemoveScript<T>()).
	 * @param execute_immediately If true, the script is executed once immediately before the first
	 * delay.
	 * @param args Arguments forwarded to the script's constructor.
	 * @return A reference to the newly added repeat script.
	 */
	template <typename T, typename... TArgs>
	T& AddRepeatScript(
		milliseconds execution_delay, int execution_count, bool execute_immediately, TArgs&&... args
	);

	/**
	 * @brief Checks whether a script of the specified type is attached to the entity.
	 *
	 * @tparam T The script type to check.
	 * @return True if the script exists, false otherwise.
	 */
	template <typename T>
	[[nodiscard]] bool HasScript() const;

	/**
	 * @brief Retrieves a const reference to the script of the specified type.
	 *
	 * @tparam T The script type to retrieve.
	 * @return A const reference to the script.
	 * @throws Assert if the script is not found.
	 */
	template <typename T>
	[[nodiscard]] const T& GetScript() const;

	/**
	 * @brief Retrieves a mutable reference to the script of the specified type.
	 *
	 * @tparam T The script type to retrieve.
	 * @return A reference to the script.
	 * @throws Assert if the script is not found.
	 */
	template <typename T>
	[[nodiscard]] T& GetScript();

	/**
	 * @brief Removes the script of the specified type from the entity.
	 *
	 * @tparam T The script type to remove.
	 */
	template <typename T>
	void RemoveScript();

	// Serialization.

	friend void to_json(json& j, const Entity& entity);
	friend void from_json(const json& j, Entity& entity);

	// Converts the specified entity components to a JSON object.
	template <typename... Ts>
	json Serialize() const {
		static_assert(
			sizeof...(Ts) > 0, "Cannot serialize entity without providing desired components"
		);
		PTGN_ASSERT(*this, "Cannot serialize a null entity");
		json j{};
		(SerializeImpl<Ts>(j), ...);
		return j;
	}

	// Populates the entity's components based on a JSON object. Does not impact existing
	// components, unless they are specified as part of Ts, in which case they are replaced.
	template <typename... Ts, tt::enable<(sizeof...(Ts) > 0)> = true>
	void Deserialize(const json& j) {
		PTGN_ASSERT(*this, "Cannot deserialize to a null entity");
		(DeserializeImpl<Ts>(j), ...);
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return Get<T>();
		}
		return T{ std::forward<TArgs>(args)... };
	}

protected:
	template <typename T, typename... TArgs>
	Entity& AddOrRemove(bool condition, TArgs&&... args) {
		if (condition) {
			Add<T>(std::forward<TArgs>(args)...);
		} else {
			Remove<T>();
		}
		return *this;
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrParentOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return Get<T>();
		}
		if (HasParent()) {
			return GetParent().GetOrDefault<T>(std::forward<TArgs>(args)...);
		}
		return T{ std::forward<TArgs>(args)... };
	}

private:
	friend class Manager;

	template <typename T>
	void SerializeImpl(json& j) const {
		static_assert(
			tt::has_to_json_v<T>, "Component has not been registered with the serializer"
		);
		PTGN_ASSERT(Has<T>(), "Entity must have component which is being serialized");
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		j[component_name] = Get<T>();
	}

	template <typename T>
	void DeserializeImpl(const json& j) {
		static_assert(
			tt::has_from_json_v<T>, "Component has not been registered with the serializer"
		);
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		PTGN_ASSERT(j.contains(component_name), "JSON does not contain ", component_name);
		j[component_name].get_to(GetOrAdd<T>());
	}

	void AddChildImpl(Entity& child, std::string_view name = {});

	void SetParentImpl(Entity& parent);

	void RemoveParentImpl();

	explicit Entity(const ecs::Entity<JSONArchiver>& e);
};

namespace impl {

// TODO: Figure out a way to move IScript to a different file.

class IScript {
public:
	Entity entity;

	virtual ~IScript() = default;

	// Lifecycle.
	virtual void OnCreate() {}	// Called when script is first instantiated.

	virtual void OnDestroy() {} // Called before script is destroyed.

	virtual void OnEnable() {}	// Called when entity is enabled.

	virtual void OnDisable() {} // Called when entity is disabled.

	virtual void OnShow() {}	// Called when entity is shown.

	virtual void OnHide() {}	// Called when entity is hidden.

	// Timed script triggers.
	virtual void OnTimerStart() {}

	virtual void OnTimerUpdate(float elapsed_fraction) {}

	virtual void OnTimerStop() {}

	// Repeated script triggers.
	virtual void OnRepeatStart() {}

	virtual void OnRepeatUpdate(int repeat) {}

	virtual void OnRepeatStop() {}

	// Update.
	virtual void OnUpdate(float dt) {}			  // Called every frame.

	virtual void OnFixedUpdate(float fixed_dt) {} // Called at fixed intervals (physics).

	// Input.
	virtual void OnKeyPressed(Key key) {}

	virtual void OnKeyReleased(Key key) {}

	virtual void OnMouseButtonPressed(Mouse button) {}

	virtual void OnMouseButtonReleased(Mouse button) {}

	virtual void OnMouseMove(V2_int mouse_pos) {}

	virtual void OnMouseScroll(V2_int scroll_amount) {}

	// Collision / Physics.
	virtual void OnCollisionEnter(Entity other) {}

	virtual void OnCollisionStay(Entity other) {}

	virtual void OnCollisionExit(Entity other) {}

	virtual void OnTriggerEnter(Entity other) {}

	virtual void OnTriggerStay(Entity other) {}

	virtual void OnTriggerExit(Entity other) {}

	// Animation.
	virtual void OnAnimationStart() {}

	virtual void OnAnimationEnd() {}

	virtual void OnAnimationRepeat(int repeat_count) {}

	// UI / Interaction.
	virtual void OnClick() {}

	virtual void OnHoverEnter() {}

	virtual void OnHoverExit() {}

	// Serialization (do not override these, as this is handled automatically by the
	// ScriptRegistry).
	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;
};

} // namespace impl

using Scripts = impl::ScriptContainer<impl::IScript>;

template <typename T>
using Script = impl::Script<T, impl::IScript>;

template <typename TCallback, typename... TArgs>
static void Invoke(const Entity& e, TArgs&&... args) {
	if (e.Has<TCallback>()) {
		const auto& callback{ e.Get<TCallback>() };
		Invoke(callback, std::forward<TArgs>(args)...);
	}
}

template <typename T, typename... TArgs>
T& Entity::AddScript(TArgs&&... args) {
	auto& scripts{ GetOrAdd<Scripts>() };

	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };

	script.entity = *this;
	script.OnCreate();

	return script;
}

class ScriptTimers {
	std::unordered_map<std::size_t, Timer> timers;
};

template <typename T, typename... TArgs>
T& Entity::AddTimerScript(milliseconds execution_duration, TArgs&&... args) {
	auto& script{ AddScript<T>(std::forward<TArgs>(args)...) };

	PTGN_ASSERT(
		execution_duration >= milliseconds{ 0 }, "Timer script must have a positive duration"
	);

	auto& timer_scripts{ GetOrAdd<ScriptTimers>() };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	timer_scripts.emplace(hash, Timer{ true });

	// TODO: Check this all works as intended.

	script.OnTimerStart();

	return script;
}

template <typename T, typename... TArgs>
T& Entity::AddRepeatScript(
	milliseconds execution_delay, int execution_count, bool execute_immediately, TArgs&&... args
) {
	auto& script{ AddScript<T>(std::forward<TArgs>(args)...) };

	// std::unordered_map<std::size_t, RepeatInfo>
	// TODO: Add script repeat component.
	// TODO: Trigger timer start.
	// if (execute_immediately) { script->OnRepeatStart(); }

	return script;
}

template <typename T>
[[nodiscard]] bool Entity::HasScript() const {
	return Has<Scripts>() && Get<Scripts>().HasScript<T>();
}

template <typename T>
[[nodiscard]] const T& Entity::GetScript() const {
	PTGN_ASSERT(Has<Scripts>());
	auto& scripts{ Get<Scripts>() };
	return scripts.GetScript<T>();
}

template <typename T>
[[nodiscard]] T& Entity::GetScript() {
	return const_cast<T&>(std::as_const(*this).GetScript<T>());
}

template <typename T>
void Entity::RemoveScript() {
	if (!Has<Scripts>()) {
		return;
	}

	auto& scripts{ Get<Scripts>() };

	if (!scripts.HasScript<T>()) {
		return;
	}

	auto& script{ scripts.GetScript<T>() };

	script.OnDestroy();

	scripts.RemoveScript<T>();

	if (scripts.IsEmpty()) {
		Remove<Scripts>();
	}
}

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	std::size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct ChildKey : public HashComponent {
	using HashComponent::HashComponent;
};

struct Parent : public Entity {
	using Entity::Entity;

	Parent(const Entity& entity);
};

struct Children {
	Children() = default;

	void Clear();

	void Add(Entity& child, std::string_view name = {});

	void Remove(const Entity& child);
	void Remove(std::string_view name);

	// @return Entity with given name, or null entity is no such entity exists.
	[[nodiscard]] Entity Get(std::string_view name) const;

	[[nodiscard]] bool IsEmpty() const;

	[[nodiscard]] bool Has(const Entity& child) const;
	[[nodiscard]] bool Has(std::string_view name) const;

	PTGN_SERIALIZER_REGISTER_NAMED(Children, KeyValue("children", children_))

private:
	friend class ptgn::Entity;

	std::unordered_set<Entity> children_;
};

} // namespace impl

} // namespace ptgn