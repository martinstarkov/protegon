#include "runtime/ui/tooltip.h"

#include <optional>
#include <string_view>
#include <variant>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/math/easing.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/interactive.h"

namespace ptgn {

Tooltip::Tooltip(Entity entity) : Entity{ entity } {}

void Tooltip::Show(V2_float position) {
	SetPosition(*this, position);

	auto& instance{ Entity::Get<impl::TooltipData>() };

	// TODO: Make these customizable.
	milliseconds fade_in_duration{ 250 };
	Ease fade_in_ease{ Ease::Linear };

	bool fade_in_force{ true };

	const auto fade_in = [=](auto& entity) {
		SetTint(entity, color::Transparent);
		FadeIn(entity, fade_in_duration, fade_in_ease, fade_in_force);
	};

	fade_in(instance.text);

	if (instance.bg.has_value()) {
		fade_in(*instance.bg);
	}
}

void Tooltip::Hide() {
	auto& instance{ Entity::Get<impl::TooltipData>() };

	// TODO: Make these customizable.
	milliseconds fade_out_duration{ 250 };
	Ease fade_out_ease{ Ease::Linear };

	bool fade_out_force{ true };

	const auto fade_out = [=](auto& entity) {
		FadeOut(entity, fade_out_duration, fade_out_ease, fade_out_force);
	};

	fade_out(instance.text);

	if (instance.bg.has_value()) {
		fade_out(*instance.bg);
	}
}

std::optional<Tooltip> Tooltip::Get(Scene& scene, std::string_view name) {
	auto key{ Hash(name) };
	for (auto [entity, tooltip] : scene.EntitiesWith<impl::TooltipData>()) {
		if (tooltip.hash == key) {
			return Tooltip{ entity };
		}
	}
	return {};
}

TooltipHoverScript::TooltipHoverScript(std::string_view name, V2_float offset) :
	name{ name }, offset{ offset } {}

void TooltipHoverScript::OnEvent(EventDispatcher d) {
	d.Dispatch<MouseEnter>([this](const MouseEnter&) { OnMouseEnter(); });
	d.Dispatch<MouseLeave>([this](const MouseLeave&) { OnMouseLeave(); });
}

void TooltipHoverScript::OnCreate() {
	auto& manager{ entity.GetManager() };
	manager.Refresh();
	auto tooltip{ GetTooltip() };
	AddChild(entity, tooltip);
}

void TooltipHoverScript::OnMouseEnter() {
	auto tooltip{ GetTooltip() };
	tooltip.Show(offset);
}

void TooltipHoverScript::OnMouseLeave() {
	auto tooltip{ GetTooltip() };
	tooltip.Hide();
}

Tooltip TooltipHoverScript::GetTooltip() {
	auto& scene{ entity.GetScene() };
	auto tooltip{ Tooltip::Get(scene, name) };
	PTGN_ASSERT(
		tooltip.has_value(), "Tooltip with the name: ", name, " does not exist in the manager"
	);
	return *tooltip;
}

Tooltip CreateTooltip(
	Scene& scene, std::string_view name, std::string_view content, Color text_color,
	std::variant<std::monostate, Texture, std::string_view> texture
) {
	PTGN_ASSERT(
		!Tooltip::Get(scene, name).has_value(), "Tooltip with the name: ", name,
		" already exists in the manager"
	);

	std::optional<Texture> resolved_texture{ scene.app().asset.ToTexture(texture) };

	Tooltip tooltip{ scene.CreateEntity() };

	auto& instance{ tooltip.Add<impl::TooltipData>() };

	instance.hash = Hash(name);

	if (resolved_texture.has_value()) {
		instance.bg = GameObject{ CreateSprite(scene, *resolved_texture, {}, Origin::Center) };
		SetTint(*instance.bg, color::Transparent);
		AddChild(tooltip, *instance.bg);
	}

	instance.text = GameObject{ CreateText(scene, content, text_color) };
	SetTint(instance.text, color::Transparent);
	AddChild(tooltip, instance.text);

	return tooltip;
}

void ShowTooltipOnHover(Entity entity, std::string_view tooltip_name, V2_float tooltip_offset) {
	PTGN_ASSERT(
		entity.Has<impl::Interactive>(), "Entity that shows tooltip on hover should be interactive"
	);
	AddScript<TooltipHoverScript>(entity, tooltip_name, tooltip_offset);
}

Tooltip AddTooltipOnHover(
	Entity entity, std::string_view tooltip_name, std::string_view tooltip_content,
	Color tooltip_text_color,
	std::variant<std::monostate, Texture, std::string_view> tooltip_texture, V2_float tooltip_offset
) {
	SetInteractive(entity);

	auto& scene{ entity.GetScene() };

	auto tooltip =
		CreateTooltip(scene, tooltip_name, tooltip_content, tooltip_text_color, tooltip_texture);

	ShowTooltipOnHover(entity, tooltip_name, tooltip_offset);

	return tooltip;
}

} // namespace ptgn