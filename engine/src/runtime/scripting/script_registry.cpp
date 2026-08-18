// script_registry.cpp
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif
#include <magic_enum/magic_enum.hpp>

#include "runtime/scripting/script_registration_engine.h"

#include <algorithm>
#include <cctype>
#include <concepts>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/event/key_event.h"
#include "core/event/mouse_event.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "platform/window.h"
#include "runtime/animation/animation_event.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script_event.h"
#include "runtime/timer/timer.h"
#include "runtime/timer/timer_event.h"
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

[[nodiscard]] std::string TrimKeyToken(std::string token) {
	const auto first{ token.find_first_not_of(" \t\n\r\f\v") };
	if (first == std::string::npos) {
		return {};
	}

	const auto last{ token.find_last_not_of(" \t\n\r\f\v") };
	return token.substr(first, last - first + 1);
}

[[nodiscard]] std::string NormalizeKeyToken(std::string_view token) {
	std::string normalized;
	normalized.reserve(token.size());

	for (char c : token) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			normalized.push_back(
				static_cast<char>(
					std::tolower(static_cast<unsigned char>(c))
				)
			);
		}
	}

	return normalized;
}

[[nodiscard]] std::optional<Key> ParseKeyToken(std::string_view token) {
	std::string normalized{ NormalizeKeyToken(token) };
	if (normalized.empty()) {
		return std::nullopt;
	}

	if (normalized.size() == 1 &&
		std::isdigit(static_cast<unsigned char>(normalized.front()))) {
		normalized.insert(normalized.begin(), 'k');
	}

	if (normalized == "shift" || normalized == "lshift") {
		normalized = "leftshift";
	} else if (normalized == "rshift") {
		normalized = "rightshift";
	} else if (
		normalized == "ctrl" ||
		normalized == "control" ||
		normalized == "lctrl" ||
		normalized == "leftcontrol"
	) {
		normalized = "leftctrl";
	} else if (
		normalized == "rctrl" ||
		normalized == "rightcontrol"
	) {
		normalized = "rightctrl";
	} else if (
		normalized == "alt" ||
		normalized == "option" ||
		normalized == "lalt"
	) {
		normalized = "leftalt";
	} else if (normalized == "ralt") {
		normalized = "rightalt";
	} else if (
		normalized == "super" ||
		normalized == "cmd" ||
		normalized == "command"
	) {
		normalized = "leftsuper";
	}

	for (const auto [key, name] : magic_enum::enum_entries<Key>()) {
		if (NormalizeKeyToken(name) == normalized) {
			return key;
		}
	}

	return std::nullopt;
}

[[nodiscard]] std::vector<std::vector<Key>>
ParseKeyExpression(std::string expression) {
	std::vector<std::vector<Key>> alternatives;
	expression = TrimKeyToken(std::move(expression));

	while (true) {
		const std::size_t comma{ expression.find(',') };
		std::string group{
			TrimKeyToken(expression.substr(0, comma))
		};

		if (group.empty()) {
			return {};
		}

		std::vector<Key> keys;

		while (true) {
			const std::size_t plus{ group.find('+') };
			const std::string token{
				TrimKeyToken(group.substr(0, plus))
			};

			const auto key{ ParseKeyToken(token) };
			if (!key) {
				return {};
			}

			if (std::ranges::find(keys, *key) == keys.end()) {
				keys.push_back(*key);
			}

			if (plus == std::string::npos) {
				break;
			}

			group.erase(0, plus + 1);

			if (TrimKeyToken(group).empty()) {
				return {};
			}
		}

		if (keys.empty()) {
			return {};
		}

		alternatives.push_back(std::move(keys));

		if (comma == std::string::npos) {
			break;
		}

		expression.erase(0, comma + 1);

		if (TrimKeyToken(expression).empty()) {
			return {};
		}
	}

	return alternatives;
}

[[nodiscard]] std::string KeyExpressionOrLegacy(const json& value) {
	if (const auto expression{
			JsonValueOr<std::string>(
				value,
				"keys",
				""
			)
		};
		!expression.empty()) {
		return expression;
	}

	const Key key{
		JsonValueOr<Key>(
			value,
			"key",
			Key::W
		)
	};

	const std::string_view name{
		magic_enum::enum_name(key)
	};

	return name.empty()
		? std::string{ "W" }
		: std::string{ name };
}

