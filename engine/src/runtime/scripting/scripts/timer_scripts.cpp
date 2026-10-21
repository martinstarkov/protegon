#include "runtime/scripting/scripts/timer_scripts.h"

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

void TimerStartScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Start();
}

void TimerRestartScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Restart();
}

void TimerStopScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Stop();
}

void TimerResetScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Reset();
}

void TimerPauseScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Pause();
}

void TimerResumeScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Resume();
}

void TimerTogglePausedScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.TogglePaused();
}

void TimerAdvanceScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Advance(amount);
}

void TimerRewindScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.Rewind(amount);
}

void TimerSetDurationScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.SetDuration(amount);
}

void TimerAddDurationScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.AddDuration(amount);
}

void TimerRemoveDurationScript::OnStart() {
	TimerHandle timer_handle{ GetTimer(Target(), timer) };
	if (!timer_handle) {
		PTGN_WARN("Missing timer: ", timer.value);
		return;
	}
	timer_handle.RemoveDuration(amount);
}

void LifetimeStartScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Start();
}

void LifetimeRestartScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Start();
}

void LifetimeStopScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Stop();
}

void LifetimeResetScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Reset();
}

void LifetimePauseScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Pause();
}

void LifetimeResumeScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Resume();
}

void LifetimeTogglePausedScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	if (lifetime->IsPaused()) {
		lifetime->Resume();
	} else if (lifetime->IsRunning()) {
		lifetime->Pause();
	}
}

void LifetimeAdvanceScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Advance(target, amount);
}

void LifetimeRewindScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->Rewind(amount);
}

void LifetimeSetDurationScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->SetDuration(target, amount);
}

void LifetimeAddDurationScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->AddDuration(target, amount);
}

void LifetimeRemoveDurationScript::OnStart() {
	Entity target{ Target() };
	auto* lifetime{ target ? target.TryGet<Lifetime>() : nullptr };
	if (!lifetime) {
		PTGN_WARN("Lifetime action requires a Lifetime component");
		return;
	}
	lifetime->RemoveDuration(target, amount);
}

} // namespace ptgn
