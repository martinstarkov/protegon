#pragma once

#include <memory>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/scene/resolution.h"
#include "runtime/ui/interactive.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Scene;
class ApplicationContext;

struct MouseEnter : public Event<MouseEnter> {};

struct MouseLeave : public Event<MouseLeave> {};

struct MouseMoveOver : public Event<MouseMoveOver> {};

struct MousePressedOver : public Event<MousePressedOver> {
	Mouse button;
};

struct MouseHeldOver : public Event<MouseHeldOver> {
	Mouse button;
};

struct MouseReleasedOver : public Event<MouseReleasedOver> {
	Mouse button;
};

struct MouseScrollOver : public Event<MouseScrollOver> {
	V2_float scroll_delta;
};

struct MouseMoveOut : public Event<MouseMoveOut> {};

struct MousePressedOut : public Event<MousePressedOut> {
	Mouse button;
};

struct MouseHeldOut : public Event<MouseHeldOut> {
	Mouse button;
};

struct MouseReleasedOut : public Event<MouseReleasedOut> {
	Mouse button;
};

struct MouseScrollOut : public Event<MouseScrollOut> {
	V2_float scroll_delta;
};

struct DragStart : public Event<DragStart> {
	/// @brief Position of the mouse in world coordinates at the start of the drag.
	V2_float start_position;
};

struct DragStop : public Event<DragStop> {
	/// @brief Position of the mouse in world coordinates at the end of the drag.
	V2_float stop_position;
};

struct PickupFromDropzone : public Event<PickupFromDropzone> {
	/// @brief The draggable that was picked up from the dropzone.
	Entity draggable;
};

struct PickupDraggable : public Event<PickupDraggable> {
	/// @brief The dropzone that the draggable was picked up from.
	Entity dropzone;
};

struct Dragging : public Event<Dragging> {
	/// @brief Current position of the mouse in world coordinates relative to the camera the entity
	/// is being dragged in.
	V2_float position;

	/// @brief Current offset of the mouse position relative to where it started in world
	/// coordinates.
	V2_float offset;
};

struct DropDraggable : public Event<DropDraggable> {
	/// @brief The dropzone that the draggable was dropped into.
	Entity dropzone;
};

struct DropIntoDropzone : public Event<DropIntoDropzone> {
	/// @brief The draggable that was dropped into the dropzone.
	Entity draggable;
};

struct EnterDropzone : public Event<EnterDropzone> {
	/// @brief The draggable that entered the dropzone.
	Entity draggable;
};

struct LeaveDropzone : public Event<LeaveDropzone> {
	/// @brief The draggable that left the dropzone.
	Entity draggable;
};

struct MoveOverDropzone : public Event<MoveOverDropzone> {
	/// @brief The draggable that is over the dropzone.
	Entity draggable;
};

struct MoveOutsideDropzone : public Event<MoveOutsideDropzone> {
	/// @brief The draggable that is outside the dropzone.
	Entity draggable;
};

struct DragEnter : public Event<DragEnter> {
	/// @brief The dropzone that the draggable entered.
	Entity dropzone;
};

struct DragLeave : public Event<DragLeave> {
	/// @brief The dropzone that the draggable left.
	Entity last_dropzone;
};

struct DragOver : public Event<DragOver> {
	/// @brief The dropzone that the draggable was dragged over.
	Entity dropzone;
};

struct DragOut : public Event<DragOut> {
	/// @brief The dropzone that the draggable was dragged outside of.
	Entity dropzone;
};

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

struct SceneInputSettings {
	bool debug_draw_enabled{ false };
	Color debug_draw_color{ color::Magenta };
	float debug_draw_line_width{ 1.0f };

	PTGN_SERIALIZER_REGISTER(
		SceneInputSettings, debug_draw_enabled, debug_draw_color, debug_draw_line_width
	)
};

class SceneInput {
public:
	/// @return True if any draggable entity is being dragged.
	[[nodiscard]] bool IsAnyDragging(Camera camera) const;

	/// @param True if input is in top only mode (only top interactable reacts to events), false
	/// otherwise.
	[[nodiscard]] bool IsTopOnly() const;

	/// @brief If true, only the top interactables in the scene will be triggered, i.e. if there are
	/// two buttons on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only = true);

	void SetSettings(const SceneInputSettings& settings = {});

	/// @return Mouse position relative to the specified viewport.
	[[nodiscard]] V2_float GetMousePosition(
		Frame position_frame_of_reference = Frame::World, bool clamp_to_viewport = true
	) const;

