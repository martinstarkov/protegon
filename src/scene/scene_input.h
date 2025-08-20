#pragma once

#include <unordered_set>
#include <vector>

#include "components/interactive.h"
#include "core/entity.h"
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
	[[nodiscard]] V2_float GetMouseDifference() const;

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

	constexpr static Color draw_interactive_color_{ color::Magenta };
	constexpr static float draw_interactive_line_width_{ 1.0f };

public:
	PTGN_SERIALIZER_REGISTER_NAMED(
		SceneInput, KeyValue("scene_key", scene_key_), KeyValue("top_only", top_only_),
		KeyValue("draw_interactives", draw_interactives_)
	)
};

} // namespace ptgn