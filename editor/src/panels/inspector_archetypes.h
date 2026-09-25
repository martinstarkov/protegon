#pragma once

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"

namespace ptgn::editor::inspector {

enum class InspectorArchetype : std::uint8_t {
	Generic,
	Tilemap,
	PaintGenerator,

	Rect,
	Circle,
	RoundedRect,
	Polygon,
	Ellipse,
	Triangle,
	Line,
	Capsule,
	Arc,

	Sprite,
	SpriteStack,
	Text,
	ParticleEmitter,
	Light,
	Graphics,
	CustomShader,
	RenderTarget,
	Camera,
	Effect,

	Button,
	ToggleButton,
	Slider,
	Dropdown,
	ToggleGroup,
	Tooltip,
	Dialogue,
};

[[nodiscard]] inline std::string NormalizeArchetypeName(std::string_view input) {
	std::string result;
	result.reserve(input.size());
	for (const char c : input) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result.push_back(
				static_cast<char>(
					std::tolower(static_cast<unsigned char>(c))
				)
			);
		}
	}
	return result;
}

template <typename Target, typename T>
[[nodiscard]] bool HasArchetypeComponent(const Target& target) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		return target.template Capture<T>().has_value();
	}
}

template <typename Target>
[[nodiscard]] std::string GetArchetypeDrawableName(const Target& target) {
	using Drawable = ::ptgn::impl::IDrawable;
	if constexpr (!Target::template Supports<Drawable>()) {
		return {};
	} else {
		const auto drawable{ target.template Capture<Drawable>() };
		if (!drawable) {
			return {};
		}
		const auto* info{ Drawable::FindInfo(drawable->hash) };
		if (!info) {
			return {};
		}
		return std::string{ info->GetDisplayName() };
	}
}

template <typename Target>
[[nodiscard]] InspectorArchetype ResolveInspectorArchetype(const Target& target) {
	// Authoring identities are inferred from the components that own their persistent state.
	// Keeping this component-based lets scene entities and serializable prefab targets share the
	// same archetype routing instead of special-casing individual inspector windows.
	if (HasArchetypeComponent<Target, ::ptgn::impl::TilemapData>(target)) {
		return InspectorArchetype::Tilemap;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::PaintGeneratorData>(target)) {
		return InspectorArchetype::PaintGenerator;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::CameraData>(target)) {
		return InspectorArchetype::Camera;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::RenderTargetDesc>(target)) {
		return InspectorArchetype::RenderTarget;
	}

	if (HasArchetypeComponent<Target, ::ptgn::impl::SliderData>(target)) {
		return InspectorArchetype::Slider;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::ToggleButtonData>(target)) {
		return InspectorArchetype::ToggleButton;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::DropdownData>(target)) {
		return InspectorArchetype::Dropdown;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::DialogueData>(target)) {
		return InspectorArchetype::Dialogue;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::ToggleButtonGroupData>(target)) {
		return InspectorArchetype::ToggleGroup;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::TooltipData>(target)) {
		return InspectorArchetype::Tooltip;
	}
	if (HasArchetypeComponent<Target, ::ptgn::impl::ButtonData>(target)) {
		return InspectorArchetype::Button;
	}

	if (
		HasArchetypeComponent<Target, ::ptgn::impl::EffectTag>(target) ||
		HasArchetypeComponent<Target, ::ptgn::impl::HDREffectTag>(target)
	) {
		return InspectorArchetype::Effect;
	}

	const std::string drawable_name{ GetArchetypeDrawableName(target) };
	const std::string visual{ NormalizeArchetypeName(drawable_name) };
	if (visual.empty()) {
		return InspectorArchetype::Generic;
	}

	if (visual == "rect") return InspectorArchetype::Rect;
	if (visual == "circle") return InspectorArchetype::Circle;
	if (visual == "roundedrect") return InspectorArchetype::RoundedRect;
	if (visual == "polygon") return InspectorArchetype::Polygon;
	if (visual == "ellipse") return InspectorArchetype::Ellipse;
	if (visual == "triangle") return InspectorArchetype::Triangle;
	if (visual == "line") return InspectorArchetype::Line;
	if (visual == "capsule") return InspectorArchetype::Capsule;
	if (visual == "arc") return InspectorArchetype::Arc;
	if (visual == "spritestack") return InspectorArchetype::SpriteStack;
	if (visual.contains("sprite")) return InspectorArchetype::Sprite;
	if (visual.contains("text")) return InspectorArchetype::Text;
	if (visual.contains("particle")) return InspectorArchetype::ParticleEmitter;
	if (visual.contains("light")) return InspectorArchetype::Light;
	if (visual.contains("graphics")) return InspectorArchetype::Graphics;
	if (visual.contains("customshader")) return InspectorArchetype::CustomShader;
	if (visual.contains("rendertarget")) return InspectorArchetype::RenderTarget;
	if (
		visual.contains("bloom") ||
		visual.contains("blur") ||
		visual.contains("effect") ||
		visual.contains("grayscale") ||
		visual.contains("inverse") ||
		visual.contains("sharpen") ||
		visual.contains("edge")
	) {
		return InspectorArchetype::Effect;
	}

	return InspectorArchetype::Generic;
}

