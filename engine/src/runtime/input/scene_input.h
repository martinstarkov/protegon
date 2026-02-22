#pragma once

#include <memory>
#include <unordered_set>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "renderer/camera/viewport.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/interactive.h"

namespace ptgn {

class Scene;
class Button;
class ApplicationContext;

namespace impl {

struct MouseInfo {
	explicit MouseInfo(const Scene& scene);

	V2_float position;
	V2_int scroll_delta;

	bool left_held{ false };
	bool left_pressed{ false };
	bool left_released{ false };
};

struct DragState {
	V2_int drag_start_position;
};

} // namespace impl

struct InteractiveDebugDrawSettings {
	bool enabled{ true };
	Color color{ color::Purple };
	float line_width{ 1.0f };

	PTGN_SERIALIZER_REGISTER(InteractiveDebugDrawSettings, enabled, color, line_width)
};

class SceneInput {
public:
	/// @return True if any draggable entity is being dragged.
	[[nodiscard]] bool IsAnyDragging() const;

	/// @param True if input is in top only mode (only top interactable reacts to events), false
	/// otherwise.
	[[nodiscard]] bool IsTopOnly() const;

	/// @brief If true, only the top interactables in the scene will be triggered, i.e. if there are
	/// two buttons on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only = true);

	void SetInteractiveDebugDraw(const InteractiveDebugDrawSettings& settings = {});

	/// @return Mouse position relative to the specified viewport.
	[[nodiscard]] V2_float GetMousePosition(
		ViewportType relative_to_viewport = ViewportType::World, bool clamp_to_viewport = true
	) const;

	/// @return Mouse position relative to the specified viewport during the previous frame.
	[[nodiscard]] V2_float GetPreviousMousePosition(
		ViewportType relative_to_viewport = ViewportType::World, bool clamp_to_viewport = true
	) const;

	/// @return Mouse delta (current_position - previous_position) relative to the specified
	/// viewport.
	[[nodiscard]] V2_float GetMouseDelta(
		ViewportType relative_to_viewport = ViewportType::World, bool clamp_to_viewport = true
	) const;

	/// @return The amount scrolled by the mouse vertically in the current frame,
	/// positive upward, negative downward. Zero if no scroll occurred.
	[[nodiscard]] float GetMouseScroll() const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse is pressed.
	[[nodiscard]] bool MousePressed(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True the first frame that the mouse button is released.
	[[nodiscard]] bool MouseReleased(Mouse button) const;

	/// @param button The mouse button to check.
	/// @return True every frame that the mouse button is pressed.
	[[nodiscard]] bool MouseHeld(Mouse button) const;

	/// @param button The mouse button to check.
	/// @param time The duration of time for which the mouse should be held.
	/// @return True if the mouse button has been held for the given amount of time.
	[[nodiscard]] bool MouseHeld(Mouse button, milliseconds time) const;

	/// @param button The mouse button to check.
	/// @return The amount of time that the mouse button has been held down, negative numbers
	/// indicate the time since the mouse button was last held.
	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse button) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is pressed.
	[[nodiscard]] bool KeyPressed(Key key) const;

	/// @param key The key to check.
	/// @return True the first frame that the key is released.
	[[nodiscard]] bool KeyReleased(Key key) const;

	/// @param key The key to check.
	/// @return True every frame that the key is pressed.
	[[nodiscard]] bool KeyHeld(Key key) const;

	/// @param key The key to check.
	/// @param time The duration of time for which the key should be held.
	/// @return True if the key has been held for the given amount of time.
	[[nodiscard]] bool KeyHeld(Key key, milliseconds time) const;

	/// @param key The key to check.
	/// @return The amount of time that the key has been held down, negative numbers
	/// indicate the time since the key was last held.
	[[nodiscard]] milliseconds GetKeyHeldTime(Key key) const;

private:
	friend class Scene;
	friend class Button;
	friend bool IsDragging(Entity entity);

