#include "runtime/scripting/script_registration_engine.h"

#include <string>
#include <string_view>
#include <utility>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "runtime/animation/animation_event.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"

namespace ptgn {

namespace {

template <typename T>
[[nodiscard]] T JsonValueOr(const json& input, std::string_view key, T fallback) {
	if (!input.is_object()) {
		return fallback;
	}
	const auto it{ input.find(std::string{ key }) };
	if (it == input.end() || it->is_null()) {
		return fallback;
	}
	try {
		return it->template get<T>();
	} catch (...) {
		return fallback;
	}
}

template <typename T>
[[nodiscard]] json MakeEventDefault(std::string_view key, T value) {
	json result = json::object();
	result[std::string{ key }] = std::move(value);
	return result;
}

template <typename TEvent>
[[nodiscard]] Mouse EventMouse(const TEvent& event) {
	if constexpr (requires { event.mouse; }) {
		return event.mouse;
	} else if constexpr (requires { event.button; }) {
		return event.button;
	} else {
		return Mouse::Left;
	}
}

template <typename TEvent>
[[nodiscard]] bool MatchKeyEvent(Entity, const json& value, const TEvent& event) {
	return event.key == JsonValueOr<Key>(value, "key", Key::W);
}

template <typename TEvent>
[[nodiscard]] bool MatchMouseEvent(Entity, const json& value, const TEvent& event) {
	return EventMouse(event) == JsonValueOr<Mouse>(value, "button", Mouse::Left);
}

[[nodiscard]] bool HasOverlapCollider(Entity entity) {
	if (!entity || !entity.Has<Collider>()) {
		return false;
	}

	const auto& collider{ entity.Get<Collider>() };

	return collider.mode == CollisionMode::Overlap;
}

[[nodiscard]] bool HasCollisionCollider(Entity entity) {
	if (!entity || !entity.Has<Collider>()) {
		return false;
	}

	const auto& collider{ entity.Get<Collider>() };

	return collider.mode == CollisionMode::Discrete || collider.mode == CollisionMode::Continuous;
}

[[nodiscard]] bool HasInteractive(Entity entity) {
	return entity && entity.Has<impl::Interactive>();
}

[[nodiscard]] bool HasDraggable(Entity entity) {
	return HasInteractive(entity) && entity && entity.Has<impl::Draggable>();
}

[[nodiscard]] bool HasDropzone(Entity entity) {
	return HasInteractive(entity) && entity && entity.Has<impl::Dropzone>();
}

[[nodiscard]] bool HasAnimationData(Entity entity) {
	return entity && entity.Has<impl::AnimationData>();
}

[[nodiscard]] bool HasButtonData(Entity entity) {
	return entity && entity.Has<impl::ButtonData>();
}

[[nodiscard]] bool HasToggleButtonData(Entity entity) {
	return entity && entity.Has<impl::ToggleButtonData>();
}

[[nodiscard]] bool HasDropdownData(Entity entity) {
	return entity && entity.Has<impl::DropdownData>();
}

} // namespace

PTGN_REGISTER_SCRIPT(Script);

PTGN_REGISTER_SCRIPT(
	WaitScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.requires_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 250.0f },
	}
);

PTGN_REGISTER_SCRIPT(
	MoveToScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 300.0f,
			.ease = Ease::OutCubic,
		},
	}
);

PTGN_REGISTER_SCRIPT(
	RotateToScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 300.0f },
	}
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 180.0f,
			.ease = Ease::OutBack,
		},
	}
);

PTGN_REGISTER_SCRIPT(
	TintToScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 300.0f },
	}
);

PTGN_REGISTER_SCRIPT(
	BounceScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.requires_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 500.0f },
	}
);

PTGN_REGISTER_SCRIPT(
	ShakeScript,
	{
		.completion = ScriptCompletion::ScriptControlled,
		.supports_timing = true,
	}
);

