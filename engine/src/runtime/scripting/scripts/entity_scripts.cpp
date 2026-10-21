#include "runtime/scripting/scripts/entity_scripts.h"

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

void SetVisibleScript::OnStart() {
	SetVisible(Target(), visible);
}

void SetEnabledScript::OnStart() {
	const auto* registration{ ComponentRegistry::Find(component) };
	if (!registration || !registration->Has(Target())) {
		PTGN_WARN("Cannot set enabled state for missing component: ", component);

		return;
	}

	if (!registration->serializable || !registration->deserializable) {
		PTGN_WARN("Component does not support enabled state serialization: ", component);

		return;
	}

	json value;
	if (!registration->Serialize(value, Target())) {
		PTGN_WARN("Failed to serialize component enabled state: ", component);

		return;
	}

	if (value.is_boolean()) {
		value = enabled;
	} else if (value.is_object()) {
		auto it{ value.find("enabled") };
		if (it == value.end() || !it->is_boolean()) {
			PTGN_WARN("Component does not expose a bool enabled field: ", component);

			return;
		}

		*it = enabled;
	} else {
		PTGN_WARN("Component does not expose a supported enabled value: ", component);

		return;
	}

	if (!registration->Deserialize(value, Target())) {
		PTGN_WARN("Failed to deserialize component enabled state: ", component);
	}
}

void AddComponentsScript::OnStart() {
	for (const auto& component : components) {
		if (component.apply_live) {
			component.apply_live(Target());
			continue;
		}

		const auto* registration{ ComponentRegistry::Find(component.type) };
		if (!registration) {
			continue;
		}

		if (registration->is_empty && registration->default_constructible) {
			registration->AddDefault(Target());
		} else if (registration->deserializable && !component.value.is_null()) {
			registration->Deserialize(component.value, Target());
		} else if (registration->default_constructible) {
			registration->AddDefault(Target());
		}
	}
}

void RemoveComponentsScript::OnStart() {
	for (const auto& name : components) {
		if (const auto* registration{ ComponentRegistry::Find(name) }) {
			registration->Remove(Target());
		}
	}
}

ComponentDefinition MakeComponentDefinition(const RegisteredComponent& component) {
	auto default_value{ component.MakeDefaultJson() };
	if (!default_value) {
		return {};
	}

	json value{ std::move(*default_value) };
	if (value.is_null()) {
		value = json::object();
	}

	std::string component_name{ component.name };

	return ComponentDefinition{
		.type_hash = static_cast<TypeHashValue>(component.type_id),
		.type	   = component_name,
		.value	   = std::move(value),
		.apply_live =
			[component_name](Entity entity) {
				if (const auto* registration{ ComponentRegistry::Find(component_name) };
					registration && registration->default_constructible) {
					registration->AddDefault(entity);
				}
			},
	};
}

ComponentDefinition MakeComponentDefinition(std::string_view name) {
	const auto* component{ ComponentRegistry::Find(name) };

	return component ? MakeComponentDefinition(*component) : ComponentDefinition{};
}

void EntityDestroyEntityScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	target.Destroy(option);
}

void EntitySpawnPrefabScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (text.empty()) {
		return;
	}
	Entity spawned{ GetScene().CreatePrefab(std::string_view{ text }) };
	if (spawned && spawned.Has<Transform>() && target.Has<Transform>()) {
		SetPosition(spawned, GetPosition(target));
	}
}

void EntitySetColorScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	target.Add<Color>(selected_color);
}

void EntitySetDepthScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetDepth(target, number);
}

void EntitySetPositionScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetPosition(target, vector);
}

void EntityAddPositionScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	Translate(target, vector);
}

void EntitySetRotationScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetRotation(target, Degrees{ number });
}

void EntityAddRotationScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	Rotate(target, Degrees{ number });
}

void EntitySetScaleScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetScale(target, vector);
}

void EntityMultiplyScaleScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	Scale(target, vector);
}

void EntitySetRenderMaskScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	SetRenderMask(target, static_cast<RenderMaskBits>(std::max(0, index)));
}

void EntitySetBlendModeScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto mode{ magic_enum::enum_cast<BlendMode>(text) }) {
		SetBlendMode(target, *mode);
	}
}

void EntitySetOriginScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (auto origin{ magic_enum::enum_cast<Origin>(text) }) {
		target.Add<Origin>(*origin);
	}
}

void EntitySetParentScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	if (text.empty()) {
		return;
	}
	Entity parent{ GetScene().GetEntity(Tag{ text }) };
	if (!parent || parent == target) {
		return;
	}
	Transform world{ GetWorldTransform(target) };
	SetParent(target, parent);
	if (option) {
		SetWorldTransform(target, world);
	}
}

void EntityRemoveParentScript::OnStart() {
	Entity target{ Target() };
	if (!target) {
		return;
	}
	RemoveParent(target);
}

} // namespace ptgn
