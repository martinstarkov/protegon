#pragma once

#include <utility>

#include "core/log.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Defines the conditions under which a drag event is triggered for a draggable or dropzone
/// entity.
enum class TriggerCondition {
	/// @brief Event is never triggered.
	None,
	/// @brief Event triggered if the mouse position overlaps the dropzone.
	MouseOverlaps,
	/// @brief Event triggered if the object's transform overlaps the dropzone.
	TransformOverlaps,
	/// @brief Event triggered if any part of the object overlaps the dropzone.
	Overlaps,
	/// @brief Event triggered if the object is entirely contained within the dropzone.
	Contains
};
PTGN_REFLECT_ENUM(TriggerCondition);

/// @brief Represents the different phases of a drag event, which can be used to specify when
/// certain conditions or callbacks should be evaluated for draggable and dropzone entities.
enum class DragEventPhase {
	MoveOver,
	Drop,
	Pickup
};
PTGN_REFLECT_ENUM(DragEventPhase);

namespace impl {

template <typename TComponent>
void SetTriggerCondition(Entity entity, DragEventPhase phase, TriggerCondition condition) {
	if (!entity.Has<TComponent>()) {
		PTGN_WARN("Cannot set trigger condition of entity without ", type_name_without_namespaces<TComponent>());
		return;
	}
	auto& component{ entity.Get<TComponent>() };
	switch (phase) {
		using enum ptgn::DragEventPhase;
		case MoveOver: component.move_condition = condition; break;
		case Drop:	   component.drop_condition = condition; break;
		case Pickup:   component.pickup_condition = condition; break;
		default:	   PTGN_ERROR("Unknown DragEventPhase: ", std::to_underlying(condition));
	}
}

} // namespace impl

} // namespace ptgn