PTGN_REGISTER_SCRIPT(AddShakeTraumaScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(RecoverShakeScript, { .completion = ScriptCompletion::ScriptControlled });
PTGN_REGISTER_SCRIPT(ResetShakeScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(FollowTargetScript, { .completion = ScriptCompletion::ScriptControlled });
PTGN_REGISTER_SCRIPT(FollowEntityScript, { .completion = ScriptCompletion::ScriptControlled });
PTGN_REGISTER_SCRIPT(FollowPathScript, { .completion = ScriptCompletion::ScriptControlled });

PTGN_REGISTER_SCRIPT(
	NativeScript,
	{
		.completion = ScriptCompletion::ScriptControlled,
		.supports_timing = true,
		.serializable = false,
	}
);

PTGN_REGISTER_SCRIPT(SetVisibleScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(PlaySoundScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(AnimationActionScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(SetTextureScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(SetEnabledScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(SceneChangeScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(EmitSignalScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(AddComponentsScript, { .completion = ScriptCompletion::Instant });
PTGN_REGISTER_SCRIPT(RemoveComponentsScript, { .completion = ScriptCompletion::Instant });

PTGN_REGISTER_EVENT(
	event::KeyPressed,
	{
		.default_value = MakeEventDefault("key", Key::W),
		.matches = &MatchKeyEvent<event::KeyPressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyHeld,
	{
		.default_value = MakeEventDefault("key", Key::W),
		.matches = &MatchKeyEvent<event::KeyHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyReleased,
	{
		.default_value = MakeEventDefault("key", Key::W),
		.matches = &MatchKeyEvent<event::KeyReleased>,
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressed,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MousePressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeld,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MouseHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleased,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MouseReleased>,
	}
);

PTGN_REGISTER_EVENT(event::MouseMoveOver, {
	.available = &HasInteractive,
});
PTGN_REGISTER_EVENT(event::MouseMoveOut, {
	.available = &HasInteractive,
});

PTGN_REGISTER_EVENT(
	event::MousePressedOver,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MousePressedOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeldOver,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MouseHeldOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleasedOver,
	{
		.default_value = MakeEventDefault("button", Mouse::Left),
		.matches = &MatchMouseEvent<event::MouseReleasedOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonPress,
	{ .available = &HasButtonData }
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStart,
	{ .available = &HasButtonData }
);

PTGN_REGISTER_EVENT(
	event::ButtonHover,
	{ .available = &HasButtonData }
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStop,
	{ .available = &HasButtonData }
);

PTGN_REGISTER_EVENT(
	event::ToggleButtonToggle,
	{ .available = &HasToggleButtonData }
);

PTGN_REGISTER_EVENT(
	event::DropdownOpen,
	{ .available = &HasDropdownData }
);

PTGN_REGISTER_EVENT(
	event::DropdownClose,
	{ .available = &HasDropdownData }
);

PTGN_REGISTER_EVENT(
	event::DropdownToggle,
	{ .available = &HasDropdownData }
);

PTGN_REGISTER_EVENT(
	event::DropdownItemPress,
	{ .available = &HasDropdownData }
);

PTGN_REGISTER_EVENT(event::DragStart, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::Drag, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DragStop, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::PickupDraggable, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DropDraggable, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DragEnter, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DragLeave, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DragOver, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::DragOut, { .available = &HasDraggable });
PTGN_REGISTER_EVENT(event::PickupFromDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::DropIntoDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::EnterDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::LeaveDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::MoveOverDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::MoveOutsideDropzone, { .available = &HasDropzone });
PTGN_REGISTER_EVENT(event::OverlapStart, { .available = &HasOverlapCollider });
PTGN_REGISTER_EVENT(event::Overlap, { .available = &HasOverlapCollider });
PTGN_REGISTER_EVENT(event::OverlapStop, { .available = &HasOverlapCollider });
PTGN_REGISTER_EVENT(event::Collision, { .available = &HasCollisionCollider });

PTGN_REGISTER_EVENT(event::AnimationStart, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationStop, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationPause, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationResume, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationFrameChange, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationUpdate, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationFinalFrame, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationComplete, { .available = &HasAnimationData });
PTGN_REGISTER_EVENT(event::AnimationLoopComplete, { .available = &HasAnimationData });

PTGN_REGISTER_EVENT(
	Signal,
	{
		.default_value = MakeEventDefault("signal", std::string{}),
		.matches = [](Entity, const json& value, const Signal& event) {
			return std::string_view{ event.key } ==
				JsonValueOr<std::string>(value, "signal", "");
		},
	}
);

namespace impl {

void EnsureEngineScriptsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file. The namespace-scope
	// registration initializers then populate the runtime registries.
}

} // namespace impl

} // namespace ptgn