template <typename TEvent>
[[nodiscard]] bool MatchKeyEvent(
	Entity owner,
	const json& value,
	const TEvent& event
) {
	if (!owner) {
		return false;
	}

	const auto alternatives{
		ParseKeyExpression(
			KeyExpressionOrLegacy(value)
		)
	};

	if (alternatives.empty()) {
		return false;
	}

	const auto& input{
		owner.GetScene().ctx().input
	};

	constexpr bool held_event{
		std::same_as<TEvent, event::KeyHeld>
	};

	constexpr bool released_event{
		std::same_as<TEvent, event::KeyReleased>
	};

	const bool require_held_duration{
		JsonValueOr<bool>(
			value,
			"require_held_duration",
			true
		)
	};

	const float held_ms{
		std::max(
			0.0f,
			JsonValueOr<float>(
				value,
				"held_duration_ms",
				250.0f
			)
		)
	};

	const milliseconds held_duration{
		static_cast<milliseconds::rep>(held_ms)
	};

	for (const auto& keys : alternatives) {
		if (std::ranges::find(keys, event.key) == keys.end()) {
			continue;
		}

		const bool complete{
			std::ranges::all_of(
				keys,
				[&](Key key) {
					if constexpr (released_event) {
						if (key == event.key) {
							return true;
						}
					}

					if constexpr (held_event) {
						return require_held_duration
							? input.KeyHeld(key, held_duration)
							: input.KeyHeld(key);
					}

					return input.KeyHeld(key);
				}
			)
		};

		if (complete) {
			return true;
		}
	}

	return false;
}

template <typename TEvent>
[[nodiscard]] bool MatchMouseEvent(
	Entity owner,
	const json& value,
	const TEvent& event
) {
	const Mouse button{
		JsonValueOr<Mouse>(
			value,
			"button",
			Mouse::Left
		)
	};

	if (EventMouse(event) != button) {
		return false;
	}

	if constexpr (
		std::same_as<TEvent, event::MouseHeld> ||
		std::same_as<TEvent, event::MouseHeldOver>
	) {
		if (!owner) {
			return false;
		}

		const bool require_held_duration{
			JsonValueOr<bool>(
				value,
				"require_held_duration",
				true
			)
		};

		if (!require_held_duration) {
			return owner.GetScene().ctx().input.MouseHeld(button);
		}

		const float held_ms{
			std::max(
				0.0f,
				JsonValueOr<float>(
					value,
					"held_duration_ms",
					250.0f
				)
			)
		};

		return owner.GetScene().ctx().input.MouseHeld(
			button,
			milliseconds{
				static_cast<milliseconds::rep>(
					held_ms
				)
			}
		);
	}

	return true;
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

	return collider.mode == CollisionMode::Discrete ||
		collider.mode == CollisionMode::Continuous;
}

[[nodiscard]] bool HasInteractive(Entity entity) {
	return entity &&
		entity.Has<impl::Interactive>();
}

[[nodiscard]] bool HasDraggable(Entity entity) {
	return HasInteractive(entity) &&
		entity.Has<impl::Draggable>();
}

[[nodiscard]] bool HasDropzone(Entity entity) {
	return HasInteractive(entity) &&
		entity.Has<impl::Dropzone>();
}

[[nodiscard]] bool HasAnimationData(Entity entity) {
	return entity &&
		entity.Has<impl::AnimationData>();
}

[[nodiscard]] bool HasButtonData(Entity entity) {
	return entity &&
		entity.Has<impl::ButtonData>();
}

[[nodiscard]] bool HasToggleButtonData(Entity entity) {
	return entity &&
		entity.Has<impl::ToggleButtonData>();
}

[[nodiscard]] bool HasDropdownData(Entity entity) {
	return entity &&
		entity.Has<impl::DropdownData>();
}

[[nodiscard]] bool HasTimers(Entity entity) {
	return entity && entity.Has<impl::Timers>();
}

} // namespace

PTGN_REGISTER_SCRIPT(Script);

PTGN_REGISTER_SCRIPT(
	WaitScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.requires_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 250.0f
		},
	}
);

PTGN_REGISTER_SCRIPT(
	MoveToScript,
	{
		.completion = ScriptCompletion::Instant,
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
		.completion = ScriptCompletion::Instant,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 300.0f
		},
	}
);

PTGN_REGISTER_SCRIPT(
	ScaleToScript,
	{
		.completion = ScriptCompletion::Instant,
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
		.completion = ScriptCompletion::Instant,
		.supports_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 300.0f
		},
	}
);

