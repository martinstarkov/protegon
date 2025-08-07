#pragma once

#include <vector>

#include "common/assert.h"
#include "common/move_direction.h"
#include "core/entity.h"
#include "core/script_registry.h"
#include "core/time.h"
#include "core/timer.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "serialization/fwd.h"

namespace ptgn {

class Scene;

namespace impl {

class IScript {
public:
	Entity entity;

	virtual ~IScript() = default;

	// Lifecycle.

	virtual void OnCreate() { /* user implementation */
	} // Called when script is first instantiated.

	virtual void OnDestroy() { /* user implementation */ } // Called before script is destroyed.

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

	virtual void OnDragEnter([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragLeave([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOver([[maybe_unused]] Entity dropzone) { /* user implementation */ }

	virtual void OnDragOut([[maybe_unused]] Entity dropzone) { /* user implementation */ }

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

	// TODO: Fix these.

	// Must return true for collision to be checked.
	// Defaults to true.
	virtual bool PreCollisionCondition([[maybe_unused]] Entity other) {
		return true;
	}

	virtual void OnCollisionStart([[maybe_unused]] Collision collision) { /* user implementation */
	}

	virtual void OnCollision([[maybe_unused]] Collision collision) { /* user implementation */ }

	virtual void OnCollisionStop([[maybe_unused]] Collision collision) { /* user implementation */ }

	virtual void OnRaycastHit([[maybe_unused]] Collision collision) { /* user implementation */ }

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
	static void Invoke(Scene& scene, TArgs&&... args) {
		auto entities{ GetEntities(scene) };
		for (const auto& entity : entities) {
			Invoke<TCallback>(entity, std::forward<TArgs>(args)...);
		}
	}

	template <auto TCallback, typename... TArgs>
	static void Invoke(Entity entity, TArgs&&... args) {
		if (!entity.IsAlive() || !entity.Has<Scripts>()) {
			return;
		}
		// Copy on purpose to prevent iterator invalidation.
		auto scripts{ entity.Get<Scripts>().scripts };
		for (const auto& [key, script] : scripts) {
			PTGN_ASSERT(script != nullptr, "Cannot invoke nullptr script");
			std::invoke(TCallback, script, std::forward<TArgs>(args)...);
		}
	}

private:
	[[nodiscard]] static std::vector<Entity> GetEntities(Scene& scene);
};

template <typename T>
using Script = impl::Script<T, impl::IScript>;

namespace impl {

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

} // namespace impl

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
T& AddScript(Entity& entity, TArgs&&... args) {
	auto& scripts{ entity.TryAdd<Scripts>() };

	auto& script{ scripts.AddScript<T>(std::forward<TArgs>(args)...) };

	script.entity = entity;
	script.OnCreate();

	return script;
}

template <auto TCallback, typename... TArgs>
void InvokeScript(const Entity& entity, TArgs&&... args) {
	Scripts::Invoke<TCallback>(entity, std::forward<TArgs>(args)...);
}

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
T& AddTimerScript(Entity& entity, milliseconds execution_duration, TArgs&&... args) {
	auto& script{ AddScript<T>(entity, std::forward<TArgs>(args)...) };

	PTGN_ASSERT(
		execution_duration >= milliseconds{ 0 }, "Timer script must have a positive duration"
	);

	auto& timer_scripts{ entity.TryAdd<impl::ScriptTimers>() };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	timer_scripts.timers.emplace(hash, ScriptTimerInfo{ Timer{ true }, execution_duration });

	script.OnTimerStart();

	script.OnTimerUpdate(0.0f);

	return script;
}

// TODO: Get rid of these in favor of a better timer system.

template <typename T>
[[nodiscard]] const ScriptTimerInfo& GetTimerScriptInfo(const Entity& entity) {
	const auto& scripts{ impl::GetScriptInfo<T, impl::ScriptTimers>(entity) };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	auto it{ scripts.timers.find(hash) };

	PTGN_ASSERT(
		it != scripts.timers.end(), "Entity script ", class_name, " does not have timer info"
	);

	return it->second;
}

template <typename T>
[[nodiscard]] ScriptTimerInfo& GetTimerScriptInfo(Entity& entity) {
	return const_cast<ScriptTimerInfo&>(GetTimerScriptInfo<T>(std::as_const(entity)));
}

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
	Entity& entity, milliseconds execution_delay, int execution_count, bool execute_immediately,
	TArgs&&... args
) {
	auto& script{ AddScript<T>(entity, std::forward<TArgs>(args)...) };

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

	auto& repeat_scripts{ entity.TryAdd<impl::ScriptRepeats>() };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	repeat_scripts.repeats.emplace(
		hash,
		ScriptRepeatInfo{ Timer{ true }, execution_delay, current_executions, execution_count }
	);

	return script;
}

// TODO: Get rid of these in favor of a better repeat system.

template <typename T>
[[nodiscard]] const ScriptRepeatInfo& GetRepeatScriptInfo(const Entity& entity) {
	const auto& scripts{ impl::GetScriptInfo<T, impl::ScriptRepeats>(entity) };

	constexpr auto class_name{ type_name<T>() };
	constexpr auto hash{ Hash(class_name) };

	auto it{ scripts.repeats.find(hash) };

	PTGN_ASSERT(
		it != scripts.repeats.end(), "Entity script ", class_name, " does not have repeat info"
	);

	return it->second;
}

template <typename T>
[[nodiscard]] ScriptRepeatInfo& GetRepeatScriptInfo(Entity& entity) {
	return const_cast<ScriptRepeatInfo&>(GetRepeatScriptInfo<T>(std::as_const(entity)));
}

/**
 * @brief Checks whether a script of the specified type is attached to the entity.
 *
 * @tparam T The script type to check.
 * @return True if the script exists, false otherwise.
 */
template <typename T>
[[nodiscard]] bool HasScript(const Entity& entity) {
	return entity.Has<Scripts>() && entity.Get<Scripts>().HasScript<T>();
}

/**
 * @brief Retrieves a const reference to the script of the specified type.
 *
 * @tparam T The script type to retrieve.
 * @return A const reference to the base script.
 * @throws Assert if the script is not found.
 */
template <typename T>
[[nodiscard]] const T& GetScript(const Entity& entity) {
	PTGN_ASSERT(entity.Has<Scripts>());
	auto& scripts{ entity.Get<Scripts>() };
	return scripts.GetScript<T>();
}

/**
 * @brief Retrieves a mutable reference to the script of the specified type.
 *
 * @tparam T The script type to retrieve.
 * @return A reference to the base script.
 * @throws Assert if the script is not found.
 */
template <typename T>
[[nodiscard]] T& GetScript(Entity& entity) {
	return const_cast<T&>(GetScript<T>(std::as_const(entity)));
}

/**
 * @brief Removes the script of the specified type from the entity.
 *
 * @tparam T The script type to remove.
 */
template <typename T>
void RemoveScript(Entity& entity) {
	if (!entity.Has<Scripts>()) {
		return;
	}

	auto& scripts{ entity.Get<Scripts>() };

	if (!scripts.HasScript<T>()) {
		return;
	}

	auto& script{ scripts.GetScript<T>() };

	if (entity.Has<impl::ScriptTimers>()) {
		// TODO: Consider replacing with a different callback / hook for early timer stop?
		script.OnTimerStop();

		auto& timers{ entity.Get<impl::ScriptTimers>().timers };

		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };

		timers.erase(hash);

		if (timers.empty()) {
			entity.Remove<impl::ScriptTimers>();
		}
	}

	if (entity.Has<impl::ScriptRepeats>()) {
		// TODO: Consider replacing with a different callback / hook for early repeat stop?
		script.OnRepeatStop();

		auto& repeats{ entity.Get<impl::ScriptRepeats>().repeats };

		constexpr auto class_name{ type_name<T>() };
		constexpr auto hash{ Hash(class_name) };

		repeats.erase(hash);

		if (repeats.empty()) {
			entity.Remove<impl::ScriptRepeats>();
		}
	}

	script.OnDestroy();

	scripts.RemoveScript<T>();

	if (scripts.IsEmpty()) {
		entity.Remove<Scripts>();
	}
}

} // namespace ptgn