[[nodiscard]] inline std::string_view GetInspectorArchetypeLabel(
	InspectorArchetype archetype
) {
	switch (archetype) {
		case InspectorArchetype::Generic: return "Entity";
		case InspectorArchetype::Tilemap: return "Tilemap";
		case InspectorArchetype::PaintGenerator: return "Paint Generator";
		case InspectorArchetype::Rect: return "Rectangle";
		case InspectorArchetype::Circle: return "Circle";
		case InspectorArchetype::RoundedRect: return "Rounded Rectangle";
		case InspectorArchetype::Polygon: return "Polygon";
		case InspectorArchetype::Ellipse: return "Ellipse";
		case InspectorArchetype::Triangle: return "Triangle";
		case InspectorArchetype::Line: return "Line";
		case InspectorArchetype::Capsule: return "Capsule";
		case InspectorArchetype::Arc: return "Arc";
		case InspectorArchetype::Sprite: return "Sprite";
		case InspectorArchetype::SpriteStack: return "Sprite Stack";
		case InspectorArchetype::Text: return "Text";
		case InspectorArchetype::ParticleEmitter: return "Particle Emitter";
		case InspectorArchetype::Light: return "Light";
		case InspectorArchetype::Graphics: return "Graphics";
		case InspectorArchetype::CustomShader: return "Custom Shader";
		case InspectorArchetype::RenderTarget: return "Render Target";
		case InspectorArchetype::Camera: return "Camera";
		case InspectorArchetype::Effect: return "Effect";
		case InspectorArchetype::Button: return "Button";
		case InspectorArchetype::ToggleButton: return "Toggle Button";
		case InspectorArchetype::Slider: return "Slider";
		case InspectorArchetype::Dropdown: return "Dropdown";
		case InspectorArchetype::ToggleGroup: return "Toggle Group";
		case InspectorArchetype::Tooltip: return "Tooltip";
		case InspectorArchetype::Dialogue: return "Dialogue Box";
	}
	return "Entity";
}

template <typename Target>
[[nodiscard]] std::string GetInspectorArchetypeLabel(const Target& target) {
	const InspectorArchetype archetype{
		ResolveInspectorArchetype(target)
	};
	if (archetype == InspectorArchetype::Effect) {
		const std::string drawable{
			GetArchetypeDrawableName(target)
		};
		return drawable.empty()
			? std::string{ "Effect" }
			: drawable;
	}
	return std::string{
		GetInspectorArchetypeLabel(archetype)
	};
}

[[nodiscard]] inline bool IsVisualArchetype(
	InspectorArchetype archetype
) {
	return
		archetype >= InspectorArchetype::Rect &&
		archetype <= InspectorArchetype::Effect &&
		archetype != InspectorArchetype::Camera;
}

[[nodiscard]] inline bool IsUIArchetype(
	InspectorArchetype archetype
) {
	return
		archetype >= InspectorArchetype::Button &&
		archetype <= InspectorArchetype::Dialogue;
}

[[nodiscard]] inline bool ArchetypeRequiresTransform(
	InspectorArchetype archetype
) {
	switch (archetype) {
		case InspectorArchetype::Generic:
		case InspectorArchetype::ToggleGroup:
		case InspectorArchetype::RenderTarget:
		case InspectorArchetype::Effect:
			return false;

		case InspectorArchetype::Tilemap:
		case InspectorArchetype::PaintGenerator:
			return true;

		default:
			return true;
	}
}

[[nodiscard]] inline bool ArchetypeOwnsVisual(
	InspectorArchetype archetype
) {
	return IsVisualArchetype(archetype);
}

[[nodiscard]] inline bool ArchetypeOwnsUI(
	InspectorArchetype archetype
) {
	return IsUIArchetype(archetype);
}

[[nodiscard]] inline bool ArchetypeOwnsCamera(
	InspectorArchetype archetype
) {
	return archetype == InspectorArchetype::Camera;
}

} // namespace ptgn::editor::inspector
