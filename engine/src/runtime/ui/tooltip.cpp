#include "runtime/ui/tooltip.h"

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/math/easing.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "core/graphics/color.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"

#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/interaction/interactive.h"

namespace ptgn {

Tooltip::Tooltip(Entity entity) : Entity{ entity } {}

void Tooltip::Show(V2_float position) {
	auto parent_scale{ GetScale(GetParent(*this)) };
	PTGN_ASSERT(!parent_scale.HasZero(), "Attempting division by zero");
	SetPosition(*this, position / parent_scale);

	auto& instance{ Entity::Get<impl::TooltipData>() };

	milliseconds fade_in_duration{ instance.fade_in_duration };
	Ease fade_in_ease{ instance.fade_in_ease };

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

	milliseconds fade_out_duration{ instance.fade_out_duration };
	Ease fade_out_ease{ instance.fade_out_ease };

	bool fade_out_force{ true };

	const auto fade_out = [=](auto& entity) {
		FadeOut(entity, fade_out_duration, fade_out_ease, fade_out_force);
	};

	fade_out(instance.text);

	if (instance.bg.has_value()) {
		fade_out(*instance.bg);
	}
}

std::optional<Tooltip> Tooltip::Get(Scene& scene, std::string_view tooltip_name) {
	auto key{ Hash(tooltip_name) };
	for (auto [entity, tooltip] : scene.EntitiesWith<impl::TooltipData>()) {
		if (tooltip.hash == key) {
			return Tooltip{ entity };
		}
	}
	return std::nullopt;
}

TooltipHoverScript::TooltipHoverScript(std::string_view name, V2_float tooltip_offset) :
	name{ name }, offset{ tooltip_offset } {}

void TooltipHoverScript::OnEvent(Event d) {
	d.Dispatch<event::MouseEnter>(&TooltipHoverScript::OnMouseEnter, this);
	d.Dispatch<event::MouseLeave>(&TooltipHoverScript::OnMouseLeave, this);
}

void TooltipHoverScript::OnCreate() {
	auto& manager{ entity.GetManager() };
	manager.Refresh();
	auto tooltip{ GetTooltip() };
	AddChild(entity, tooltip);
	IgnoreParentRotation(tooltip);
	IgnoreParentScale(tooltip);
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
	Scene& scene, std::string_view tooltip_name, const TooltipProperties& tooltip_properties
) {
	PTGN_ASSERT(
		!Tooltip::Get(scene, tooltip_name).has_value(), "Tooltip with the name: ", tooltip_name,
		" already exists in the manager"
	);

	std::optional<Texture> resolved_texture;

	if (tooltip_properties.texture.has_value()) {
		const auto& assets{ scene.ctx().asset };
		resolved_texture = tooltip_properties.texture->Get(assets);
	}

	Tooltip tooltip{ scene.CreateEntity() };

	auto& instance{ tooltip.Add<impl::TooltipData>() };

	instance.hash			   = Hash(tooltip_name);
	instance.fade_in_duration  = tooltip_properties.fade_in_duration;
	instance.fade_out_duration = tooltip_properties.fade_out_duration;
	instance.fade_in_ease	   = tooltip_properties.fade_in_ease;
	instance.fade_out_ease	   = tooltip_properties.fade_out_ease;

	if (resolved_texture.has_value()) {
		instance.bg = GameObject{ CreateSprite(scene, *resolved_texture, {}, Origin::Center) };
		SetTint(*instance.bg, color::Transparent);
		AddChild(tooltip, *instance.bg);
	}

	instance.text = GameObject{
		CreateText(scene, {}, tooltip_properties.content, tooltip_properties.text_color)
	};
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
	Entity entity, std::string_view tooltip_name, const TooltipProperties& tooltip_properties,
	V2_float tooltip_offset
) {
	SetInteractive(entity);

	if (entity.Has<Texture>() && !HasInteractiveShape(entity)) {
		auto rect{ entity.GetScene().CreateEntity() };
		V2_float size{ *GetTextureSize(entity) };
		rect.Add<Rect>(size);
		AddInteractiveShape(entity, GameObject{ std::move(rect) });
	}

	PTGN_ASSERT(
		HasInteractiveShape(entity), "Cannot AddTooltipOnHover to entity with no interactive shape"
	);

	auto& scene{ entity.GetScene() };

	auto tooltip = CreateTooltip(scene, tooltip_name, tooltip_properties);

	ShowTooltipOnHover(entity, tooltip_name, tooltip_offset);

	return tooltip;
}

} // namespace ptgn