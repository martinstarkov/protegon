#include "runtime/scripting/script_registration_engine.h"

#include <string>
#include <string_view>
#include <utility>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/ui/button.h"

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

PTGN_REGISTER_SCRIPT(
	AddShakeTraumaScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_SCRIPT(
	RecoverShakeScript,
	{ .completion = ScriptCompletion::ScriptControlled }
);

PTGN_REGISTER_SCRIPT(
	ResetShakeScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript,
	{ .completion = ScriptCompletion::ScriptControlled }
);

PTGN_REGISTER_SCRIPT(
	FollowEntityScript,
	{ .completion = ScriptCompletion::ScriptControlled }
);

PTGN_REGISTER_SCRIPT(
	FollowPathScript,
	{ .completion = ScriptCompletion::ScriptControlled }
);

PTGN_REGISTER_SCRIPT(
	NativeScript,
	{
		.completion = ScriptCompletion::ScriptControlled,
		.supports_timing = true,
		.serializable = false,
	}
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript,
	{ .completion = ScriptCompletion::Instant }
);

PTGN_REGISTER_EVENT(
	event::KeyPressed,
	{
		.default_value = json{ { "key", Key::W } },
		.matches = &MatchKeyEvent<event::KeyPressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyHeld,
	{
		.default_value = json{ { "key", Key::W } },
		.matches = &MatchKeyEvent<event::KeyHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyReleased,
	{
		.default_value = json{ { "key", Key::W } },
		.matches = &MatchKeyEvent<event::KeyReleased>,
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressed,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MousePressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeld,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MouseHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleased,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MouseReleased>,
	}
);

PTGN_REGISTER_EVENT(event::MouseMoveOver);
PTGN_REGISTER_EVENT(event::MouseMoveOut);

PTGN_REGISTER_EVENT(
	event::MousePressedOver,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MousePressedOver>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeldOver,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MouseHeldOver>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleasedOver,
	{
		.default_value = json{ { "button", Mouse::Left } },
		.matches = &MatchMouseEvent<event::MouseReleasedOver>,
	}
);

PTGN_REGISTER_EVENT(event::ButtonPress);
PTGN_REGISTER_EVENT(event::DragStart);
PTGN_REGISTER_EVENT(event::Drag);
PTGN_REGISTER_EVENT(event::DragStop);
PTGN_REGISTER_EVENT(event::OverlapStart);
PTGN_REGISTER_EVENT(event::Overlap);
PTGN_REGISTER_EVENT(event::OverlapStop);
PTGN_REGISTER_EVENT(event::Collision);

PTGN_REGISTER_EVENT(
	Signal,
	{
		.default_value = json{ { "signal", "" } },
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
