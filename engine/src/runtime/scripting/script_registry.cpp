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
[[nodiscard]] bool MatchAlwaysEvent(Entity, const json&, const TEvent&) {
	return true;
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

PTGN_REGISTER_SCRIPT(Script, (ScriptRegistrationOptions{}));

PTGN_REGISTER_SCRIPT(
	WaitScript,
	(ScriptRegistrationOptions{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.requires_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 250.0f },
	})
);

PTGN_REGISTER_SCRIPT(
	MoveToScript,
	(ScriptRegistrationOptions{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 300.0f,
			.ease = Ease::OutCubic,
		},
	})
);

PTGN_REGISTER_SCRIPT(
	RotateToScript,
	(ScriptRegistrationOptions{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{ .duration_ms = 300.0f },
	})
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript,
	(ScriptRegistrationOptions{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 180.0f,
			.ease = Ease::OutBack,
		},
	})
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript,
	(ScriptRegistrationOptions{ .completion = ScriptCompletion::ScriptControlled })
);

PTGN_REGISTER_SCRIPT(
	NativeScript,
	(ScriptRegistrationOptions{
		.completion = ScriptCompletion::ScriptControlled,
		.supports_timing = true,
		.serializable = false,
	})
);

PTGN_REGISTER_SCRIPT(
	SetVisibleScript,
	(ScriptRegistrationOptions{ .completion = ScriptCompletion::Instant })
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript,
	(ScriptRegistrationOptions{ .completion = ScriptCompletion::Instant })
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript,
	(ScriptRegistrationOptions{ .completion = ScriptCompletion::Instant })
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript,
	(ScriptRegistrationOptions{ .completion = ScriptCompletion::Instant })
);

#define PTGN_REGISTER_KEY_EVENT(Type)                                                    \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(SequenceEventRegistrationOptions<Type>{                                            \
			.default_value = json{ { "key", Key::W } },                                      \
			.matches = &MatchKeyEvent<Type>,                                                  \
		})                                                                                  \
	)

#define PTGN_REGISTER_MOUSE_EVENT(Type)                                                  \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(SequenceEventRegistrationOptions<Type>{                                            \
			.default_value = json{ { "button", Mouse::Left } },                               \
			.matches = &MatchMouseEvent<Type>,                                                \
		})                                                                                  \
	)

#define PTGN_REGISTER_EMPTY_EVENT(Type)                                                  \
	PTGN_REGISTER_EVENT(                                                                  \
		Type,                                                                               \
		(SequenceEventRegistrationOptions<Type>{ .matches = &MatchAlwaysEvent<Type> })       \
	)

PTGN_REGISTER_KEY_EVENT(event::KeyPressed);
PTGN_REGISTER_KEY_EVENT(event::KeyHeld);
PTGN_REGISTER_KEY_EVENT(event::KeyReleased);
PTGN_REGISTER_MOUSE_EVENT(event::MousePressed);
PTGN_REGISTER_MOUSE_EVENT(event::MouseHeld);
PTGN_REGISTER_MOUSE_EVENT(event::MouseReleased);

PTGN_REGISTER_EMPTY_EVENT(event::MouseMoveOver);
PTGN_REGISTER_EMPTY_EVENT(event::MouseMoveOut);
PTGN_REGISTER_MOUSE_EVENT(event::MousePressedOver);
PTGN_REGISTER_MOUSE_EVENT(event::MouseHeldOver);
PTGN_REGISTER_MOUSE_EVENT(event::MouseReleasedOver);
PTGN_REGISTER_EMPTY_EVENT(event::ButtonPress);

PTGN_REGISTER_EMPTY_EVENT(event::DragStart);
PTGN_REGISTER_EMPTY_EVENT(event::Drag);
PTGN_REGISTER_EMPTY_EVENT(event::DragStop);

PTGN_REGISTER_EMPTY_EVENT(event::OverlapStart);
PTGN_REGISTER_EMPTY_EVENT(event::Overlap);
PTGN_REGISTER_EMPTY_EVENT(event::OverlapStop);
PTGN_REGISTER_EMPTY_EVENT(event::Collision);

PTGN_REGISTER_EVENT(
	Signal,
	(SequenceEventRegistrationOptions<Signal>{
		.default_value = json{ { "signal", "" } },
		.matches = [](Entity, const json& value, const Signal& event) {
			return std::string_view{ event.key } ==
				JsonValueOr<std::string>(value, "signal", "");
		},
	})
);

#undef PTGN_REGISTER_KEY_EVENT
#undef PTGN_REGISTER_MOUSE_EVENT
#undef PTGN_REGISTER_EMPTY_EVENT

namespace impl {

void EnsureEngineScriptsRegistered() {
	// Intentionally empty.
	//
	// Referencing this function forces the linker to include this object file. The namespace-scope
	// PTGN_REGISTER_SCRIPT and PTGN_REGISTER_EVENT initializers then populate the runtime registries.
}

} // namespace impl

} // namespace ptgn
