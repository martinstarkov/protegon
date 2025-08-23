#pragma once

#include <unordered_set>
#include <vector>

#include "components/interactive.h"
#include "core/entity.h"
#include "core/time.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Camera;
class Button;

struct MouseInfo {
	explicit MouseInfo(const Scene& scene);

	V2_int position;
	V2_int scroll_delta;

	bool left_pressed{ false };
	bool left_down{ false };
	bool left_up{ false };
};

struct DragState {
	V2_int drag_start_position;
};

class SceneInput {
public:
	[[nodiscard]] bool IsDragging(const Entity& e) const;

	[[nodiscard]] bool IsAnyDragging() const;

	// If set to true, only the interactables in the scene will be triggered, i.e. if there are two
	// button on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only);

	void SetDrawInteractives(bool draw_interactives);
	void SetDrawInteractivesColor(const Color& color);
	void SetDrawInteractivesLineWidth(float line_width);

	// Convert a point from screen space to scene primary camera (world) space.
	[[nodiscard]] V2_float ScreenToWorld(const V2_float& screen_point) const;

	// @return Mouse position relative to the scene primary camera, clamped to the range [0,
	// window_size].
	[[nodiscard]] V2_float GetMousePosition() const;

	// @return Mouse position relative to the scene primary camera, without clamping to the range
	// [0, window_size].
	[[nodiscard]] V2_float GetMousePositionUnclamped() const;

	// @return Mouse position during the previous frame relative to the scene primary camera.
	[[nodiscard]] V2_float GetMousePositionPrevious() const;

	// @return Mouse position difference between the current and previous frames relative to the
	// scene primary camera.
	[[nodiscard]] V2_float GetMousePositionDifference() const;

	// @param mouse_button The mouse button to check.
	// @return The amount of time that the mouse button has been held down, 0 if it is not currently
	// pressed.
	[[nodiscard]] milliseconds GetMouseHeldTime(Mouse mouse_button) const;

	// @param key The key to check.
	// @return The amount of time that the key has been held down, 0 if it is not currently pressed.
	[[nodiscard]] milliseconds GetKeyHeldTime(Key key) const;

	/*
	 * @param mouse_button The mouse button to check.
	 * @param time The duration of time for which the mouse should be held.
	 * @return True if the mouse button has been held for the given amount of time.
	 */
	[[nodiscard]] bool MouseHeld(Mouse mouse_button, milliseconds time = milliseconds{ 50 }) const;

	/*
	 * @param key The key to check.
	 * @param time The duration of time for which the key should be held.
	 * @return True if the key has been held for the given amount of time.
	 */
	[[nodiscard]] bool KeyHeld(Key key, milliseconds time = milliseconds{ 50 }) const;

	// @return True if mouse position is within window bounds, false otherwise.
	[[nodiscard]] bool MouseWithinWindow() const;

	// While the mouse is in relative mode, the cursor is hidden, the mouse position is constrained
	// to the window, and there will be continuous relative mouse motion events triggered even if
	// the mouse is at the edge of the window.
	// @param Whether or not mouse relative mode should be turned on or not.
	void SetRelativeMouseMode(bool on) const;

	// @param relative_to_viewport If true, returns the position translated and scaled such that
	// (0,0) is in the top left of the viewport, instead of the window.
	// @return Mouse position relative to the top left of the window, clamped to the range [0,
	// window_size].
	[[nodiscard]] V2_float GetMouseWindowPosition(bool relative_to_viewport = true) const;

	// @return Mouse position relative to the top left of the window, without clamping to the range
	// [0, window_size].
	[[nodiscard]] V2_float GetMouseWindowPositionUnclamped() const;

	// @param relative_to_viewport If true, returns the position translated and scaled such that
	// (0,0) is in the top left of the viewport, instead of the window.
	// @return Mouse position during the previous frame relative to the top left of the window.
	[[nodiscard]] V2_float GetMouseWindowPositionPrevious(bool relative_to_viewport = true) const;

	// @param relative_to_viewport If true, returns the position translated and scaled such that
	// (0,0) is in the top left of the viewport, instead of the window.
	// @return Mouse position difference between the current and previous frames relative to the top
	// left of the window.
	[[nodiscard]] V2_float GetMouseWindowPositionDifference(bool relative_to_viewport = true) const;

	// @return In desktop mode: mouse position relative to the screen (display). In browser: same as
	// GetMousePosition().
	[[nodiscard]] V2_float GetMouseScreenPosition() const;

