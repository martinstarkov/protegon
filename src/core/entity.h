#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_set>

#include "common/assert.h"
#include "common/move_direction.h"
#include "common/type_info.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/script.h"
#include "core/timer.h"
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

class Manager;
class Scene;

namespace impl {

class RenderData;
class IScript;

struct IgnoreParentTransform : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	IgnoreParentTransform() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(IgnoreParentTransform, value_)
};

template <typename T>
inline constexpr bool is_retrievable_component_v{
	!tt::is_any_of_v<T, Transform, Depth, Enabled, Visible, IDrawable>
};

} // namespace impl

class Entity : private ecs::impl::Entity<JSONArchiver> {
private:
	using Parent = ecs::impl::Entity<JSONArchiver>;

public:
	// Entity wrapper functionality.

	using Parent::Entity;

	Entity(const Parent& e);

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	~Entity()							 = default;

	explicit Entity(Scene& scene);

	[[nodiscard]] ecs::impl::Index GetId() const;

	explicit operator bool() const {
		return Parent::operator bool();
	}

	friend bool operator==(const Entity& a, const Entity& b) {
		return static_cast<const Parent&>(a) == static_cast<const Parent&>(b);
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return a null entity.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	[[nodiscard]] Entity Copy() {
		return Parent::Copy<Ts...>();
	}

	// Adds or replaces the component if the entity already has it.
	// @return Reference to the added or replaced component.
	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return Parent::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	// Only adds the component if one does not exist on the entity.
	// @return Reference to the added or existing component.
	template <typename T, typename... Ts>
	T& TryAdd(Ts&&... constructor_args) {
		return Parent::TryAdd<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		Parent::Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return Parent::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return Parent::HasAny<Ts...>();
	}

	template <typename... Ts, tt::enable<(impl::is_retrievable_component_v<Ts> && ...)> = true>
	[[nodiscard]] decltype(auto) Get() const {
		return GetImpl<Ts...>();
	}

	template <typename... Ts, tt::enable<(impl::is_retrievable_component_v<Ts> && ...)> = true>
	[[nodiscard]] decltype(auto) Get() {
		return GetImpl<Ts...>();
	}

	template <typename T, tt::enable<impl::is_retrievable_component_v<T>> = true>
	[[nodiscard]] const T* TryGet() const {
		return TryGetImpl<T>();
	}

	template <typename T, tt::enable<impl::is_retrievable_component_v<T>> = true>
	[[nodiscard]] T* TryGet() {
		return TryGetImpl<T>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	Entity& Destroy();

	[[nodiscard]] const Scene& GetScene() const;

	[[nodiscard]] Scene& GetScene();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] Manager& GetManager();

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

	void IgnoreParentTransform(bool ignore_parent_transform);

	void SetParent(Entity& parent, bool ignore_parent_transform = false);

	void RemoveParent();

	// Creates a child in the same manager as the parent entity.
	// @param name Optional name to retrieve the child handle by later. Note: If no name is
	// provided, the returned handle will be the only way to access the child directly.
	Entity CreateChild(std::string_view name = {});

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

	// If true, enables the entity to trigger interaction scripts.
	// Note: that setting interactive to true will also enable the entity but not the other way
	// around.
	// @return *this.
	Entity& SetInteractive(bool interactive = true);

	// If not enabled, removes the entity from the scene update list.
	// @return *this.
	Entity& SetEnabled(bool enabled = true);

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
	[[nodiscard]] Transform& GetTransform();

	// @return The absolute transform of the entity with respect to its parent scene camera
	// transform.
	[[nodiscard]] Transform GetAbsoluteTransform() const;

	// @return The transform of the entity used for drawing it with respect to its parent scene
	// camera transform. This includes any shake of bounce offsets which are only used for
	// rendering.
	[[nodiscard]] Transform GetDrawTransform() const;

	Entity& SetDrawOffset(const V2_float& offset = {});

	[[nodiscard]] std::optional<V2_float> GetRotationCenter() const;

	Entity& AddPostFX(Entity post_fx);

	Entity& AddPreFX(Entity pre_fx);

	// Set the relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	// @return *this.
	Entity& SetPosition(const V2_float& position);

	// @return The relative position of the entity with respect to its parent entity, camera, or
	// scene camera position.
	[[nodiscard]] V2_float GetPosition() const;
	[[nodiscard]] V2_float& GetPosition();

	// @return The absolute position of the entity with respect to its parent scene camera position.
	[[nodiscard]] V2_float GetAbsolutePosition() const;

	// Set the relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	// @return *this.
	Entity& SetRotation(float rotation);

	// @return The relative rotation of the entity with respect to its parent entity, camera, or
	// scene camera rotation. Clockwise positive. Unit: Radians.
	[[nodiscard]] float GetRotation() const;
	[[nodiscard]] float& GetRotation();

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
	[[nodiscard]] V2_float& GetScale();

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
	 * arguments. If the same script type T already exists on the entity, nothing happens.
	 *
	 * @tparam T The script type to be added.
	 * @tparam TArgs The types of arguments to pass to the script constructor.
	 * @param args Arguments forwarded to the script's constructor.
	 * @return A reference to the newly added script.
	 */
	template <typename T, typename... TArgs>
	T& AddScript(TArgs&&... args);

	template <auto TCallback, typename... TArgs>
	void InvokeScript(TArgs&&... args) const;

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

	template <typename T>
	[[nodiscard]] const ScriptTimerInfo& GetTimerScriptInfo() const;

	template <typename T>
	[[nodiscard]] ScriptTimerInfo& GetTimerScriptInfo();

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
	 * infinite execution, which can be stopped with RemoveScript<T>()).
	 * @param execute_immediately If true, the script is executed once immediately before the first
	 * delay.
	 * @param args Arguments forwarded to the script's constructor.
	 * @return A reference to the newly added repeat script.
	 */
	template <typename T, typename... TArgs>
	T& AddRepeatScript(
		milliseconds execution_delay, int execution_count, bool execute_immediately, TArgs&&... args
	);

