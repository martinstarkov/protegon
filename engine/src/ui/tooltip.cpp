#include "ui/tooltip.h"

#include <string>
#include <string_view>

#include "core/app/manager.h"
#include "core/assert.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/ecs/entity_hierarchy.h"
#include "core/ecs/game_object.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/time.h"
#include "math/easing.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/material/texture.h"
#include "renderer/text/text.h"
#include "tween/tween_effect.h"

namespace ptgn {

Tooltip::Tooltip(const Entity& entity) : Entity{ entity } {}

void Tooltip::Show(const V2_float& position) {
	SetPosition(*this, position);

	auto& instance{ Entity::Get<impl::TooltipInstance>() };

	milliseconds fade_in_duration{ 250 };
	SymmetricalEase fade_in_ease{ SymmetricalEase::Linear };
	bool fade_in_force{ true };

	const auto fade_in = [=](auto& entity) {
		SetTint(entity, color::Transparent);
		FadeIn(entity, fade_in_duration, fade_in_ease, fade_in_force);
	};

	fade_in(instance.text);
	fade_in(instance.bg);
}

void Tooltip::Hide() {
	auto& instance{ Entity::Get<impl::TooltipInstance>() };

	milliseconds fade_out_duration{ 250 };
	SymmetricalEase fade_out_ease{ SymmetricalEase::Linear };
	bool fade_out_force{ true };

	const auto fade_out = [=](auto& entity) {
		FadeOut(entity, fade_out_duration, fade_out_ease, fade_out_force);
	};

	fade_out(instance.text);
	fade_out(instance.bg);
}

Tooltip Tooltip::Get(Manager& manager, std::string_view name) {
	auto key{ Hash(name) };
	for (auto [entity, tooltip] : manager.EntitiesWith<impl::TooltipInstance>()) {
		if (tooltip.hash == key) {
			return entity;
		}
	}
	return {};
}

TooltipHoverScript::TooltipHoverScript(const std::string& name, const V2_float& offset) :
	name{ name }, offset{ offset } {}

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
	auto& manager{ entity.GetManager() };
	auto tooltip{ Tooltip::Get(manager, name) };
	PTGN_ASSERT(tooltip);
	return tooltip;
}

Tooltip CreateTooltip(
	Manager& manager, std::string_view name, std::string_view content, const Color& text_color,
	const TextureHandle& texture_key
) {
	PTGN_ASSERT(
		Tooltip::Get(manager, name) == Tooltip{}, "Tooltip with the name: ", name,
		" already exists in the manager"
	);

	Tooltip tooltip{ manager.CreateEntity() };

	auto& instance{ tooltip.Add<impl::TooltipInstance>() };

	instance.bg	  = CreateSprite(manager, texture_key, {}, Origin::Center);
	instance.text = CreateText(manager, content, text_color);
	instance.hash = Hash(name);

	SetTint(instance.bg, color::Transparent);
	SetTint(instance.text, color::Transparent);

	AddChild(tooltip, instance.bg);
	AddChild(tooltip, instance.text);

	return tooltip;
}

} // namespace ptgn