	// @return The amount scrolled by the mouse vertically in the current frame,
	// positive upward, negative downward. Zero if no scroll occurred.
	[[nodiscard]] int GetMouseScroll() const;

	// @param mouse_button The mouse button to check.
	// @return True if the mouse button is pressed (true every frame that the button is down).
	[[nodiscard]] bool MousePressed(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True if the mouse button is released (true every frame that the button is up).
	[[nodiscard]] bool MouseReleased(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True the first frame that the mouse button is pressed (false every frame after that).
	[[nodiscard]] bool MouseDown(Mouse mouse_button) const;

	// @param mouse_button The mouse button to check.
	// @return True the first frame that the mouse button is released (false every frame after
	// that).
	[[nodiscard]] bool MouseUp(Mouse mouse_button) const;

	// @param key The key to check.
	// @return True if the key is pressed (true every frame that the key is down).
	[[nodiscard]] bool KeyPressed(Key key) const;

	// @param key The key to check.
	// @return True if the key is released (true every frame that the key is up).
	[[nodiscard]] bool KeyReleased(Key key) const;

	// @param key The key to check.
	// @return True the first frame that the key is pressed (false every frame after that).
	[[nodiscard]] bool KeyDown(Key key) const;

	// @param key The key to check.
	// @return True the first frame that the key is released (false every frame after that).
	[[nodiscard]] bool KeyUp(Key key) const;

private:
	friend class Scene;
	friend class Button;

	enum class DropzoneAction {
		Move,
		Drop,
		Pickup
	};

	template <DropzoneAction action, typename T>
	CallbackTrigger GetCallbackTrigger(const T& component) {
		if constexpr (action == DropzoneAction::Move) {
			return component.move_trigger_;
		} else if constexpr (action == DropzoneAction::Pickup) {
			return component.pickup_trigger_;
		} else if constexpr (action == DropzoneAction::Drop) {
			return component.drop_trigger_;
		} else {
			return CallbackTrigger::None;
		}
	}

	template <
		SceneInput::DropzoneAction action, typename DropzoneFunc, typename DraggableFunc,
		typename OverlapFunc>
	void AddDropzoneActions(
		Entity& dragging, Entity& dropzone, const V2_float& mouse_position,
		DropzoneFunc&& dropzone_func, DraggableFunc&& draggable_func, OverlapFunc&& overlap_func
	) {
		// This function basically determines whether or not the the callback trigger of the entity
		// is met (since they can be different), and if so it calls the respective provided
		// function.

		auto draggable_trigger{ dragging.Has<Draggable>()
									? GetCallbackTrigger<action>(dragging.Get<Draggable>())
									: CallbackTrigger::None };

		auto dropzone_trigger{ GetCallbackTrigger<action>(dropzone.Get<Dropzone>()) };

		if (draggable_trigger == dropzone_trigger) {
			if (IsOverlappingDropzone(mouse_position, dragging, dropzone, draggable_trigger)) {
				overlap_func();
				dropzone_func();
				draggable_func();
			}
		} else {
			// Only trigger overlap func once.
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

	static bool IsOverlappingDropzone(
		const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
		CallbackTrigger trigger
	);

	void Update(Scene& scene);

	void Init(std::size_t scene_key);
	void Shutdown();

	struct InteractiveEntities {
		std::vector<Entity> under_mouse;
		std::vector<Entity> not_under_mouse;
	};

	InteractiveEntities GetInteractiveEntities(Scene& scene, const MouseInfo& mouse_state) const;
	static std::vector<Entity> GetDropzones(Scene& scene);
	void DispatchMouseEvents(
		const std::vector<Entity>& over, const std::vector<Entity>& out, const MouseInfo& mouse
	) const;
	void UpdateMouseOverStates(const std::vector<Entity>& current) const;
	void HandleDragging(
		const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
		const MouseInfo& mouse
	);
	void HandleDropzones(const std::vector<Entity>& dropzones, const MouseInfo& mouse);

	// TODO: Add to serialization.

	std::unordered_set<Entity> dragging_entities_;
	std::unordered_set<Entity> last_mouse_over_;
	std::unordered_set<Entity> last_dropzones_;

	std::size_t scene_key_{ 0 };

	bool top_only_{ false };

	bool draw_interactives_{ false };

	Color draw_interactive_color_{ color::Magenta };
	float draw_interactive_line_width_{ 1.0f };

public:
	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("scene_key", scene_key_), KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", draw_interactives_)
	)
};

} // namespace ptgn