	enum class DropzoneAction {
		Move,
		Drop,
		Pickup
	};

	struct InteractiveEntities {
		std::vector<Entity> under_mouse;
		std::vector<Entity> not_under_mouse;
	};

	explicit SceneInput(Scene& scene);

	void Init(const std::shared_ptr<ApplicationContext>& ctx);

	/// Convert position from being relative to the center of the window to being relative to
	/// the center of the specified viewport.
	[[nodiscard]] V2_float GetMousePositionRelativeTo(
		V2_float position, ViewportType relative_to_viewport, bool clamp_to_viewport
	) const;

	template <DropzoneAction action, typename T>
	TriggerCondition GetTriggerCondition(const T& component) {
		if constexpr (action == DropzoneAction::Move) {
			return component.move_trigger_;
		} else if constexpr (action == DropzoneAction::Pickup) {
			return component.pickup_trigger_;
		} else if constexpr (action == DropzoneAction::Drop) {
			return component.drop_trigger_;
		} else {
			return TriggerCondition::None;
		}
	}

	template <
		SceneInput::DropzoneAction action, typename DropzoneFunc, typename DraggableFunc,
		typename OverlapFunc>
	void AddDropzoneActions(
		Entity& dragging, Entity& dropzone, const V2_float& mouse_position,
		DropzoneFunc&& dropzone_func, DraggableFunc&& draggable_func, OverlapFunc&& overlap_func
	) {
		// This function basically determines whether or not the the callback condition of the
		// entity is met (since they can be different), and if so it calls the respective provided
		// function.

		auto draggable_trigger{ dragging.Has<Draggable>()
									? GetTriggerCondition<action>(dragging.Get<Draggable>())
									: TriggerCondition::None };

		auto dropzone_trigger{ GetTriggerCondition<action>(dropzone.Get<Dropzone>()) };

		if (draggable_trigger == dropzone_trigger) {
			if (IsOverlappingDropzone(mouse_position, dragging, dropzone, draggable_trigger)) {
				overlap_func();
				dropzone_func();
				draggable_func();
			}
		} else {
			// Only condition overlap func once.
			bool overlap{ false };
			if (IsOverlappingDropzone(mouse_position, dragging, dropzone, dropzone_trigger)) {
				overlap_func();
				overlap = true;
				dropzone_func();
			}
			if (IsOverlappingDropzone(mouse_position, dragging, dropzone, draggable_trigger)) {
				if (!overlap) {
					overlap_func();
				}
				draggable_func();
			}
		}
	}

	static void CleanupDropzones(const std::vector<Entity>& dropzones);

	bool IsOverlappingDropzone(
		const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
		TriggerCondition condition
	);

	void Update();

	InteractiveEntities GetInteractiveEntities(const impl::MouseInfo& mouse_state) const;

	std::vector<Entity> GetDropzones();

	void DispatchMouseEvents(
		const std::vector<Entity>& over, const std::vector<Entity>& out,
		const impl::MouseInfo& mouse
	) const;

	void UpdateMouseOverStates(const std::vector<Entity>& current) const;

	void HandleDragging(
		const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
		const impl::MouseInfo& mouse
	);

	void HandleDropzones(const std::vector<Entity>& dropzones, const impl::MouseInfo& mouse);

	Scene& scene_;
	std::shared_ptr<ApplicationContext> ctx_;

	/// @brief A set of entities currently being dragged.
	std::unordered_set<Entity> dragging_entities_;

	/// @brief Stores the set of entities that were under the mouse cursor in the previous frame.
	std::unordered_set<Entity> last_mouse_over_;

	/// @brief Stores the set of entities that were dropzones in the previous frame or update cycle.
	std::unordered_set<Entity> last_dropzones_;

	/// @brief Indicates whether only the top interactable entity should be processed or considered.
	bool top_only_{ false };

	InteractiveDebugDrawSettings interactive_debug_draw_settings_;

public:
	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", interactive_debug_draw_settings_)
	)
};

} // namespace ptgn