	template <typename T>
	[[nodiscard]] const ScriptRepeatInfo& GetRepeatScriptInfo() const;

	template <typename T>
	[[nodiscard]] ScriptRepeatInfo& GetRepeatScriptInfo();

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
	 * @return A const reference to the base script.
	 * @throws Assert if the script is not found.
	 */
	template <typename T>
	[[nodiscard]] const T& GetScript() const;

	/**
	 * @brief Retrieves a mutable reference to the script of the specified type.
	 *
	 * @tparam T The script type to retrieve.
	 * @return A reference to the base script.
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
	[[nodiscard]] json Serialize() const {
		PTGN_ASSERT(*this, "Cannot serialize a null entity");

		json j{};

		if constexpr (sizeof...(Ts) == 0) {
			SerializeAllImpl(j);
		} else {
			(SerializeImpl<Ts>(j), ...);
		}

		return j;
	}

	// Populates the entity's components based on a JSON object. Does not impact existing
	// components, unless they are specified as part of Ts, in which case they are replaced.
	template <typename... Ts>
	void Deserialize(const json& j) {
		if constexpr (sizeof...(Ts) == 0) {
			DeserializeAllImpl(j);
		} else {
			PTGN_ASSERT(*this, "Cannot deserialize to a null entity");
			(DeserializeImpl<Ts>(j), ...);
		}
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return GetImpl<T>();
		}
		return T{ std::forward<TArgs>(args)... };
	}

	template <typename T, typename... TArgs>
	[[nodiscard]] T GetOrParentOrDefault(TArgs&&... args) const {
		if (Has<T>()) {
			return GetImpl<T>();
		}
		if (HasParent()) {
			return GetParent().GetOrParentOrDefault<T>(std::forward<TArgs>(args)...);
		}
		return T{ std::forward<TArgs>(args)... };
	}

	// @return True if *this was created before other.
	[[nodiscard]] bool WasCreatedBefore(const Entity& other) const;

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

private:
	friend class Manager;
	friend class impl::RenderData;

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() const {
		return Parent::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) GetImpl() {
		return Parent::Get<Ts...>();
	}

	template <typename T>
	[[nodiscard]] const T* TryGetImpl() const {
		return Parent::TryGet<T>();
	}

	template <typename T>
	[[nodiscard]] T* TryGetImpl() {
		return Parent::TryGet<T>();
	}

	template <typename T>
	void SerializeImpl(json& j) const {
		static_assert(
			tt::has_to_json_v<T>, "Component has not been registered with the serializer"
		);
		PTGN_ASSERT(Has<T>(), "Entity must have component which is being serialized");
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		j[component_name] = GetImpl<T>();
	}

	void SerializeAllImpl(json& j) const;

	template <typename T>
	void DeserializeImpl(const json& j) {
		static_assert(
			tt::has_from_json_v<T>, "Component has not been registered with the serializer"
		);
		constexpr auto component_name{ type_name_without_namespaces<T>() };
		PTGN_ASSERT(j.contains(component_name), "JSON does not contain ", component_name);
		j[component_name].get_to(TryAdd<T>());
	}

	void DeserializeAllImpl(const json& j);

	void AddChildImpl(Entity& child, std::string_view name = {});

	void SetParentImpl(Entity& parent);

	void RemoveParentImpl();
};

// TODO: Move elsewhere once IScript is not tied to entity.
struct Collision {
	Collision() = default;

	Collision(const Entity& other, const V2_float& collision_normal) :
		entity{ other }, normal{ collision_normal } {}

	Entity entity;
	// Normal set to {} for overlap only collisions.
	V2_float normal;

	friend bool operator==(const Collision& a, const Collision& b) {
		return a.entity == b.entity;
	}

	friend bool operator!=(const Collision& a, const Collision& b) {
		return !operator==(a, b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Collision, entity, normal)
};

namespace impl {

// TODO: Figure out a way to move IScript to a different file.

class IScript {
public:
	Entity entity;

	virtual ~IScript() = default;

	// Lifecycle.

	virtual void OnCreate() { /* user implementation */
	} // Called when script is first instantiated.

	virtual void OnDestroy() { /* user implementation */ } // Called before script is destroyed.

	virtual void OnEnable() { /* user implementation */ }  // Called when entity is enabled.

	virtual void OnDisable() { /* user implementation */ } // Called when entity is disabled.

	virtual void OnShow() { /* user implementation */ }	   // Called when entity is shown.

	virtual void OnHide() { /* user implementation */ }	   // Called when entity is hidden.

	// Timed script triggers.

	virtual void OnTimerStart() { /* user implementation */ }

	virtual void OnTimerUpdate([[maybe_unused]] float elapsed_fraction) { /* user implementation */
	}

	// @return True if the timer should be removed from the entity after it finishes.
	virtual bool OnTimerStop() { /* user implementation */
		return true;
	}

	// Repeated script triggers.

	virtual void OnRepeatStart() { /* user implementation */ }

	// @param repeat starts from 0.
	virtual void OnRepeatUpdate([[maybe_unused]] int repeat) { /* user implementation */ }

	virtual void OnRepeatStop() { /* user implementation */ }

	virtual void OnUpdate([[maybe_unused]] float dt) { /* user implementation */
	} // Called every frame.

	// TODO: Consider implementing?
	// virtual void OnFixedUpdate([[maybe_unused]] float fixed_dt) {} // Called at fixed intervals
	// (physics).

	// Keyboard events.

	virtual void OnKeyDown([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyPressed([[maybe_unused]] Key key) { /* user implementation */ }

	virtual void OnKeyUp([[maybe_unused]] Key key) { /* user implementation */ }

	// Mouse events.

	virtual void OnMouseDown([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseDownOutside([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseMove([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnMouseEnter([[maybe_unused]] V2_float mouse_position) { /* user implementation */
	}

	virtual void OnMouseLeave([[maybe_unused]] V2_float mouse_position) { /* user implementation */
	}

	virtual void OnMouseOut([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnMouseOver([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnMouseUp([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMouseUpOutside([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	virtual void OnMousePressed([[maybe_unused]] Mouse mouse) { /* user implementation */ }

	// Scroll amount in each direction.
	virtual void OnMouseScroll([[maybe_unused]] V2_int scroll_amount) { /* user implementation */ }

	// Draggable events.

	// Triggered when the user start holding left click over a draggable interactive object.
	virtual void OnDragStart([[maybe_unused]] V2_float start_position) { /* user implementation */ }

	// Triggered when the user lets go of left click while dragging a draggable interactive object.
	virtual void OnDragStop([[maybe_unused]] V2_float stop_position) { /* user implementation */ }

	// Triggered every frame while the user is holding left click over a draggable interactive
	// object.
	virtual void OnDrag([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnDragEnter([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnDragLeave([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnDragOver([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	virtual void OnDragOut([[maybe_unused]] V2_float mouse_position) { /* user implementation */ }

	// Triggered when the user lets go (by releasing left click) of a draggable interactive object
	// while over top of a dropzone interactive object.
	virtual void OnDrop([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// Triggered when the user picks up (by pressing left click) a draggable interactive object
	// while over top of a dropzone interactive object.
	virtual void OnPickup([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	// TODO: Consider adding OnDrop event(s).

	// Animation events.

	virtual void OnAnimationStart() { /* user implementation */ }

	virtual void OnAnimationUpdate() { /* user implementation */ }

	// Called for each repeat of the full animation.
	// @param repeat Starts from 0.
	virtual void OnAnimationRepeat([[maybe_unused]] std::size_t repeat) { /* user implementation */
	}

	// Called when the frame of the animation changes
	virtual void OnAnimationFrameChange([[maybe_unused]] std::size_t new_frame
	) { /* user implementation */ }

	// Called once when the animation goes through its first full cycle.
	virtual void OnAnimationComplete() { /* user implementation */ }

	virtual void OnAnimationPause() { /* user implementation */ }

	virtual void OnAnimationResume() { /* user implementation */ }

	virtual void OnAnimationStop() { /* user implementation */ }

	// Called every frame that the player is moving.
	virtual void OnMove() { /* user implementation */ }

	// Called on the first frame of player movement.
	virtual void OnMoveStart() { /* user implementation */ }

	// Called on the first frame of player stopping their movement.
	virtual void OnMoveStop() { /* user implementation */ }

	// Called when the movement direction changes. Passed parameter is the difference in direction.
	// If not moving, this is simply the new direction. If moving already, this is the newly added
	// component of movement. To get the current direction instead, simply use GetDirection().
	virtual void OnMoveDirectionChange([[maybe_unused]] MoveDirection direction_difference
	) { /* user implementation */ }

	virtual void OnMoveUp() { /* user implementation */ }

	virtual void OnMoveDown() { /* user implementation */ }

	virtual void OnMoveLeft() { /* user implementation */ }

	virtual void OnMoveRight() { /* user implementation */ }

	virtual void OnMoveUpStart() { /* user implementation */ }

	virtual void OnMoveDownStart() { /* user implementation */ }

	virtual void OnMoveLeftStart() { /* user implementation */ }

	virtual void OnMoveRightStart() { /* user implementation */ }

	virtual void OnMoveUpStop() { /* user implementation */ }

	virtual void OnMoveDownStop() { /* user implementation */ }

	virtual void OnMoveLeftStop() { /* user implementation */ }

	virtual void OnMoveRightStop() { /* user implementation */ }

	// Must return true for collision to be checked.
	// Defaults to true.
	virtual bool PreCollisionCondition([[maybe_unused]] Entity other) {
		return true;
	}

	virtual void OnCollisionStart([[maybe_unused]] Collision collision) { /* user implementation */
	}

	virtual void OnCollision([[maybe_unused]] Collision collision) { /* user implementation */ }

	virtual void OnCollisionStop([[maybe_unused]] Collision collision) { /* user implementation */ }

	// Button events.

	virtual void OnButtonHoverStart() { /* user implementation */ }

	virtual void OnButtonHoverStop() { /* user implementation */ }

	virtual void OnButtonActivate() { /* user implementation */ }

	// Serialization (do not override these, as this is handled automatically by the
	// ScriptRegistry).
	virtual json Serialize() const			= 0;
	virtual void Deserialize(const json& j) = 0;
};

} // namespace impl

class Scripts : public impl::ScriptContainer<impl::IScript> {
public:
	static void Update(Scene& scene, float dt);

	template <auto TCallback, typename... TArgs>
	void Invoke(TArgs&&... args) const {
		for (const auto& [key, script] : scripts) {
			PTGN_ASSERT(script != nullptr, "Cannot invoke nullptr script");
			std::invoke(TCallback, script, std::forward<TArgs>(args)...);
		}
	}
};

template <typename T>
using Script = impl::Script<T, impl::IScript>;

template <auto TCallback, typename... TArgs>
void Entity::InvokeScript(TArgs&&... args) const {
	if (!Has<Scripts>()) {
		return;
	}

	const auto& scripts{ Get<Scripts>() };

	scripts.Invoke<TCallback>(std::forward<TArgs>(args)...);
}

template <typename T, typename... TArgs>
T& Entity::AddScript(TArgs&&... args) {
	auto& scripts{ TryAdd<Scripts>() };

	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };

	script.entity = *this;
	script.OnCreate();

	return script;
}

template <typename T, typename TScriptType>
[[nodiscard]] const auto& GetScriptInfo(const Entity& entity) {
	PTGN_ASSERT(entity.Has<Scripts>(), "Entity has no scripts");

	[[maybe_unused]] const auto& script{ entity.Get<Scripts>() };

	constexpr auto class_name{ type_name<T>() };

	PTGN_ASSERT(script.HasScript<T>(), "Entity does not have the specified script: ", class_name);

	PTGN_ASSERT(entity.Has<TScriptType>(), "Entity does not have script info for ", class_name);

	const auto& scripts{ entity.Get<TScriptType>() };

	return scripts;
}

template <typename T>
const ScriptTimerInfo& Entity::GetTimerScriptInfo() const {
	const auto& scripts{ GetScriptInfo<T, impl::ScriptTimers>(*this) };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	auto it{ scripts.timers.find(hash) };

	PTGN_ASSERT(
		it != scripts.timers.end(), "Entity script ", class_name, " does not have timer info"
	);

	return it->second;
}

template <typename T>
ScriptTimerInfo& Entity::GetTimerScriptInfo() {
	return const_cast<ScriptTimerInfo&>(std::as_const(*this).GetTimerScriptInfo<T>());
}

template <typename T>
const ScriptRepeatInfo& Entity::GetRepeatScriptInfo() const {
	const auto& scripts{ GetScriptInfo<T, impl::ScriptRepeats>(*this) };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	auto it{ scripts.repeats.find(hash) };

	PTGN_ASSERT(
		it != scripts.repeats.end(), "Entity script ", class_name, " does not have repeat info"
	);

	return it->second;
}

template <typename T>
ScriptRepeatInfo& Entity::GetRepeatScriptInfo() {
	return const_cast<ScriptRepeatInfo&>(std::as_const(*this).GetRepeatScriptInfo<T>());
}

template <typename T, typename... TArgs>
T& Entity::AddTimerScript(milliseconds execution_duration, TArgs&&... args) {
	auto& script{ AddScript<T>(std::forward<TArgs>(args)...) };

	PTGN_ASSERT(
		execution_duration >= milliseconds{ 0 }, "Timer script must have a positive duration"
	);

	auto& timer_scripts{ TryAdd<impl::ScriptTimers>() };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	timer_scripts.timers.emplace(hash, ScriptTimerInfo{ Timer{ true }, execution_duration });

	script.OnTimerStart();

	script.OnTimerUpdate(0.0f);

	return script;
}

template <typename T, typename... TArgs>
T& Entity::AddRepeatScript(
	milliseconds execution_delay, int execution_count, bool execute_immediately, TArgs&&... args
) {
	auto& script{ AddScript<T>(std::forward<TArgs>(args)...) };

	PTGN_ASSERT(
		execution_delay >= milliseconds{ 0 }, "Repeat script must have a positive execution_delay"
	);

	bool infinite_execution{ execution_count == -1 };

	PTGN_ASSERT(
		infinite_execution || execution_count > 0,
		"Repeated script execution count must be above 0 or -1 for infinite execution"
	);

	script.OnRepeatStart();

	int current_executions{ 0 };

	if (execute_immediately) {
		script.OnRepeatUpdate(current_executions);
		current_executions++;

		if (!infinite_execution && current_executions >= execution_count) {
			script.OnRepeatStop();
			return script;
		}

		// More than one script execution requested.
	}

	auto& repeat_scripts{ TryAdd<impl::ScriptRepeats>() };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	repeat_scripts.repeats.emplace(
		hash,
		ScriptRepeatInfo{ Timer{ true }, execution_delay, current_executions, execution_count }
	);

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

	if (Has<impl::ScriptTimers>()) {
		// TODO: Consider replacing with a different callback / hook for early timer stop?
		script.OnTimerStop();

		auto& timers{ Get<impl::ScriptTimers>().timers };

		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };

		timers.erase(hash);

		if (timers.empty()) {
			Remove<impl::ScriptTimers>();
		}
	}

	if (Has<impl::ScriptRepeats>()) {
		// TODO: Consider replacing with a different callback / hook for early repeat stop?
		script.OnRepeatStop();

		auto& repeats{ Get<impl::ScriptRepeats>().repeats };

		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };

		repeats.erase(hash);

		if (repeats.empty()) {
			Remove<impl::ScriptRepeats>();
		}
	}

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

struct PostFX {
	PostFX() = default;

	std::unordered_set<Entity> post_fx_;

	friend bool operator==(const PostFX& a, const PostFX& b) {
		return a.post_fx_ == b.post_fx_;
	}

	friend bool operator!=(const PostFX& a, const PostFX& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_NAMED(PostFX, KeyValue("post_fx", post_fx_))
};

struct PreFX {
	PreFX() = default;

	std::unordered_set<Entity> pre_fx_;

	friend bool operator==(const PreFX& a, const PreFX& b) {
		return a.pre_fx_ == b.pre_fx_;
	}

	friend bool operator!=(const PreFX& a, const PreFX& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_NAMED(PreFX, KeyValue("pre_fx", pre_fx_))
};

} // namespace impl

} // namespace ptgn