#include "runtime/ui/tooltip.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/easing.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

bool IsTooltipPart(Entity entity, impl::TooltipPart part) {
	switch (part) {
		case impl::TooltipPart::Background: return entity.Has<impl::TooltipBackgroundPart>();
		case impl::TooltipPart::Text:       return entity.Has<impl::TooltipTextPart>();
	}

	return false;
}

} // namespace

Tooltip::Tooltip(Entity entity) : Entity{ entity } {}

void Tooltip::Show(V2_float position) {
	auto parent_scale{ GetScale(GetParent(*this)) };
	PTGN_ASSERT(!parent_scale.HasZero(), "Attempting division by zero");
	SetPosition(*this, position / parent_scale);

	const auto& instance{ Entity::Get<impl::TooltipData>() };

	milliseconds fade_in_duration{ instance.fade_in_duration };
	Ease fade_in_ease{ instance.fade_in_ease };

	bool fade_in_force{ true };

	const auto fade_in = [=](auto& entity) {
		FadeIn(entity, fade_in_duration, fade_in_ease, fade_in_force, true);
	};

	auto text_entity{ FindPart(impl::TooltipPart::Text) };
	PTGN_ASSERT(text_entity, "Tooltip has no text part");
	Text text{ text_entity };
	fade_in(text);

	if (auto background_entity{ FindPart(impl::TooltipPart::Background) }) {
		Sprite background{ background_entity };
		fade_in(background);
	}
}

void Tooltip::Hide() {
	const auto& instance{ Entity::Get<impl::TooltipData>() };

	milliseconds fade_out_duration{ instance.fade_out_duration };
	Ease fade_out_ease{ instance.fade_out_ease };

	bool fade_out_force{ true };

	const auto fade_out = [=](auto& entity) {
		FadeOut(entity, fade_out_duration, fade_out_ease, fade_out_force);
	};

	auto text_entity{ FindPart(impl::TooltipPart::Text) };
	PTGN_ASSERT(text_entity, "Tooltip has no text part");
	Text text{ text_entity };
	fade_out(text);

	if (auto background_entity{ FindPart(impl::TooltipPart::Background) }) {
		Sprite background{ background_entity };
		fade_out(background);
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

Entity Tooltip::FindPart(impl::TooltipPart part) const {
	if (!HasChildren(*this)) {
		return {};
	}

	auto children{ GetChildren(*this) };
	auto it{ std::ranges::find_if(children, [part](Entity entity) {
		return IsTooltipPart(entity, part);
	}) };

	return it != children.end() ? *it : Entity{};
}

namespace impl {

Tooltip TooltipSystem::GetTooltip(Entity entity) {
	const auto& hover{ entity.Get<TooltipHoverData>() };
	auto tooltip{ Tooltip::Get(entity.GetScene(), hover.name) };

	PTGN_ASSERT(
		tooltip.has_value(), "Tooltip with the name: ", hover.name,
		" does not exist in the scene"
	);

	return tooltip.value();
}

void TooltipSystem::Prepare(Scene& scene) {
	for (auto [entity, hover] : scene.EntitiesWith<TooltipHoverData>()) {
		entity.TryAdd<Transform>();

		if (!entity.Has<Interactive>()) {
			SetInteractive(entity);
		}

		if (entity.HasAny<ptgn::Texture, ptgn::TextureKey>() && !HasInteractiveShape(entity)) {
			auto rect{ scene.CreateEntity() };
			rect.Add<ptgn::Tag>("Tooltip Interactive Rect");
			rect.Add<ptgn::Rect>(*GetTextureSize(entity));
			AddInteractiveShape(entity, rect);
		}

		PTGN_ASSERT(
			HasInteractiveShape(entity),
			"TooltipHoverData requires a Rect, Circle, or interactive child shape"
		);

		auto tooltip{ Tooltip::Get(scene, hover.name) };
		PTGN_ASSERT(
			tooltip.has_value(), "Tooltip with the name: ", hover.name,
			" does not exist in the scene"
		);

		PTGN_ASSERT(
			!HasParent(tooltip.value()) || GetParent(tooltip.value()) == entity,
			"Tooltip '", hover.name, "' is already attached to another entity"
		);

		if (!HasParent(tooltip.value())) {
			SetParent(tooltip.value(), entity);
			ptgn::IgnoreParentRotation(tooltip.value());
			ptgn::IgnoreParentScale(tooltip.value());
		}
	}
}

void TooltipSystem::OnEvent(Entity entity, Event event) {
	if (!entity || !entity.Has<TooltipHoverData>()) {
		return;
	}

	event.Dispatch<event::MouseEnter>([entity]() { OnMouseEnter(entity); });
	event.Dispatch<event::MouseLeave>([entity]() { OnMouseLeave(entity); });
}

void TooltipSystem::OnMouseEnter(Entity entity) {
	const auto& hover{ entity.Get<TooltipHoverData>() };
	GetTooltip(entity).Show(hover.offset);
}

void TooltipSystem::OnMouseLeave(Entity entity) {
	GetTooltip(entity).Hide();
}

} // namespace impl

Tooltip CreateTooltip(
	Scene& scene, std::string_view tooltip_name, const TooltipProperties& tooltip_properties
) {
	PTGN_ASSERT(
		!Tooltip::Get(scene, tooltip_name).has_value(), "Tooltip with the name: ", tooltip_name,
		" already exists in the manager"
	);

	Tooltip tooltip{ scene.CreateEntity() };
	tooltip.Add<Tag>("Tooltip");

	auto& instance{ tooltip.Add<impl::TooltipData>() };

	instance.hash			   = Hash(tooltip_name);
	instance.fade_in_duration  = tooltip_properties.fade_in_duration;
	instance.fade_out_duration = tooltip_properties.fade_out_duration;
	instance.fade_in_ease	   = tooltip_properties.fade_in_ease;
	instance.fade_out_ease	   = tooltip_properties.fade_out_ease;

	if (tooltip_properties.texture.has_value()) {
		auto background{
			CreateSprite(scene, {}, tooltip_properties.texture.value(), Origin::Center)
		};
		background.Add<Tag>("Tooltip Sprite");
		background.Add<impl::TooltipBackgroundPart>();
		background.Add<Tint>(color::Transparent);
		SetParent(background, tooltip);
	}

	Text text{
		CreateText(scene).Content(tooltip_properties.content).Color(tooltip_properties.text_color)
	};
	text.Add<Tag>("Tooltip Text");
	text.Add<impl::TooltipTextPart>();
	text.Add<Tint>(color::Transparent);
	SetParent(text, tooltip);

	return tooltip;
}

void ShowTooltipOnHover(Entity entity, std::string_view tooltip_name, V2_float tooltip_offset) {
	auto& hover{ entity.TryAdd<impl::TooltipHoverData>() };
	hover.name = tooltip_name;
	hover.offset = tooltip_offset;

	if (!entity.Has<impl::Interactive>()) {
		SetInteractive(entity);
	}
}

Tooltip AddTooltipOnHover(
	Entity entity, std::string_view tooltip_name, const TooltipProperties& tooltip_properties,
	V2_float tooltip_offset
) {
	SetInteractive(entity);

	if (entity.HasAny<ptgn::Texture, ptgn::TextureKey>() && !HasInteractiveShape(entity)) {
		auto rect{ entity.GetScene().CreateEntity() };
		rect.Add<ptgn::Tag>("Tooltip Interactive Rect");
		V2_float size{ *GetTextureSize(entity) };
		rect.Add<Rect>(size);
		AddInteractiveShape(entity, rect);
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
