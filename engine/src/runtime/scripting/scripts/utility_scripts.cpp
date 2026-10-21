#include "runtime/scripting/scripts/utility_scripts.h"

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

std::string ResolveConsoleMessage(std::string_view message, Entity target) {
	std::string resolved;
	resolved.reserve(message.size());

	std::size_t offset{ 0 };
	while (offset < message.size()) {
		std::size_t opening{ message.find('{', offset) };
		if (opening == std::string_view::npos) {
			resolved.append(message.substr(offset));
			break;
		}

		resolved.append(message.substr(offset, opening - offset));
		std::size_t closing{ message.find('}', opening + 1) };
		if (closing == std::string_view::npos) {
			resolved.append(message.substr(opening));
			break;
		}

		std::string_view expression{ message.substr(opening + 1, closing - opening - 1) };
		bool is_counter{ expression.starts_with("counter:") };
		bool is_bool{ expression.starts_with("bool:") };
		if (!is_counter && !is_bool) {
			resolved.append(message.substr(opening, closing - opening + 1));
			offset = closing + 1;
			continue;
		}

		std::string_view name{ expression.substr(is_counter ? 8 : 5) };
		if (is_counter) {
			if (target && !name.empty()) {
				CounterHandle counter{ GetCounter(target, CounterKey{ std::string{ name } }) };
				if (counter) {
					resolved += std::to_string(counter.Value());
					offset	  = closing + 1;
					continue;
				}
			}
			resolved += "<missing counter:";
			resolved.append(name);
			resolved += '>';
		} else {
			if (target && !name.empty()) {
				BooleanHandle boolean{ GetBoolean(target, BooleanKey{ std::string{ name } }) };
				if (boolean) {
					resolved += boolean.Value() ? "true" : "false";
					offset	  = closing + 1;
					continue;
				}
			}
			resolved += "<missing bool:";
			resolved.append(name);
			resolved += '>';
		}
		offset = closing + 1;
	}

	return resolved;
}

void PrintConsoleScript::OnStart() {
	std::string prefix;
	switch (level) {
		case ConsoleLogLevel::Debug:   prefix = "[DEBUG]"; break;
		case ConsoleLogLevel::Info:	   prefix = "[INFO]"; break;
		case ConsoleLogLevel::Warning: prefix = "[WARN]"; break;
		case ConsoleLogLevel::Error:   prefix = "[ERROR]"; break;
	}

	if (include_timestamp) {
		std::ostringstream time;
		time << std::fixed << std::setprecision(3) << GetScene().ctx().GameTime().count();
		prefix += " [" + time.str() + "s]";
	}

	if (include_entity) {
		std::ostringstream identifier;
		Entity entity{ Target() };
		if (entity && entity.Has<UUID>()) {
			identifier << entity.Get<UUID>();
		} else {
			identifier << "Unknown";
		}
		prefix += " [Entity: " + identifier.str() + "]";
	}

	if (include_sequence) {
		prefix += " [" + std::string{ GetSequenceName() } + "]";
	}

	prefix += ' ';
	std::string parsed{ ResolveConsoleMessage(message, Target()) };
	std::size_t begin{ 0 };
	while (true) {
		std::size_t end{ parsed.find('\n', begin) };
		PrintLine(prefix, std::string_view{ parsed }.substr(begin, end - begin));
		if (end == std::string::npos) {
			break;
		}
		begin = end + 1;
	}
}

void EmitSignalScript::OnStart() {
	script_runtime::DispatchGlobal<Signal>(GetScene(), Signal{ signal });
}

ScriptSequence& ScriptSequence::EmitSignal(SignalKey signal) {
	return Then(EmitSignalScript{ std::move(signal) });
}

ScriptSequence& EmitSignal(ScriptSequence& sequence, SignalKey signal) {
	return sequence.EmitSignal(std::move(signal));
}

SequenceHandle After(Scene& scene, milliseconds duration, SequenceFunction function) {
	Entity owner{ scene.CreateEntity("After Script Sequence") };
	auto handle_ref{ std::make_shared<SequenceHandle>() };
	NativeScript callback{ NativeScriptCallbacks{
		.on_start =
			[function = std::move(function), handle_ref](Script&) mutable {
				std::visit(
					[&](auto& fn) {
						if constexpr (std::is_invocable_v<decltype(fn), SequenceHandle>) {
							fn(*handle_ref);
						} else {
							fn();
						}
					},
					function
				);
			},
	} };
	ScriptSequence sequence{ "After" };
	sequence.Wait(duration).Then(std::move(callback)).Transient().DestroyOwnerOnComplete();
	auto handle{ script_runtime::RunSequence(owner, std::move(sequence)) };
	*handle_ref = handle;
	handle.Start();

	return handle;
}

SequenceHandle During(Scene& scene, milliseconds duration, DuringSequenceFunction function) {
	Entity owner{ scene.CreateEntity("During Script Sequence") };
	NativeScript callback{ NativeScriptCallbacks{
		.on_update =
			[function = std::move(function)](Script& script) mutable {
				std::visit(
					[&](auto& fn) {
						if constexpr (std::is_invocable_v<decltype(fn), Entity, float>) {
							fn(script.Target(), script.Progress());
						} else {
							fn();
						}
					},
					function
				);

				return ScriptStatus::Running;
			},
	} };
	ScriptSequence sequence{ "During" };
	sequence.During(duration, std::move(callback)).Transient().DestroyOwnerOnComplete();
	auto handle{ script_runtime::RunSequence(owner, std::move(sequence)) };
	handle.Start();

	return handle;
}

} // namespace ptgn
