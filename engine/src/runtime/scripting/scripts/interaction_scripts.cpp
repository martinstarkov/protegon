#include "runtime/scripting/scripts/interaction_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void DraggableSetFollowMouseScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::Draggable>()) {
		SetDraggableFollowMouse(target, option);
	}
}

void DraggableSetMoveConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Draggable>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDraggableCondition(target, DragEventPhase::MoveOver, *condition);
	}
}

void DraggableSetDropConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Draggable>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDraggableCondition(target, DragEventPhase::Drop, *condition);
	}
}

void DraggableSetPickupConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Draggable>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDraggableCondition(target, DragEventPhase::Pickup, *condition);
	}
}

void DropzoneSetMoveConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Dropzone>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDropzoneCondition(target, DragEventPhase::MoveOver, *condition);
	}
}

void DropzoneSetDropConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Dropzone>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDropzoneCondition(target, DragEventPhase::Drop, *condition);
	}
}

void DropzoneSetPickupConditionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::Dropzone>()) {
		return;
	}
	if (auto condition{ magic_enum::enum_cast<TriggerCondition>(text) }) {
		SetDropzoneCondition(target, DragEventPhase::Pickup, *condition);
	}
}

void InteractionAddInteractiveShapeScript::OnStart() {
	if (!target) {
		return;
	}
	if (index == 0) {
		AddInteractiveRect(target, Transform{ vector }, vector2);
	} else if (index == 1) {
		AddInteractiveCircle(target, Transform{ vector }, number);
	}
}

void InteractionRemoveInteractiveShapeScript::OnStart() {
	if (!target) {
		return;
	}
	RemoveInteractiveShape(target, text);
}

void InteractionClearInteractiveShapesScript::OnStart() {
	if (!target) {
		return;
	}
	ClearInteractiveShapes(target);
}

void InteractionLockInteractionsScript::OnStart() {
	if (!target) {
		return;
	}
	target.Add<InteractionLock>(InteractionLock{ option, true, secondsf{ number } });
}

void InteractionUnlockInteractionsScript::OnStart() {
	if (!target) {
		return;
	}
	target.Remove<InteractionLock>();
}

} // namespace ptgn
