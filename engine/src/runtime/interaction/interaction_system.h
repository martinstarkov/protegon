#pragma once

#include <functional>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "runtime/ecs/entity.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn {

class Scene;
class SceneContext;
class RenderTarget;

namespace impl {

struct MouseInfo {
	explicit MouseInfo(const Scene& scene);

	V2_float position{};
	V2_float scroll_delta{};

	bool left_held{ false };
	bool left_pressed{ false };
	bool left_released{ false };
};

struct InteractedEntities {
	std::vector<Entity> entities{};
};

void GetShapes(
	Entity entity, Entity root_entity, std::vector<std::pair<InteractiveShape, Entity>>& vector
);

} // namespace impl

class InteractionSystem {
public:
	/// @param True if input is in top only mode (only top interactable reacts to events), false
	/// otherwise.
	[[nodiscard]] bool IsTopOnly() const;

	/// @brief If true, only the top interactables in the scene will be triggered, i.e. if there are
	/// two buttons on top of each other, only the top one will be able to be hovered or pressed.
	void SetTopOnly(bool top_only = true);

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
		std::vector<Entity> under_mouse{};
		std::vector<Entity> not_under_mouse{};

		friend std::ostream& operator<<(std::ostream& os, const InteractiveEntities& entities) {
			os << "{ under_mouse: " << entities.under_mouse.size();
			os << ", not_under_mouse: " << entities.not_under_mouse.size() << " }";
			return os;
		}
	};

	void UpdateForCamera(
		Scene& scene, const impl::MouseInfo& mouse_state, bool& handled_under_mouse,
		const RenderTarget& render_target, const Camera& camera,
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
	/// entity is met (since conditions can be different), and if so it calls the respective
	/// provided function.
	template <DropzoneAction action, typename FDropzone, typename FDraggable, typename FOverlap>
	static void AddDropzoneActions(
		Entity& dragging, Entity& dropzone, const V2_float& mouse_position, FDropzone&& dropzone_fn,
		FDraggable&& draggable_fn, FOverlap&& overlap_fn
	) {
		auto draggable_trigger{ dragging.Has<impl::Draggable>()
									? GetTriggerCondition<action>(dragging.Get<impl::Draggable>())
									: TriggerCondition::None };

		auto dropzone_trigger{ GetTriggerCondition<action>(dropzone.Get<impl::Dropzone>()) };

		if (draggable_trigger == dropzone_trigger) {
			if (IsOverlappingDropzone(mouse_position, dragging, dropzone, draggable_trigger)) {
				std::invoke(std::forward<FOverlap>(overlap_fn));
				std::invoke(std::forward<FDropzone>(dropzone_fn));
				std::invoke(std::forward<FDraggable>(draggable_fn));
			}
			return;
		}

		// Only condition overlap fn once.
		bool overlap{ false };
		if (IsOverlappingDropzone(mouse_position, dragging, dropzone, dropzone_trigger)) {
			std::invoke(overlap_fn);
			overlap = true;
			std::invoke(std::forward<FDropzone>(dropzone_fn));
		}
		if (IsOverlappingDropzone(mouse_position, dragging, dropzone, draggable_trigger)) {
			if (!overlap) {
				std::invoke(std::forward<FOverlap>(overlap_fn));
			}
			std::invoke(std::forward<FDraggable>(draggable_fn));
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

	/// @brief A set of entities currently being dragged per a given camera.
	std::unordered_map<SceneCamera, impl::InteractedEntities> dragging_entities_;

	/// @brief Stores the set of entities that were under the mouse cursor in the previous frame per
	/// a given camera.
	std::unordered_map<SceneCamera, impl::InteractedEntities> last_mouse_over_;
	/// @brief Indicates whether only the top interactable entity should be processed or considered.
	bool top_only_{ false };
};

} // namespace ptgn