	/// @return Mouse position relative to the specified viewport during the previous frame.
	[[nodiscard]] V2_float GetPreviousMousePosition(
		Frame position_frame_of_reference = Frame::World, bool clamp_to_viewport = true
	) const;

	/// @return Mouse delta (current_position - previous_position) relative to the specified
	/// viewport.
	[[nodiscard]] V2_float GetMouseDelta(
		Frame delta_frame_of_reference = Frame::World, bool clamp_to_viewport = true
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
	friend bool IsDragging(Entity entity);

	enum class DropzoneAction {
		Move,
		Drop,
		Pickup
	};

	struct InteractiveEntities {
		std::vector<Entity> under_mouse;
		std::vector<Entity> not_under_mouse;

		friend std::ostream& operator<<(std::ostream& o, const InteractiveEntities& entities) {
			o << "Under mouse: ";
			o << entities.under_mouse.size();
			o << ", Not under mouse: ";
			o << entities.not_under_mouse.size();
			return o;
		}
	};

	explicit SceneInput(Scene& scene);

	void Init(const std::shared_ptr<ApplicationContext>& ctx);

	/// Convert position from being relative to the center of the window to being relative to
	/// the center of the specified viewport.
	[[nodiscard]] V2_float GetMousePositionRelativeTo(
		V2_float position, Frame frame_of_reference, bool clamp_to_viewport
	) const;

	[[nodiscard]] static bool Overlap(V2_float point, Entity entity);
	[[nodiscard]] static bool Overlap(Entity entityA, Entity entityB);

	[[nodiscard]] static Transform GetWorldOffsetTransform(
		const Shape& shape, Entity shape_entity, Entity parent
	);

	template <DropzoneAction action, typename T>
	static TriggerCondition GetTriggerCondition(const T& component) {
		if constexpr (action == DropzoneAction::Move) {
			return component.move_condition;
		} else if constexpr (action == DropzoneAction::Pickup) {
			return component.pickup_condition;
		} else if constexpr (action == DropzoneAction::Drop) {
			return component.drop_condition;
		} else {
			return TriggerCondition::None;
		}
	}

	template <
		SceneInput::DropzoneAction action, typename DropzoneFunc, typename DraggableFunc,
		typename OverlapFunc>
	static void AddDropzoneActions(
		Entity& dragging, Entity& dropzone, const V2_float& mouse_position,
		DropzoneFunc&& dropzone_func, DraggableFunc&& draggable_func, OverlapFunc&& overlap_func
	) {
		// This function basically determines whether or not the the callback condition of the
		// entity is met (since they can be different), and if so it calls the respective provided
		// function.

		auto draggable_trigger{ dragging.Has<impl::Draggable>()
									? GetTriggerCondition<action>(dragging.Get<impl::Draggable>())
									: TriggerCondition::None };

		auto dropzone_trigger{ GetTriggerCondition<action>(dropzone.Get<impl::Dropzone>()) };

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

	static bool IsOverlappingDropzone(
		const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
		TriggerCondition condition
	);

	void Update();

	InteractiveEntities GetInteractiveEntities(
		const impl::MouseInfo& mouse_state, const std::vector<Entity>& all_entities, Camera camera
	) const;

	std::vector<Entity> GetDropzones();

	static void DispatchMouseEvents(
		const std::vector<Entity>& over, const std::vector<Entity>& out,
		const impl::MouseInfo& mouse
	);

	static void UpdateMouseOverStates(
		const std::vector<Entity>& current, const std::unordered_set<Entity>& last_mouse_over
	);

	static void HandleDragging(
		const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
		const impl::MouseInfo& mouse, std::unordered_set<Entity>& dragging_entities
	);

	static void HandleDropzones(
		const std::vector<Entity>& dropzones, const impl::MouseInfo& mouse,
		const std::unordered_set<Entity>& dragging_entities
	);

	Scene& scene_;
	std::shared_ptr<ApplicationContext> ctx_;

	/// @brief A set of entities currently being dragged per a given camera.
	std::unordered_map<Camera, std::unordered_set<Entity>> dragging_entities_;

	/// @brief Stores the set of entities that were under the mouse cursor in the previous frame per
	/// a given camera.
	std::unordered_map<Camera, std::unordered_set<Entity>> last_mouse_over_;

	/// @brief Indicates whether only the top interactable entity should be processed or considered.
	bool top_only_{ false };

	SceneInputSettings settings_;
};

} // namespace ptgn