PTGN_REGISTER_SCRIPT(
	BounceScript,
	{
		.completion = ScriptCompletion::Duration,
		.supports_timing = true,
		.requires_timing = true,
		.default_timing = ScriptTiming{
			.duration_ms = 500.0f
		},
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
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	RecoverShakeScript,
	{
		.completion = ScriptCompletion::ScriptControlled
	}
);

PTGN_REGISTER_SCRIPT(
	ResetShakeScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	FollowTargetScript,
	{
		.completion = ScriptCompletion::ScriptControlled
	}
);

PTGN_REGISTER_SCRIPT(
	FollowEntityScript,
	{
		.completion = ScriptCompletion::ScriptControlled
	}
);

PTGN_REGISTER_SCRIPT(
	FollowPathScript,
	{
		.completion = ScriptCompletion::ScriptControlled
	}
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
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	PlaySoundScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	AnimationActionScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	TimerActionScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	SetTextureScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	SetEnabledScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	SceneChangeScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	EmitSignalScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	AddComponentsScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_SCRIPT(
	RemoveComponentsScript,
	{
		.completion = ScriptCompletion::Instant
	}
);

PTGN_REGISTER_EVENT(
	event::KeyPressed,
	{
		.default_value = json{
			{ "keys", "W" }
		},
		.matches = &MatchKeyEvent<event::KeyPressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyHeld,
	{
		.default_value = json{
			{ "keys", "W" },
			{ "require_held_duration", true },
			{ "held_duration_ms", 250.0f },
		},
		.matches = &MatchKeyEvent<event::KeyHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::KeyReleased,
	{
		.default_value = json{
			{ "keys", "W" }
		},
		.matches = &MatchKeyEvent<event::KeyReleased>,
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressed,
	{
		.default_value = MakeEventDefault(
			"button",
			Mouse::Left
		),
		.matches = &MatchMouseEvent<event::MousePressed>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeld,
	{
		.default_value = json{
			{ "button", Mouse::Left },
			{ "require_held_duration", true },
			{ "held_duration_ms", 250.0f },
		},
		.matches = &MatchMouseEvent<event::MouseHeld>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleased,
	{
		.default_value = MakeEventDefault(
			"button",
			Mouse::Left
		),
		.matches = &MatchMouseEvent<event::MouseReleased>,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOver,
	{
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseMoveOut,
	{
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MousePressedOver,
	{
		.default_value = MakeEventDefault(
			"button",
			Mouse::Left
		),
		.matches = &MatchMouseEvent<event::MousePressedOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseHeldOver,
	{
		.default_value = json{
			{ "button", Mouse::Left },
			{ "require_held_duration", true },
			{ "held_duration_ms", 250.0f },
		},
		.matches = &MatchMouseEvent<event::MouseHeldOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::MouseReleasedOver,
	{
		.default_value = MakeEventDefault(
			"button",
			Mouse::Left
		),
		.matches = &MatchMouseEvent<event::MouseReleasedOver>,
		.available = &HasInteractive,
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonPress,
	{
		.available = &HasButtonData
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStart,
	{
		.available = &HasButtonData
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonHover,
	{
		.available = &HasButtonData
	}
);

PTGN_REGISTER_EVENT(
	event::ButtonHoverStop,
	{
		.available = &HasButtonData
	}
);

PTGN_REGISTER_EVENT(
	event::ToggleButtonToggle,
	{
		.available = &HasToggleButtonData
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownOpen,
	{
		.available = &HasDropdownData
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownClose,
	{
		.available = &HasDropdownData
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownToggle,
	{
		.available = &HasDropdownData
	}
);

PTGN_REGISTER_EVENT(
	event::DropdownItemPress,
	{
		.available = &HasDropdownData
	}
);

PTGN_REGISTER_EVENT(
	event::DragStart,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::Drag,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DragStop,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::PickupDraggable,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DropDraggable,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DragEnter,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DragLeave,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DragOver,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::DragOut,
	{
		.available = &HasDraggable
	}
);

PTGN_REGISTER_EVENT(
	event::PickupFromDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::DropIntoDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::EnterDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::LeaveDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::MoveOverDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::MoveOutsideDropzone,
	{
		.available = &HasDropzone
	}
);

PTGN_REGISTER_EVENT(
	event::OverlapStart,
	{
		.available = &HasOverlapCollider
	}
);

PTGN_REGISTER_EVENT(
	event::Overlap,
	{
		.available = &HasOverlapCollider
	}
);

PTGN_REGISTER_EVENT(
	event::OverlapStop,
	{
		.available = &HasOverlapCollider
	}
);

PTGN_REGISTER_EVENT(
	event::Collision,
	{
		.available = &HasCollisionCollider
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationStart,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationStop,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationPause,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationResume,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationFrameChange,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationUpdate,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationFinalFrame,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationComplete,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(
	event::AnimationLoopComplete,
	{
		.available = &HasAnimationData
	}
);

PTGN_REGISTER_EVENT(event::EntityCreated);

PTGN_REGISTER_EVENT(
	event::TimerElapsed,
	{
		.default_value = [] {
			auto value = MakeEventDefault(
				"timer",
				TimerKey{}
			);
			value["duration"] = nullptr;
			return value;
		}(),
		.matches = [](
			Entity,
			const json& value,
			const event::TimerElapsed& event
		) {
			if (event.timer != JsonValueOr<TimerKey>(
					value,
					"timer",
					TimerKey{}
				)) {
				return false;
			}

			if (!value.is_object()) {
				return event.completed;
			}

			const auto duration_it{ value.find("duration") };
			if (duration_it == value.end() || duration_it->is_null()) {
				return event.completed;
			}

			millisecondsf duration;
			try {
				duration_it->get_to(duration);
			} catch (...) {
				return false;
			}

			if (duration <= millisecondsf{ 0.0f }) {
				return false;
			}

			return event.previous_elapsed < duration &&
				   event.elapsed >= duration;
		},
		.available = &HasTimers,
	}
);

PTGN_REGISTER_EVENT(
	Signal,
	{
		.default_value = MakeEventDefault(
			"signal",
			std::string{}
		),
		.matches = [](
			Entity,
			const json& value,
			const Signal& event
		) {
			return std::string_view{
				event.key
			} ==
				JsonValueOr<std::string>(
					value,
					"signal",
					""
				);
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
