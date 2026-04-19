#pragma once

#include <functional>
#include <ostream>
#include <unordered_map>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/scene/scene_camera.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class SceneContext;
class RenderTarget;

namespace impl {

struct MouseInfo {
	explicit MouseInfo(const Scene& scene);

	V2_float position;
	V2_float scroll_delta;

	bool left_held{ false };
	bool left_pressed{ false };
	bool left_released{ false };
};

struct DragState {
	V2_int drag_start_position;
};

struct InteractedEntities {
	std::vector<Entity> entities;
	SceneCamera camera;
};

} // namespace impl

struct InteractiveDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	float draw_line_width{ 1.0f };

	PTGN_REFLECT(InteractiveDebugSettings, draw_enabled, draw_color, draw_line_width)
};

class InteractionSystem {
public:
	/// @param True if input is in top only mode (only top interactable reacts to events), false
	/// otherwise.
	[[nodiscard]] bool IsTopOnly() const;

	/// @brief If true, only the top interactables in the scene will be triggered, i.e. if there are
	/// two buttons on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only = true);

	void SetDebugSettings(const InteractiveDebugSettings& settings = {});

	/// @return True if any draggable entity is being dragged.
	[[nodiscard]] bool IsAnyDragging(SceneCamera camera) const;

	[[nodiscard]] static bool Overlap(V2_float point, Entity interactive_entity);
	[[nodiscard]] static bool Overlap(Entity entityA, Entity entityB);

private:
	friend class Scene;
	friend class SceneContext;
	friend bool IsDragging(Entity entity);

	enum class DropzoneAction {
		Move,
		Drop,
		Pickup
	};

	struct InteractiveEntities {
		std::vector<Entity> under_mouse;
		std::vector<Entity> not_under_mouse;

		friend std::ostream& operator<<(std::ostream& os, const InteractiveEntities& entities) {
			os << "{ under_mouse: " << entities.under_mouse.size();
			os << ", not_under_mouse: " << entities.not_under_mouse.size() << " }";
			return os;
		}
	};

	void UpdateForCamera(
		Scene& scene, const impl::MouseInfo& mouse_state, bool& handled_under_mouse,
		const RenderTarget& render_target, const Camera& camera, std::size_t camera_uuid,
		const std::function<bool(Entity)>& filter, const SceneCamera& scene_camera
	);

	static Transform GetWorldOffsetTransform(const Shape& shape, Entity shape_entity);

	template <DropzoneAction action, typename T>
	static TriggerCondition GetTriggerCondition(const T& component) {
		using enum DropzoneAction;

		if constexpr (action == Move) {
			return component.move_condition;
		} else if constexpr (action == Pickup) {
			return component.pickup_condition;
		} else if constexpr (action == Drop) {
			return component.drop_condition;
		} else {
			return TriggerCondition::None;
		}
	}

	/// @brief This function basically determines whether or not the the callback condition of the
	/// entity is met (since they can be different), and if so it calls the respective provided
	/// function.
	template <
		DropzoneAction action, typename DropzoneFunc, typename DraggableFunc, typename OverlapFunc>
	static void AddDropzoneActions(
		Entity& dragging, Entity& dropzone, const V2_float& mouse_position,
		DropzoneFunc&& dropzone_func, DraggableFunc&& draggable_func, OverlapFunc&& overlap_func
	) {
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

	void Update(Scene& scene);

	InteractiveEntities GetInteractiveEntities(
		const impl::MouseInfo& mouse_state, const std::vector<Entity>& all_entities
	) const;

	std::vector<Entity> GetDropzones(const Scene& scene);

	static void DispatchMouseEvents(
		const std::vector<Entity>& over, const std::vector<Entity>& out,
		const impl::MouseInfo& mouse
	);

	static void UpdateMouseOverStates(
		const std::vector<Entity>& current, const std::vector<Entity>& last_mouse_over
	);

	static void HandleDragging(
		const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
		const impl::MouseInfo& mouse, std::vector<Entity>& dragging_entities
	);

	static void HandleDropzones(
		const std::vector<Entity>& dropzones, const impl::MouseInfo& mouse,
		const std::vector<Entity>& dragging_entities
	);

	void DrawDebug(Scene& scene) const;
	void DrawDebugForCamera(
		Scene& scene, const impl::MouseInfo& mouse_state, const impl::RenderCamera& camera,
		const std::function<bool(Entity)>& filter
	) const;

	using CameraUUID = std::size_t;

	/// @brief A set of entities currently being dragged per a given camera uuid.
	std::unordered_map<CameraUUID, impl::InteractedEntities> dragging_entities_;

	/// @brief Stores the set of entities that were under the mouse cursor in the previous frame per
	/// a given camera.
	std::unordered_map<CameraUUID, impl::InteractedEntities> last_mouse_over_;
	/// @brief Indicates whether only the top interactable entity should be processed or considered.
	bool top_only_{ false };

	InteractiveDebugSettings debug_settings_;
};

} // namespace ptgn