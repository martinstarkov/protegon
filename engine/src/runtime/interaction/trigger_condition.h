#pragma once

#include <utility>

#include "core/log.h"
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
PTGN_SERIALIZE_ENUM(TriggerCondition);

/// @brief Represents the different phases of a drag event, which can be used to specify when
/// certain conditions or callbacks should be evaluated for draggable and dropzone entities.
enum class DragEventPhase {
	MoveOver,
	Drop,
	Pickup
};
PTGN_SERIALIZE_ENUM(DragEventPhase);

/// @brief Controls the lifecycle state of a behavior/component on an entity.
enum class ComponentState {
	/// @brief Component exists but is inactive.
	Disabled,
	/// @brief Component exists and is active.
	Enabled,
	/// @brief Component is removed entirely from the entity.
	Removed
};
PTGN_SERIALIZE_ENUM(ComponentState);

namespace impl {

template <typename TComponent>
void SetTriggerCondition(Entity entity, DragEventPhase phase, TriggerCondition condition) {
	auto& component{ entity.Get<TComponent>() };
	switch (phase) {
		using enum ptgn::DragEventPhase;
		case MoveOver: component.move_condition = condition; break;
		case Drop:	   component.drop_condition = condition; break;
		case Pickup:   component.pickup_condition = condition; break;
		default:	   PTGN_ERROR("Unknown DragEventPhase: ", std::to_underlying(condition));
	}
}

template <typename TComponent>
void SetComponentState(Entity entity, ComponentState state) {
	switch (state) {
		using enum ptgn::ComponentState;
		case Disabled: entity.TryAdd<TComponent>().enabled = false; break;
		case Enabled:  entity.TryAdd<TComponent>().enabled = true; break;
		case Removed:  entity.Remove<TComponent>(); break;
		default:	   PTGN_ERROR("Unknown ComponentState: ", std::to_underlying(state));
	}
}

} // namespace impl

} // namespace ptgn