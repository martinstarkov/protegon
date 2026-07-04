#include "runtime/ui/button.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/span.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/toggle_button.h"

namespace ptgn {

namespace {

constexpr Color kDefaultIdleButtonBackgroundColor{ color::DarkGray };
constexpr Color kDefaultHoverButtonBackgroundColor{ color::Gray };
constexpr Color kDefaultPressButtonBackgroundColor{ 32, 32, 32, 255 };

constexpr Color kDefaultIdleButtonBorderColor{ color::Gray };
constexpr Color kDefaultHoverButtonBorderColor{ color::LightGray };
constexpr Color kDefaultPressButtonBorderColor{ color::Gray };

constexpr float kDefaultButtonBorderWidth{ 2.0f };

ButtonDesc MakeButtonDesc(V2_float size, const ButtonConfig& config) {
	ButtonDesc desc{
		.size	= size,
		.origin = config.origin,
	};

	auto add_background = [&](ButtonVisualState state, const std::optional<Color>& color) {
		if (!color.has_value()) {
			return;
		}

		std::optional<std::variant<V2_float, float>> size;

		if (config.background_size.has_value()) {
			size = config.background_size.value();
		}

		desc.shapes.emplace_back(
			ButtonShapeConfig{
				.part		= ButtonPart::Background,
				.state		= state,
				.size		= size,
				.color		= color,
				.fill_style = Solid{},
			}
		);
	};

	add_background(ButtonVisualState::Idle, config.background_color);
	add_background(ButtonVisualState::Hover, config.background_color_hover);
	add_background(ButtonVisualState::Press, config.background_color_press);

	auto add_sprite = [&](ButtonVisualState state, const std::optional<std::string>& texture,
						  const std::optional<Color>& tint) {
		if (!texture.has_value()) {
			return;
		}

		desc.sprites.emplace_back(
			ButtonSpriteConfig{
				.state	 = state,
				.texture = texture.value(),
				.tint	 = tint,
			}
		);
	};

	add_sprite(ButtonVisualState::Idle, config.texture, config.texture_tint);

	if (config.texture_hover.has_value()) {
		add_sprite(ButtonVisualState::Hover, config.texture_hover, config.texture_tint_hover);
	} else if (config.texture.has_value() && config.texture_tint_hover.has_value()) {
		add_sprite(ButtonVisualState::Hover, config.texture, config.texture_tint_hover);
	}

	if (config.texture_press.has_value()) {
		add_sprite(ButtonVisualState::Press, config.texture_press, config.texture_tint_press);
	} else if (config.texture_tint_press.has_value()) {
		auto press_texture{ config.texture_hover.has_value() ? config.texture_hover
															 : config.texture };

		add_sprite(ButtonVisualState::Press, press_texture, config.texture_tint_press);
	}

	auto add_text = [&](ButtonVisualState state, Color color) {
		desc.texts.emplace_back(
			ButtonTextConfig{
				.state		   = state,
				.content	   = config.content.value(),
				.font		   = config.font,
				.font_size	   = config.font_size,
				.color		   = color,
				.box		   = config.text_box,
				.origin		   = config.text_origin,
				.anchor		   = config.text_anchor,
				.transform	   = config.text_transform,
				.outline_width = config.text_outline_width,
				.outline_color = config.text_outline_color,
				.auto_box	   = config.text_auto_box,
				.padding	   = config.text_padding,
			}
		);
	};

	if (config.content.has_value()) {
		add_text(ButtonVisualState::Idle, config.text_color.value_or(kDefaultButtonTextColor));

		if (config.text_color_hover.has_value()) {
			add_text(ButtonVisualState::Hover, config.text_color_hover.value());
		}

		if (config.text_color_press.has_value()) {
			add_text(ButtonVisualState::Press, config.text_color_press.value());
		}
	}

	desc.sounds.hover = config.sound_hover;
	desc.sounds.press = config.sound_press;

	desc.move  = config.move;
	desc.scale = config.scale;

	return desc;
}

constexpr Color GetDefaultBackgroundColor(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;
		case Base:			return kDefaultIdleButtonBackgroundColor;
		case Idle:			return kDefaultIdleButtonBackgroundColor;
		case Hover:			return kDefaultHoverButtonBackgroundColor;
		case Press:			return kDefaultPressButtonBackgroundColor;
		case Disabled:		return kDefaultIdleButtonBackgroundColor;
		case DisabledHover: return kDefaultHoverButtonBackgroundColor;
		case DisabledPress: return kDefaultPressButtonBackgroundColor;
		case Toggled:		return kDefaultIdleButtonBackgroundColor;
		case ToggledHover:	return kDefaultHoverButtonBackgroundColor;
		case ToggledPress:	return kDefaultPressButtonBackgroundColor;
		default:			PTGN_ERROR("Unknown ButtonVisualState: ", std::to_underlying(state));
	}
}

constexpr Color GetDefaultBorderColor(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;
		case Base:			return kDefaultIdleButtonBorderColor;
		case Idle:			return kDefaultIdleButtonBorderColor;
		case Hover:			return kDefaultHoverButtonBorderColor;
		case Press:			return kDefaultPressButtonBorderColor;
		case Disabled:		return kDefaultIdleButtonBorderColor;
		case DisabledHover: return kDefaultHoverButtonBorderColor;
		case DisabledPress: return kDefaultPressButtonBorderColor;
		case Toggled:		return kDefaultIdleButtonBorderColor;
		case ToggledHover:	return kDefaultHoverButtonBorderColor;
		case ToggledPress:	return kDefaultPressButtonBorderColor;
		default:			PTGN_ERROR("Unknown ButtonVisualState: ", std::to_underlying(state));
	}
}

std::vector<ButtonVisualState> GetVisualStateFallbacks(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;
		case Base:			return { Base };
		case Idle:			return { Idle, Base };
		case Hover:			return { Hover, Idle, Base };
		case Press:			return { Press, Hover, Idle, Base };
		case Disabled:		return { Disabled, Idle, Base };
		case DisabledHover: return { DisabledHover, Disabled, Hover, Idle, Base };
		case DisabledPress:
			return { DisabledPress, DisabledHover, Disabled, Press, Hover, Idle, Base };
		case Toggled:	   return { Toggled, Idle, Base };
		case ToggledHover: return { ToggledHover, Toggled, Hover, Idle, Base };
		case ToggledPress: return { ToggledPress, ToggledHover, Toggled, Press, Hover, Idle, Base };
		default:		   PTGN_ERROR("Unknown ButtonVisualState: ", std::to_underlying(state));
	}
}

ButtonVisualState GetPressVisualState(Button button) {
	using enum ButtonVisualState;

	if (!button.IsEnabled(false)) {
		return DisabledPress;
	}

	if (button.Has<impl::ToggleButtonData>() && ToggleButton{ button }.IsToggled()) {
		return ToggledPress;
	}

	return Press;
}

constexpr ButtonVisualState DisabledStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;
		case Idle:	return ButtonVisualState::Disabled;
		case Hover: return ButtonVisualState::DisabledHover;
		case Press: return ButtonVisualState::DisabledPress;
		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

constexpr ButtonVisualState ToggledStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;
		case Idle:	return ButtonVisualState::Toggled;
		case Hover: return ButtonVisualState::ToggledHover;
		case Press: return ButtonVisualState::ToggledPress;
		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

constexpr ButtonVisualState NormalStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;
		case Idle:	return ButtonVisualState::Idle;
		case Hover: return ButtonVisualState::Hover;
		case Press: return ButtonVisualState::Press;
		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

bool IsButtonChild(Entity entity) {
	return entity.Has<impl::ButtonChild>();
}

std::optional<Entity> FindButtonPart(Button button, ButtonPart part) {
	if (!HasChildren(button)) {
		return std::nullopt;
	}

	auto children{ GetChildren(button) };
	auto it{ std::ranges::find_if(children, [part](Entity entity) {
		return IsButtonChild(entity) && entity.Get<impl::ButtonChild>().part == part;
	}) };

	return it != children.end() ? std::optional<Entity>{ *it } : std::nullopt;
}

std::vector<Entity> FindButtonParts(Button button, std::optional<ButtonPart> part = {}) {
	if (!HasChildren(button)) {
		return {};
	}

	return GetChildren(button) | std::views::filter([part](Entity child) {
			   if (!IsButtonChild(child)) {
				   return false;
			   }
			   return !part.has_value() || child.Get<impl::ButtonChild>().part == part.value();
		   }) |
		   std::ranges::to<std::vector>();
}

Rect GetButtonLocalRect(Button button) {
	if (!button.HasAny<Rect, Circle>()) {
		return {};
	}

	return { std::visit(
				 []<typename T>(const T& value) {
					 if constexpr (std::same_as<T, V2_float>) {
						 return value;
					 } else if constexpr (std::same_as<T, float>) {
						 return V2_float{ value * 2.0f };
					 } else {
						 static_assert(false, "Incomplete visitor");
					 }
				 },
				 button.GetSize()
			 ),
			 GetDrawOrigin(button) };
}

Rect GetButtonTextContentRect(Button button, Padding padding) {
	auto rect{ GetButtonLocalRect(button) };

	rect.min += padding.GetLeftTop();
	rect.max -= padding.GetRightBottom();

	return rect;
}

Rect GetButtonTextAutoBox(Rect content_rect, V2_float text_origin_position, Origin text_origin) {
	if (text_origin_position.x < content_rect.min.x ||
		text_origin_position.x > content_rect.max.x ||
		text_origin_position.y < content_rect.min.y ||
		text_origin_position.y > content_rect.max.y) {
		return {};
	}

	auto origin_alignment{ GetAlignment(text_origin) };

	PTGN_ASSERT(origin_alignment.horizontal.has_value());
	PTGN_ASSERT(origin_alignment.vertical.has_value());

	V2_float size;

	switch (origin_alignment.horizontal.value()) {
		using enum HorizontalAlign;

		case Left: size.x = content_rect.max.x - text_origin_position.x; break;

		case Center:
			size.x = 2.0f * std::min(
								text_origin_position.x - content_rect.min.x,
								content_rect.max.x - text_origin_position.x
							);
			break;

		case Right:	  size.x = text_origin_position.x - content_rect.min.x; break;

		case Justify: PTGN_ERROR("A text Origin cannot resolve to justified alignment");

		default:
			PTGN_ERROR(
				"Unknown HorizontalAlign: ", std::to_underlying(origin_alignment.horizontal.value())
			);
	}

	switch (origin_alignment.vertical.value()) {
		using enum VerticalAlign;

		case Top: size.y = content_rect.max.y - text_origin_position.y; break;

		case Center:
			size.y = 2.0f * std::min(
								text_origin_position.y - content_rect.min.y,
								content_rect.max.y - text_origin_position.y
							);
			break;

		case Bottom: size.y = text_origin_position.y - content_rect.min.y; break;

		default:
			PTGN_ERROR(
				"Unknown VerticalAlign: ", std::to_underlying(origin_alignment.vertical.value())
			);
	}

	if (!size.IsPositive()) {
		return {};
	}

	// Places the selected text origin at local zero.
	return Rect{ size, text_origin };
}

Entity EnsureButtonPart(Button button, ButtonPart part) {
	auto parts{ FindButtonParts(button, part) };

	PTGN_ASSERT(
		parts.size() <= 1, "Button has multiple consolidated ", magic_enum::enum_name(part),
		" parts"
	);

	if (!parts.empty()) {
		return parts.front();
	}

	Entity entity;

	switch (part) {
		case ButtonPart::Background: [[fallthrough]];
		case ButtonPart::Border:	 entity = button.GetScene().CreateEntity(); break;
		case ButtonPart::Sprite:	 entity = CreateSprite(button.GetScene()); break;
		case ButtonPart::Text:		 {
			Text text{ CreateText(button.GetScene()) };
			text.ClearAlignment();
			text.Remove<Origin>();
			entity = text;
			break;
		}
		default: PTGN_ERROR("Unknown ButtonPart: ", std::to_underlying(part));
	}

	PTGN_DEFAULT_NAME(entity, "Button " + std::string{ magic_enum::enum_name(part) });
	entity.Add<impl::ButtonChild>(part);
	SetParent(entity, button);
	SetVisible(entity, true);

	switch (part) {
		case ButtonPart::Background: [[fallthrough]];
		case ButtonPart::Border:	 entity.Add<impl::ButtonShapeVisuals>(); break;
		case ButtonPart::Sprite:	 entity.Add<impl::ButtonSpriteVisuals>(); break;
		case ButtonPart::Text:		 entity.Add<impl::ButtonTextVisuals>(); break;
		default:					 PTGN_ERROR("Unknown ButtonPart: ", std::to_underlying(part));
	}

	return entity;
}

template <typename TVisual>
bool HasResolvedState(
	const std::array<TVisual, impl::kButtonVisualStateCount>& states, ButtonVisualState state
) {
	return std::ranges::any_of(
		GetVisualStateFallbacks(state), [&states](ButtonVisualState fallback) {
			return states[std::to_underlying(fallback)].defined;
		}
	);
}

template <typename TVisual, typename T>
const T* ResolveProperty(
	const std::array<TVisual, impl::kButtonVisualStateCount>& states, ButtonVisualState state,
	const std::optional<T> TVisual::* member, ButtonVisualState* source_state = nullptr
) {
	for (auto fallback : GetVisualStateFallbacks(state)) {
		const auto& visual{ states[std::to_underlying(fallback)] };
		if (!visual.defined) {
			continue;
		}

		const auto& value{ visual.*member };
		if (!value.has_value()) {
			continue;
		}

		if (source_state) {
			*source_state = fallback;
		}
		return &value.value();
	}

	return nullptr;
}

constexpr impl::ButtonDirty DirtyForPart(ButtonPart part) {
	switch (part) {
		case ButtonPart::Background: return impl::ButtonDirty::Background;
		case ButtonPart::Border:	 return impl::ButtonDirty::Border;
		case ButtonPart::Sprite:	 return impl::ButtonDirty::Sprite;
		case ButtonPart::Text:		 return impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout;
		default:					 PTGN_ERROR("Unknown ButtonPart: ", std::to_underlying(part));
	}
}

struct ResolvedShapeVisual {
	bool visible{ false };
	std::variant<V2_float, float> size;
	Origin origin{ Origin::Center };
	Origin anchor{ Origin::Center };
	Color color{ color::White };
	FillStyle fill_style{ Solid{} };
};

ResolvedShapeVisual ResolveShapeVisual(Button button, Entity entity, ButtonPart part) {
	const auto& visuals{ entity.Get<impl::ButtonShapeVisuals>() };
	auto state{ button.GetVisualState() };

	ResolvedShapeVisual result{
		.visible	= HasResolvedState(visuals.states, state),
		.size		= button.HasAny<Rect, Circle>() ? button.GetSize() : V2_float{},
		.origin		= GetDrawOrigin(button),
		.anchor		= GetDrawOrigin(button),
		.color		= part == ButtonPart::Background ? GetDefaultBackgroundColor(state)
													 : GetDefaultBorderColor(state),
		.fill_style = part == ButtonPart::Background ? FillStyle{ Solid{} }
													 : FillStyle{ kDefaultButtonBorderWidth },
	};

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonShapeVisual::size) }) {
		result.size = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonShapeVisual::origin) }) {
		result.origin = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonShapeVisual::anchor) }) {
		result.anchor = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonShapeVisual::color) }) {
		result.color = *value;
	}
	if (auto value{
			ResolveProperty(visuals.states, state, &impl::ButtonShapeVisual::fill_style) }) {
		result.fill_style = *value;
	}

	return result;
}

struct ResolvedSpriteVisual {
	bool visible{ false };
	std::string texture;
	Origin origin{ Origin::Center };
	Origin anchor{ Origin::Center };
	Transform transform;
	std::optional<V2_float> size;
	Color tint{ color::White };
	const AnimationConfig* animation{ nullptr };
	ButtonAnimationOptions animation_options;
	std::optional<ButtonVisualState> animation_state;
};

ResolvedSpriteVisual ResolveSpriteVisual(
	Button button, const impl::ButtonSpriteVisuals& visuals, ButtonVisualState state
) {
	ResolvedSpriteVisual result{
		.visible = HasResolvedState(visuals.states, state),
		.origin	 = GetDrawOrigin(button),
		.anchor	 = GetDrawOrigin(button),
	};

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::texture) }) {
		result.texture = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::origin) }) {
		result.origin = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::anchor) }) {
		result.anchor = *value;
	}
	if (auto value{
			ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::transform) }) {
		result.transform = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::size) }) {
		result.size = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonSpriteVisual::tint) }) {
		result.tint = *value;
	}

	ButtonVisualState animation_state;
	result.animation = ResolveProperty(
		visuals.states, state, &impl::ButtonSpriteVisual::animation, &animation_state
	);
	if (result.animation) {
		result.animation_state = animation_state;
		const auto& animation_visual{ visuals.states[std::to_underlying(animation_state)] };
		if (animation_visual.animation_options.has_value()) {
			result.animation_options = animation_visual.animation_options.value();
		}
	}

	return result;
}

struct ResolvedTextVisual {
	bool visible{ false };
	StyledText styled_text;
	TextBox box;
	Origin origin{ Origin::Center };
	Origin anchor{ Origin::Center };
	Transform transform;
	bool auto_box{ true };
	Padding padding;
};

ResolvedTextVisual ResolveTextVisual(
	Button button, const impl::ButtonTextVisuals& visuals, ButtonVisualState state
) {
	ResolvedTextVisual result{
		.visible = HasResolvedState(visuals.states, state),
		.box	 = { .style = { .alignment = { .horizontal = std::nullopt,
											   .vertical   = std::nullopt } } },
		// Anchor always defaults to center, independent of button origin.
		.anchor = Origin::Center,
	};

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::anchor) }) {
		result.anchor = *value;
	}

	// Origin follows anchor unless an origin exists in the fallback chain.
	result.origin = result.anchor;

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::origin) }) {
		result.origin = *value;
	}

	if (auto value{
			ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::styled_text) }) {
		result.styled_text = *value;
	}

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::box) }) {
		result.box = *value;
	}

	if (!result.box.style.alignment.horizontal.has_value()) {
		result.box.style.alignment.horizontal = GetAlignment(result.origin).horizontal;
	}

	if (!result.box.style.alignment.vertical.has_value()) {
		result.box.style.alignment.vertical = GetAlignment(result.origin).vertical;
	}

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::transform) }) {
		result.transform = *value;
	}

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::auto_box) }) {
		result.auto_box = *value;
	}

	if (auto value{ ResolveProperty(visuals.states, state, &impl::ButtonTextVisual::padding) }) {
		result.padding = *value;
	}

	return result;
}

void ResetButtonAnimation(Animation animation) {
	if (!animation.Has<impl::AnimationData>()) {
		return;
	}

	auto animation_part{ animation.TryGet<impl::ButtonAnimationPart>() };
	auto static_frame{ animation_part ? animation_part->options.static_frame : 0uz };

	animation.Reset();
	animation.SetCurrentFrame(static_frame);
}

struct ResolvedButtonAnimation {
	Entity entity;
	ButtonVisualState state{ ButtonVisualState::Base };
	ButtonAnimationOptions options;
};

std::optional<ResolvedButtonAnimation> TryAnimationForVisualState(
	Button button, ButtonVisualState state
) {
	auto entity{ FindButtonPart(button, ButtonPart::Sprite) };
	if (!entity.has_value()) {
		return std::nullopt;
	}

	const auto& visuals{ entity->Get<impl::ButtonSpriteVisuals>() };
	auto resolved{ ResolveSpriteVisual(button, visuals, state) };
	if (!resolved.animation || !resolved.animation_state.has_value()) {
		return std::nullopt;
	}

	return ResolvedButtonAnimation{
		.entity	 = entity.value(),
		.state	 = resolved.animation_state.value(),
		.options = resolved.animation_options,
	};
}

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Button button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::AnimationComplete>([this]() {
		if (!button || !IsButtonChild(entity) ||
			entity.Get<ButtonChild>().part != ButtonPart::Sprite) {
			return;
		}

		auto& visuals{ entity.Get<ButtonSpriteVisuals>() };

		if (auto visual_override{ button.TryGet<ButtonVisualOverride>() }) {
			auto animation{ TryAnimationForVisualState(button, visual_override->state) };
			if (!animation.has_value() || animation->entity != entity) {
				return;
			}

			button.Remove<ButtonVisualOverride>();
			button.MarkDirty(ButtonDirty::All);
			button.RefreshDirty();
			return;
		}

		if (visuals.transient_animation) {
			visuals.transient_animation = false;
			button.MarkDirty(ButtonDirty::Sprite);
			button.RefreshDirty();
		}
	});
}

void ButtonScript::OnEvent(Event event) {
	using namespace ptgn::event;

	event.Dispatch<MouseMoveOver>(&ButtonScript::OnMouseMoveOver, this);
	event.Dispatch<MouseMoveOut>(&ButtonScript::OnMouseMoveOut, this);
	event.Dispatch<MousePressedOver>(&ButtonScript::OnMousePressedOver, this);
	event.Dispatch<MousePressedOut>(&ButtonScript::OnMousePressedOut, this);
	event.Dispatch<MouseReleasedOver>(&ButtonScript::OnMouseReleasedOver, this);
	event.Dispatch<MouseReleasedOut>(&ButtonScript::OnMouseReleasedOut, this);
}

void ButtonScript::OnMouseMoveOver() const {
	Button button{ entity };

	if (!button.IsEnabled(true)) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == IdleUp) {
		button.SetState(Hover);
		button.StartHover();
	} else if (state == IdleDown) {
		button.SetState(HoverPressed);
		button.StartHover();
	} else if (state == HeldOutside) {
		button.SetState(Pressed);
		return;
	}

	button.ContinueHover();
}

void ButtonScript::OnMouseMoveOut() const {
	Button button{ entity };

	if (!button.IsEnabled(true)) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == Hover) {
		button.SetState(IdleUp);
		button.StopHover();
	} else if (state == Pressed) {
		button.SetState(HeldOutside);
		button.StopHover();
	} else if (state == HoverPressed) {
		button.SetState(IdleDown);
		button.StopHover();
	}
}

void ButtonScript::OnMousePressedOver(Mouse mouse) const {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	if (button.GetInternalState() == InternalButtonState::Hover) {
		button.SetState(InternalButtonState::Pressed);
	}
}

void ButtonScript::OnMousePressedOut(Mouse mouse) const {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	if (button.GetInternalState() == InternalButtonState::IdleUp) {
		button.SetState(InternalButtonState::IdleDown);
	}
}

void ButtonScript::OnMouseReleasedOver(Mouse mouse) const {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == Pressed) {
		button.SetState(Hover);
		button.Press();
	} else if (state == HoverPressed) {
		button.SetState(Hover);
	}
}

void ButtonScript::OnMouseReleasedOut(Mouse mouse) const {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == IdleDown || state == HeldOutside) {
		button.SetState(IdleUp);
	}
}

void UpdateButtons(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<impl::ButtonData>()) {
		Button{ entity }.RefreshDirty();
	}
}

} // namespace impl

Button::Button(Entity entity) : Entity{ entity } {}

bool Button::IsEnabled(bool check_for_hover_enabled) const {
	auto enabled{ TryGet<impl::ButtonEnabled>() };
	if (!enabled) {
		return false;
	}

	return check_for_hover_enabled ? enabled->hover : enabled->press;
}

ButtonState Button::GetState() const {
	auto state{ GetInternalState() };

	using enum impl::InternalButtonState;

	if (state == Hover || state == HoverPressed) {
		return ButtonState::Hover;
	}

	if (state == Pressed || state == HeldOutside) {
		return ButtonState::Press;
	}

	return ButtonState::Idle;
}

ButtonVisualState Button::GetVisualState() const {
	if (!IsEnabled(false)) {
		return DisabledStateFrom(GetState());
	}

	if (auto visual_override{ TryGet<impl::ButtonVisualOverride>() }) {
		return visual_override->state;
	}

	if (Has<impl::ToggleButtonData>() && ToggleButton{ *this }.IsToggled()) {
		return ToggledStateFrom(GetState());
	}

	return NormalStateFrom(GetState());
}

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::ButtonData>().state;
}

std::variant<V2_float, float> Button::GetSize() const {
	if (auto rect{ TryGet<Rect>() }) {
		return rect->GetSize();
	}

	if (auto circle{ TryGet<Circle>() }) {
		return circle->radius;
	}

	PTGN_ERROR("Button has no shape. Use Button::Size() to set a shape.");
}

Button& Button::Enable(bool enable_hover, bool reset_state) {
	return SetEnabled(true, enable_hover, reset_state);
}

Button& Button::Disable(bool disable_hover, bool reset_state) {
	return SetEnabled(false, !disable_hover, reset_state);
}

Button& Button::SetEnabled(bool enable_activation, bool enable_hover, bool reset_state) {
	Add<impl::ButtonEnabled>(enable_activation, enable_hover);

	if (reset_state) {
		Get<impl::ButtonData>().state = impl::InternalButtonState::IdleUp;
	}

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();
	return *this;
}

Button& Button::Press() {
	if (!IsEnabled(false)) {
		return *this;
	}

	if (auto visual_override{ TryGet<impl::ButtonVisualOverride>() };
		visual_override && visual_override->block_press) {
		return *this;
	}

	auto press_visual_state{ GetPressVisualState(*this) };
	auto resolved_animation{ TryAnimationForVisualState(*this, press_visual_state) };

	if (resolved_animation.has_value()) {
		const auto& options{ resolved_animation->options };

		if (options.lock_visual_state) {
			auto& visual_override{ TryAdd<impl::ButtonVisualOverride>() };
			visual_override.state		= press_visual_state;
			visual_override.block_press = options.block_press;

			MarkDirty(impl::ButtonDirty::All);
			RefreshDirty();
		} else {
			ApplySpriteVisual(press_visual_state, true);
		}

		if (auto sprite{ FindButtonPart(*this, ButtonPart::Sprite) };
			sprite && sprite->Has<impl::AnimationData>()) {
			ptgn::Animation animation{ sprite.value() };
			ResetButtonAnimation(animation);
			animation.Start(true);
		}
	} else {
		PlayAnimation(ButtonState::Press);
	}

	PlaySound(ButtonState::Press);
	PushEvent<event::ButtonPress>(*this, *this);

	return *this;
}

Button& Button::StartHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PlaySound(ButtonState::Hover);
	PlayAnimation(ButtonState::Hover);
	PushEvent<event::ButtonHoverStart>(*this, *this);
	return *this;
}

Button& Button::ContinueHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PushEvent<event::ButtonHover>(*this, *this);
	return *this;
}

Button& Button::StopHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PlaySound(ButtonState::Idle);
	PlayAnimation(ButtonState::Idle);
	PushEvent<event::ButtonHoverStop>(*this, *this);
	return *this;
}

Button& Button::Size(V2_float size) {
	Remove<Circle>();
	Add<Rect>(size);
	MarkDirty(
		impl::ButtonDirty::Background | impl::ButtonDirty::Border | impl::ButtonDirty::Sprite |
		impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout
	);
	return *this;
}

Button& Button::Size(float radius) {
	Remove<Rect>();
	Add<Circle>(radius);
	MarkDirty(
		impl::ButtonDirty::Background | impl::ButtonDirty::Border | impl::ButtonDirty::Sprite |
		impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout
	);
	return *this;
}

Button& Button::RemovePart(ButtonPart part, ButtonVisualState state) {
	if (part == ButtonPart::Text) {
		CommitTextEdit();
	}
	if (part == ButtonPart::Sprite) {
		Remove<impl::ButtonVisualOverride>();
	}

	auto entity{ FindButtonPart(*this, part) };
	if (!entity.has_value()) {
		return *this;
	}

	bool any_defined{ false };

	auto has_any_defined = [](const auto& states) {
		return std::ranges::any_of(states, [](const auto& visual) { return visual.defined; });
	};

	switch (part) {
		case ButtonPart::Background:
		case ButtonPart::Border:	 {
			auto& visuals{ entity->Get<impl::ButtonShapeVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);
			break;
		}
		case ButtonPart::Sprite: {
			auto& visuals{ entity->Get<impl::ButtonSpriteVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			visuals.applied_animation_state.reset();
			any_defined = has_any_defined(visuals.states);
			break;
		}
		case ButtonPart::Text: {
			auto& visuals{ entity->Get<impl::ButtonTextVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			visuals.editing.reset();
			any_defined = has_any_defined(visuals.states);
			break;
		}
		default: PTGN_ERROR("Unknown ButtonPart: ", std::to_underlying(part));
	}

	if (!any_defined) {
		entity->Destroy();
	} else {
		MarkDirty(DirtyForPart(part));
	}

	return *this;
}

Button& Button::RemoveParts(ButtonPart part) {
	if (part == ButtonPart::Text) {
		CommitTextEdit();
	}
	if (part == ButtonPart::Sprite) {
		Remove<impl::ButtonVisualOverride>();
	}

	if (auto entity{ FindButtonPart(*this, part) }) {
		entity->Destroy();
	}
	return *this;
}

Text Button::GetText(ButtonVisualState state) {
	CommitTextEdit();

	auto entity{ EnsureButtonPart(*this, ButtonPart::Text) };

	auto& visuals{ entity.Get<impl::ButtonTextVisuals>() };

	auto& visual{ visuals.states[std::to_underlying(state)] };

	auto resolved{ ResolveTextVisual(*this, visuals, state) };

	visual.defined = true;

	ptgn::Text text{ entity };

	text.Content(std::move(resolved.styled_text));
	text.Box(resolved.box);

	SetTransform(text, resolved.transform);
	SetDrawOrigin(text, resolved.origin);

	visuals.editing = impl::ButtonTextEditSnapshot{
		.state	   = state,
		.box	   = text.GetTextBox(),
		.origin	   = GetDrawOrigin(text),
		.transform = text.Get<Transform>(),
	};

	return text;
}

Text Button::Text(ButtonVisualState state) {
	return GetText(state);
}

Text Button::Text(std::string_view content, Color color, float font_size, ButtonVisualState state) {
	auto text{ GetText(state) };

	text.Clear().Content(content).Color(color).Size(font_size);

	return text;
}

Text Button::Text(StyledText styled_text, ButtonVisualState state) {
	auto text{ GetText(state) };

	text.Clear().Content(std::move(styled_text));

	return text;
}

Button& Button::TextOrigin(Origin origin, ButtonVisualState state) {
	CommitTextEdit();
	auto entity{ EnsureButtonPart(*this, ButtonPart::Text) };
	auto& visual{ entity.Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };
	visual.defined = true;
	visual.origin  = origin;
	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
	return *this;
}

Button& Button::ClearTextOrigin(ButtonVisualState state) {
	CommitTextEdit();

	auto entity{ FindButtonPart(*this, ButtonPart::Text) };

	if (!entity.has_value()) {
		return *this;
	}

	auto& visual{ entity->Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };

	visual.origin.reset();

	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);

	return *this;
}

Button& Button::ClearTextAnchor(ButtonVisualState state) {
	CommitTextEdit();

	auto entity{ FindButtonPart(*this, ButtonPart::Text) };

	if (!entity.has_value()) {
		return *this;
	}

	auto& visual{ entity->Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };

	visual.anchor.reset();

	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);

	return *this;
}

Button& Button::TextAnchor(Origin anchor, ButtonVisualState state) {
	CommitTextEdit();
	auto entity{ EnsureButtonPart(*this, ButtonPart::Text) };
	auto& visual{ entity.Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };
	visual.defined = true;
	visual.anchor  = anchor;
	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
	return *this;
}

Button& Button::TextAutoBox(bool enabled, ButtonVisualState state) {
	CommitTextEdit();
	auto entity{ EnsureButtonPart(*this, ButtonPart::Text) };
	auto& visual{ entity.Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };
	visual.defined	= true;
	visual.auto_box = enabled;
	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
	return *this;
}

Button& Button::TextPadding(Padding padding, ButtonVisualState state) {
	CommitTextEdit();
	auto entity{ EnsureButtonPart(*this, ButtonPart::Text) };
	auto& visual{ entity.Get<impl::ButtonTextVisuals>().states[std::to_underlying(state)] };
	visual.defined = true;
	visual.padding = padding;
	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
	return *this;
}

Button& Button::RemoveText() {
	return RemoveParts(ButtonPart::Text);
}

Button& Button::RemoveText(ButtonVisualState state) {
	return RemovePart(ButtonPart::Text, state);
}

Button& Button::Sprite(
	std::string_view texture_key, std::optional<Origin> origin, ButtonVisualState state
) {
	auto entity{ EnsureButtonPart(*this, ButtonPart::Sprite) };
	auto& visuals{ entity.Get<impl::ButtonSpriteVisuals>() };
	auto& visual{ visuals.states[std::to_underlying(state)] };

	visual.defined = true;
	visual.texture = std::string{ texture_key };
	visual.origin  = origin;

	MarkDirty(impl::ButtonDirty::Sprite);
	return *this;
}

Button& Button::Sprites(
	std::optional<std::string_view> idle_texture_key,
	std::optional<std::string_view> hover_texture_key,
	std::optional<std::string_view> press_texture_key
) {
	if (idle_texture_key.has_value()) {
		Sprite(idle_texture_key.value(), std::nullopt, ButtonVisualState::Idle);
	}
	if (hover_texture_key.has_value()) {
		Sprite(hover_texture_key.value(), std::nullopt, ButtonVisualState::Hover);
	}
	if (press_texture_key.has_value()) {
		Sprite(press_texture_key.value(), std::nullopt, ButtonVisualState::Press);
	}
	return *this;
}

Button& Button::SpriteAnchor(Origin anchor, ButtonVisualState state) {
	auto entity{ EnsureButtonPart(*this, ButtonPart::Sprite) };
	auto& visual{ entity.Get<impl::ButtonSpriteVisuals>().states[std::to_underlying(state)] };
	visual.defined = true;
	visual.anchor  = anchor;
	MarkDirty(impl::ButtonDirty::Sprite);
	return *this;
}

Button& Button::RemoveSprite() {
	return RemoveParts(ButtonPart::Sprite);
}

Button& Button::RemoveSprite(ButtonVisualState state) {
	return RemovePart(ButtonPart::Sprite, state);
}

Entity Button::ShapePart(ButtonPart part, ButtonVisualState state, Color color, FillStyle fill) {
	PTGN_ASSERT(
		part == ButtonPart::Background || part == ButtonPart::Border,
		"Shape button parts can only be backgrounds or borders"
	);

	auto entity{ EnsureButtonPart(*this, part) };
	auto& visual{ entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)] };

	if (!visual.defined) {
		visual.defined	  = true;
		visual.color	  = color;
		visual.fill_style = fill;
	}

	MarkDirty(DirtyForPart(part));
	return entity;
}

Button& Button::Background(ButtonVisualState state) {
	auto _{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	return *this;
}

Button& Button::Background() {
	Background(ButtonVisualState::Idle);
	Background(ButtonVisualState::Hover);
	Background(ButtonVisualState::Press);
	return *this;
}

Button& Button::BackgroundOrigin(Origin origin, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].origin = origin;
	MarkDirty(impl::ButtonDirty::Background);
	return *this;
}

Button& Button::ClearBackgroundOrigin() {
	if (auto entity{ FindButtonPart(*this, ButtonPart::Background) }) {
		auto& visuals{ entity->Get<impl::ButtonShapeVisuals>() };
		for (auto& visual : visuals.states) {
			visual.origin.reset();
		}
		MarkDirty(impl::ButtonDirty::Background);
	}
	return *this;
}

Button& Button::BackgroundAnchor(Origin anchor, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].anchor = anchor;
	MarkDirty(impl::ButtonDirty::Background);
	return *this;
}

Button& Button::BackgroundColor(Color color, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].color = color;
	MarkDirty(impl::ButtonDirty::Background);
	return *this;
}

Button& Button::BackgroundColors(
	std::optional<Color> idle, std::optional<Color> hover, std::optional<Color> press
) {
	if (idle.has_value()) {
		BackgroundColor(idle.value(), ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		BackgroundColor(hover.value(), ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		BackgroundColor(press.value(), ButtonVisualState::Press);
	}
	return *this;
}

Button& Button::ToggledBackgroundColors(
	std::optional<Color> toggled, std::optional<Color> toggled_hover,
	std::optional<Color> toggled_press
) {
	if (toggled.has_value()) {
		BackgroundColor(toggled.value(), ButtonVisualState::Toggled);
	}
	if (toggled_hover.has_value()) {
		BackgroundColor(toggled_hover.value(), ButtonVisualState::ToggledHover);
	}
	if (toggled_press.has_value()) {
		BackgroundColor(toggled_press.value(), ButtonVisualState::ToggledPress);
	}
	return *this;
}

Button& Button::DisabledBackgroundColors(
	std::optional<Color> disabled, std::optional<Color> disabled_hover,
	std::optional<Color> disabled_press
) {
	if (disabled.has_value()) {
		BackgroundColor(disabled.value(), ButtonVisualState::Disabled);
	}
	if (disabled_hover.has_value()) {
		BackgroundColor(disabled_hover.value(), ButtonVisualState::DisabledHover);
	}
	if (disabled_press.has_value()) {
		BackgroundColor(disabled_press.value(), ButtonVisualState::DisabledPress);
	}
	return *this;
}

Button& Button::BackgroundSize(V2_float size, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].size = size;
	MarkDirty(impl::ButtonDirty::Background);
	return *this;
}

Button& Button::BackgroundSize(float radius, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Background, state, GetDefaultBackgroundColor(state), FillStyle{ Solid{} }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].size = radius;
	MarkDirty(impl::ButtonDirty::Background);
	return *this;
}

Button& Button::RemoveBackground() {
	return RemoveParts(ButtonPart::Background);
}

Button& Button::RemoveBackground(ButtonVisualState state) {
	return RemovePart(ButtonPart::Background, state);
}

Button& Button::Border(ButtonVisualState state) {
	auto _{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	return *this;
}

Button& Button::Border() {
	Border(ButtonVisualState::Idle);
	Border(ButtonVisualState::Hover);
	Border(ButtonVisualState::Press);
	return *this;
}

Button& Button::BorderOrigin(Origin origin, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].origin = origin;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::ClearBorderOrigin() {
	if (auto entity{ FindButtonPart(*this, ButtonPart::Border) }) {
		auto& visuals{ entity->Get<impl::ButtonShapeVisuals>() };
		for (auto& visual : visuals.states) {
			visual.origin.reset();
		}
		MarkDirty(impl::ButtonDirty::Border);
	}
	return *this;
}

Button& Button::BorderAnchor(Origin anchor, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].anchor = anchor;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::BorderColor(Color color, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].color = color;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::BorderColors(
	std::optional<Color> idle, std::optional<Color> hover, std::optional<Color> press
) {
	if (idle.has_value()) {
		BorderColor(idle.value(), ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		BorderColor(hover.value(), ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		BorderColor(press.value(), ButtonVisualState::Press);
	}
	return *this;
}

Button& Button::BorderWidth(FillStyle fill, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].fill_style = fill;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::BorderSize(V2_float size, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].size = size;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::BorderSize(float radius, ButtonVisualState state) {
	auto entity{ ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state),
		FillStyle{ kDefaultButtonBorderWidth }
	) };
	entity.Get<impl::ButtonShapeVisuals>().states[std::to_underlying(state)].size = radius;
	MarkDirty(impl::ButtonDirty::Border);
	return *this;
}

Button& Button::RemoveBorder() {
	return RemoveParts(ButtonPart::Border);
}

Button& Button::RemoveBorder(ButtonVisualState state) {
	return RemovePart(ButtonPart::Border, state);
}

Button& Button::Animation(
	AnimationConfig config, std::optional<Origin> origin, ButtonVisualState state,
	ButtonAnimationOptions options
) {
	auto entity{ EnsureButtonPart(*this, ButtonPart::Sprite) };
	auto& visuals{ entity.Get<impl::ButtonSpriteVisuals>() };
	auto& visual{ visuals.states[std::to_underlying(state)] };

	visual.defined			 = true;
	visual.animation		 = std::move(config);
	visual.animation_options = options;
	visual.origin			 = origin;

	visuals.applied_animation_state.reset();

	if (!HasScript<impl::ButtonAnimationCompleteScript>(entity)) {
		AddScript<impl::ButtonAnimationCompleteScript>(entity, *this);
	}

	MarkDirty(impl::ButtonDirty::Sprite);
	return *this;
}

Button& Button::Animation(
	AnimationConfig config, std::optional<Origin> origin, ButtonVisualState state
) {
	ButtonAnimationOptions options;

	if (state == ButtonVisualState::Press || state == ButtonVisualState::ToggledPress ||
		state == ButtonVisualState::DisabledPress) {
		options.playback		  = ButtonAnimationPlayback::PlayOnce;
		options.lock_visual_state = true;
		options.block_press		  = false;
	}

	return Animation(std::move(config), origin, state, options);
}

Button& Button::Animation(
	std::optional<AnimationConfig> idle_animation, std::optional<AnimationConfig> hover_animation,
	std::optional<AnimationConfig> press_animation
) {
	if (idle_animation.has_value()) {
		Animation(std::move(idle_animation.value()), std::nullopt, ButtonVisualState::Idle);
	}
	if (hover_animation.has_value()) {
		Animation(std::move(hover_animation.value()), std::nullopt, ButtonVisualState::Hover);
	}
	if (press_animation.has_value()) {
		Animation(std::move(press_animation.value()), std::nullopt, ButtonVisualState::Press);
	}
	return *this;
}

Button& Button::StaticAnimationFrame(
	AnimationConfig config, std::optional<Origin> origin, ButtonVisualState state, std::size_t frame
) {
	return Animation(
		std::move(config), origin, state,
		ButtonAnimationOptions{
			.playback	  = ButtonAnimationPlayback::StaticFrame,
			.static_frame = frame,
		}
	);
}

Button& Button::RemoveAnimation() {
	return RemoveSprite();
}

Button& Button::RemoveAnimation(ButtonVisualState state) {
	return RemoveSprite(state);
}

Button& Button::Sounds(
	std::optional<std::string_view> press_sound_key, std::optional<std::string_view> hover_sound_key
) {
	Sound(press_sound_key, ButtonState::Press);
	Sound(hover_sound_key, ButtonState::Hover);
	return *this;
}

Button& Button::Sound(std::optional<std::string_view> sound_key, ButtonState state) {
	auto& sounds{ TryAdd<impl::ButtonSounds>() };

	std::optional<Audio>* slot{ nullptr };

	switch (state) {
		using enum ButtonState;
		case Idle:	slot = &sounds.idle; break;
		case Hover: slot = &sounds.hover; break;
		case Press: slot = &sounds.press; break;
	}

	PTGN_ASSERT(slot);

	if (!sound_key.has_value()) {
		slot->reset();
		return *this;
	}

	auto audio{ impl::AssetAccessor{ GetScene().ctx().asset }.Get<Audio>(sound_key.value()) };
	slot->emplace(std::move(audio));
	return *this;
}

std::optional<Audio> Button::GetSound(ButtonState state) const {
	auto sounds{ TryGet<impl::ButtonSounds>() };
	if (!sounds) {
		return std::nullopt;
	}

	switch (state) {
		using enum ButtonState;
		case Idle:	return sounds->idle;
		case Hover: return sounds->hover;
		case Press: return sounds->press;
	}

	return std::nullopt;
}

Button& Button::ExclusiveAudio(bool enabled) {
	if (enabled) {
		Add<impl::ButtonExclusiveAudio>();
	} else {
		Remove<impl::ButtonExclusiveAudio>();
	}
	return *this;
}

void Button::MarkDirty(impl::ButtonDirty dirty) {
	Get<impl::ButtonData>().dirty |= dirty;
}

void Button::CommitTextEdit() {
	auto entity{ FindButtonPart(*this, ButtonPart::Text) };

	if (!entity.has_value()) {
		return;
	}

	auto& visuals{ entity->Get<impl::ButtonTextVisuals>() };

	if (!visuals.editing.has_value()) {
		return;
	}

	impl::ButtonTextEditSnapshot snapshot{ visuals.editing.value() };

	ptgn::Text text{ entity.value() };

	auto& visual{ visuals.states[std::to_underlying(snapshot.state)] };

	visual.defined	   = true;
	visual.styled_text = text.GetStyledText();

	if (text.GetTextBox() != snapshot.box) {
		visual.box = text.GetTextBox();
	}

	if (auto origin{ GetDrawOrigin(text) }; origin != snapshot.origin) {
		visual.origin = origin;
	}

	if (const auto& transform{ text.Get<Transform>() }; transform != snapshot.transform) {
		visual.transform = transform;
	}

	visuals.editing.reset();

	MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
}

void Button::RefreshDirty() {
	CommitTextEdit();

	auto& data{ Get<impl::ButtonData>() };
	auto visual_state{ GetVisualState() };
	std::optional<std::variant<V2_float, float>> size;
	if (HasAny<Rect, Circle>()) {
		size = GetSize();
	}
	auto origin{ GetDrawOrigin(*this) };
	bool visible{ IsVisible(*this) };

	if (!data.applied_visual_state.has_value() ||
		data.applied_visual_state.value() != visual_state) {
		data.dirty |= impl::ButtonDirty::All;
	}

	if (!data.applied_size.has_value() || data.applied_size.value() != size) {
		data.dirty |= impl::ButtonDirty::Background | impl::ButtonDirty::Border |
					  impl::ButtonDirty::Sprite | impl::ButtonDirty::Text |
					  impl::ButtonDirty::TextLayout;
	}

	if (!data.applied_origin.has_value() || data.applied_origin.value() != origin) {
		data.dirty |= impl::ButtonDirty::Background | impl::ButtonDirty::Border |
					  impl::ButtonDirty::Sprite | impl::ButtonDirty::Text |
					  impl::ButtonDirty::TextLayout;
	}

	if (!data.applied_visibility.has_value() || data.applied_visibility.value() != visible) {
		data.dirty |= impl::ButtonDirty::Visual;
	}

	auto dirty{ data.dirty };
	data.dirty = impl::ButtonDirty::None;

	if (impl::HasDirty(dirty, impl::ButtonDirty::Background)) {
		ApplyShapeVisual(ButtonPart::Background);
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Border)) {
		ApplyShapeVisual(ButtonPart::Border);
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Sprite)) {
		ApplySpriteVisual();
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Text)) {
		ApplyTextVisual();
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::TextLayout) ||
		impl::HasDirty(dirty, impl::ButtonDirty::Text)) {
		UpdateChildLayouts();
	}

	data.applied_visual_state = visual_state;
	data.applied_size		  = size;
	data.applied_origin		  = origin;
	data.applied_visibility	  = visible;
}

void Button::RefreshVisualState() const {
	auto& button{ const_cast<Button&>(*this) };
	button.MarkDirty(impl::ButtonDirty::All);
	button.RefreshDirty();
}

void Button::ApplyShapeVisual(ButtonPart part) {
	auto entity{ FindButtonPart(*this, part) };
	if (!entity.has_value()) {
		return;
	}

	auto resolved{ ResolveShapeVisual(*this, entity.value(), part) };
	bool visible{ IsVisible(*this) && resolved.visible };
	SetVisible(entity.value(), visible);

	if (!resolved.visible) {
		return;
	}

	SetDrawOrigin(entity.value(), resolved.origin);
	SetPosition(entity.value(), GetButtonLocalRect(*this).GetOriginPoint(resolved.anchor));
	entity->Add<Color>(resolved.color);
	entity->Add<FillStyle>(resolved.fill_style);

	std::visit(
		[&]<typename T>(const T& size) {
			if constexpr (std::same_as<T, V2_float>) {
				entity->Remove<Circle>();
				entity->Add<Rect>(size);
				SetDraw<RectDraw>(entity.value());
			} else if constexpr (std::same_as<T, float>) {
				entity->Remove<Rect>();
				entity->Add<Circle>(size);
				SetDraw<CircleDraw>(entity.value());
			} else {
				static_assert(false, "Incomplete visitor");
			}
		},
		resolved.size
	);
}

void Button::ApplySpriteVisual() {
	ApplySpriteVisual(GetVisualState(), false);
}

void Button::ApplySpriteVisual(ButtonVisualState state, bool transient) {
	auto entity{ FindButtonPart(*this, ButtonPart::Sprite) };
	if (!entity.has_value()) {
		return;
	}

	auto& visuals{ entity->Get<impl::ButtonSpriteVisuals>() };
	visuals.transient_animation = transient;
	auto resolved{ ResolveSpriteVisual(*this, visuals, state) };
	bool visible{ IsVisible(*this) && resolved.visible };
	SetVisible(entity.value(), visible);

	ptgn::Sprite sprite{ entity.value() };

	if (!resolved.visible) {
		if (sprite.Has<impl::AnimationData>()) {
			ptgn::Animation{ sprite }.Stop();
		}
		entity->Remove<impl::ButtonAnimationPart>();
		visuals.applied_animation_state.reset();
		visuals.transient_animation = false;
		return;
	}
	bool has_animation{ resolved.animation && resolved.animation_state.has_value() };

	if (!has_animation && sprite.Has<impl::AnimationData>()) {
		ptgn::Animation{ sprite }.Stop();
	}

	if (!resolved.texture.empty()) {
		sprite.SetTexture(resolved.texture);
	}

	SetTint(sprite, resolved.tint);
	SetDrawOrigin(sprite, resolved.origin);
	SetTransform(sprite, resolved.transform);
	SetPosition(
		sprite, GetPosition(sprite) + GetButtonLocalRect(*this).GetOriginPoint(resolved.anchor)
	);

	if (resolved.size.has_value()) {
		SetDisplaySize(sprite, resolved.size.value());
	} else if (!resolved.texture.empty()) {
		SetDisplaySize(sprite, GetScene().ctx().asset.GetTextureSize(resolved.texture));
	}

	if (has_animation) {
		if (visuals.applied_animation_state != resolved.animation_state) {
			ptgn::Animation{ sprite }.SetConfig(*resolved.animation);
			visuals.applied_animation_state = resolved.animation_state;
		}

		entity->Add<impl::ButtonAnimationPart>(resolved.animation_options);

		if (resolved.animation_options.playback == ButtonAnimationPlayback::StaticFrame) {
			ptgn::Animation animation{ sprite };
			animation.Reset();
			animation.SetCurrentFrame(resolved.animation_options.static_frame);
		}
	} else {
		entity->Remove<impl::ButtonAnimationPart>();
		visuals.applied_animation_state.reset();
	}
}

void Button::ApplyTextVisual() {
	auto entity{ FindButtonPart(*this, ButtonPart::Text) };
	if (!entity.has_value()) {
		return;
	}

	auto& visuals{ entity->Get<impl::ButtonTextVisuals>() };
	auto resolved{ ResolveTextVisual(*this, visuals, GetVisualState()) };

	bool visible{ IsVisible(*this) && resolved.visible };
	SetVisible(entity.value(), visible);

	if (!resolved.visible) {
		return;
	}

	auto content_rect{ GetButtonTextContentRect(*this, resolved.padding) };

	if (!content_rect.GetSize().IsPositive()) {
		SetVisible(entity.value(), false);
		return;
	}

	ptgn::Text text{ entity.value() };

	text.Clear();
	text.Content(std::move(resolved.styled_text));
	text.Box(resolved.box);

	SetDrawOrigin(text, resolved.origin);

	auto transform{ resolved.transform };

	transform.position = content_rect.GetOriginPoint(resolved.anchor) + resolved.transform.position;

	SetTransform(text, transform);
}

void Button::SetState(impl::InternalButtonState state) {
	auto& data{ Get<impl::ButtonData>() };
	if (data.state == state) {
		return;
	}

	data.state = state;
	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();
}

void Button::PlaySound(ButtonState active) {
	auto active_sound{ GetSound(active) };

	// Preserve the old behavior: exclusive audio only stops other button sounds
	// when the active state actually has a sound to play.
	if (!active_sound.has_value()) {
		return;
	}

	auto& audio{ GetScene().ctx().audio };
	bool stop_others{ Has<impl::ButtonExclusiveAudio>() };

	auto play_if_active = [this, &audio, stop_others, active](auto state) {
		auto sound{ GetSound(state) };

		if (!sound.has_value()) {
			return;
		}

		auto& asset_name{ sound.value().GetEntity().Get<impl::AssetName>().value };

		if (state == active) {
			audio.Play(asset_name);
		} else if (stop_others) {
			audio.Stop(asset_name);
		}
	};

	play_if_active(ButtonState::Idle);
	play_if_active(ButtonState::Hover);
	play_if_active(ButtonState::Press);
}

void Button::PlayAnimation(ButtonState active) const {
	(void)active;

	auto& button{ const_cast<Button&>(*this) };
	button.RefreshDirty();

	auto entity{ FindButtonPart(button, ButtonPart::Sprite) };
	if (!entity.has_value() || !entity->Has<impl::AnimationData>()) {
		return;
	}

	auto animation_part{ entity->TryGet<impl::ButtonAnimationPart>() };
	if (!animation_part) {
		return;
	}

	ptgn::Animation animation{ entity.value() };

	switch (animation_part->options.playback) {
		using enum ButtonAnimationPlayback;
		case StaticFrame: ResetButtonAnimation(animation); break;
		case Play:		  [[fallthrough]];
		case PlayOnce:	  animation.Start(true); break;
	}
}

void Button::UpdateChildSizes() const {
	auto& button{ const_cast<Button&>(*this) };
	button.MarkDirty(impl::ButtonDirty::Background | impl::ButtonDirty::Border);
}

void Button::UpdateChildLayouts() const {
	auto entity{ FindButtonPart(*this, ButtonPart::Text) };
	if (!entity.has_value()) {
		return;
	}

	const auto& visuals{ entity->Get<impl::ButtonTextVisuals>() };

	auto resolved{ ResolveTextVisual(*this, visuals, GetVisualState()) };

	if (!resolved.visible || !resolved.auto_box) {
		return;
	}

	auto content_rect{ GetButtonTextContentRect(*this, resolved.padding) };

	if (!content_rect.GetSize().IsPositive()) {
		return;
	}

	auto text_origin_position{ content_rect.GetOriginPoint(resolved.anchor) +
							   resolved.transform.position };

	auto text_box{ GetButtonTextAutoBox(content_rect, text_origin_position, resolved.origin) };

	ptgn::Text text{ entity.value() };

	if (text.GetTextBox().rect != text_box) {
		text.Box(text_box);
	}
}

Button CreateButton(Scene& scene, Transform transform, Origin origin) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.origin = origin,
		}
	);
}

Button CreateButton(Scene& scene, Transform transform, V2_float size, Origin origin) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.size	= size,
			.origin = origin,
		}
	);
}

Button CreateButton(Scene& scene, Transform transform, float radius, Origin origin) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.size	= radius,
			.origin = origin,
		}
	);
}

Button CreateButton(Scene& scene, Transform transform, V2_float size, const ButtonConfig& config) {
	return CreateButton(scene, transform, MakeButtonDesc(size, config));
}

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc) {
	PTGN_ASSERT(
		!ContainsDuplicates(
			desc.shapes,
			[](const ButtonShapeConfig& config) { return std::pair{ config.part, config.state }; }
		),
		"Button description cannot contain duplicate shape parts for the same visual state"
	);

	PTGN_ASSERT(
		!ContainsDuplicates(desc.sprites, &ButtonSpriteConfig::state),
		"Button description cannot contain multiple sprites for the same visual state"
	);

	PTGN_ASSERT(
		!ContainsDuplicates(desc.texts, &ButtonTextConfig::state),
		"Button description cannot contain multiple texts for the same visual state"
	);

	Button button{ scene.CreateEntity() };

	PTGN_DEFAULT_NAME(button, "Button");
	button.Add<impl::Visible>(true);
	button.Add<impl::ButtonData>();
	button.Add<impl::ButtonEnabled>();

	if (desc.ui_layer) {
		SetUI(button, true);
	}

	if (desc.size.has_value()) {
		std::visit([button](const auto& size) mutable { button.Size(size); }, desc.size.value());
	}

	SetTransform(button, transform);
	SetDrawOrigin(button, desc.origin);
	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

	if (!desc.enabled) {
		button.Disable();
	}

	for (const auto& shape : desc.shapes) {
		if (shape.part == ButtonPart::Background) {
			button.Background(shape.state);

			if (shape.size.has_value()) {
				std::visit(
					[&](const auto& size) { button.BackgroundSize(size, shape.state); },
					shape.size.value()
				);
			}
			if (shape.color.has_value()) {
				button.BackgroundColor(shape.color.value(), shape.state);
			}
			if (shape.origin.has_value()) {
				button.BackgroundOrigin(shape.origin.value(), shape.state);
			}
			if (shape.anchor.has_value()) {
				button.BackgroundAnchor(shape.anchor.value(), shape.state);
			}
		} else if (shape.part == ButtonPart::Border) {
			button.Border(shape.state);

			if (shape.size.has_value()) {
				std::visit(
					[&](const auto& size) { button.BorderSize(size, shape.state); },
					shape.size.value()
				);
			}
			if (shape.color.has_value()) {
				button.BorderColor(shape.color.value(), shape.state);
			}
			if (shape.origin.has_value()) {
				button.BorderOrigin(shape.origin.value(), shape.state);
			}
			if (shape.anchor.has_value()) {
				button.BorderAnchor(shape.anchor.value(), shape.state);
			}
			if (shape.fill_style.has_value()) {
				button.BorderWidth(shape.fill_style.value(), shape.state);
			}
		} else {
			PTGN_ERROR("Button description contains invalid shape part");
		}
	}

	for (const auto& sprite_config : desc.sprites) {
		button.Sprite(sprite_config.texture, sprite_config.origin, sprite_config.state);

		auto entity{ FindButtonPart(button, ButtonPart::Sprite) };
		PTGN_ASSERT(entity.has_value(), "Failed to create sprite part for button");

		auto& visual{
			entity->Get<impl::ButtonSpriteVisuals>().states[std::to_underlying(sprite_config.state)]
		};
		visual.defined	 = true;
		visual.transform = sprite_config.transform;
		visual.size		 = sprite_config.size;
		visual.tint		 = sprite_config.tint;
		visual.anchor	 = sprite_config.anchor;
		button.MarkDirty(impl::ButtonDirty::Sprite);
	}

	for (const auto& text_config : desc.texts) {
		ptgn::Text text{ button.Text(
			StyledText{ { TextRun{
				.text  = text_config.content,
				.font  = text_config.font,
				.style = { .color = text_config.color, .size = text_config.font_size },
			} } },
			text_config.state
		) };

		text.Box(text_config.box);
		SetTransform(text, text_config.transform);

		if (text_config.outline_width.has_value()) {
			text.Outline(text_config.outline_color, text_config.outline_width.value());
		}
		if (text_config.origin.has_value()) {
			SetDrawOrigin(text, text_config.origin.value());
		}

		button.CommitTextEdit();

		if (text_config.anchor.has_value()) {
			button.TextAnchor(text_config.anchor.value(), text_config.state);
		}
		button.TextAutoBox(text_config.auto_box, text_config.state);
		button.TextPadding(text_config.padding, text_config.state);
	}

	button.Sound(desc.sounds.idle, ButtonState::Idle);
	button.Sound(desc.sounds.hover, ButtonState::Hover);
	button.Sound(desc.sounds.press, ButtonState::Press);

	auto get_texts = [](auto button) {
		return FindButtonParts(button, ButtonPart::Text) |
			   std::views::transform([](Entity text) { return ptgn::Text{ text }; }) |
			   std::ranges::to<std::vector>();
	};

	if (desc.move.has_value()) {
		const auto& move{ desc.move.value() };

		if (auto entity{ FindButtonPart(button, ButtonPart::Text) }) {
			auto& visuals{ entity->Get<impl::ButtonTextVisuals>() };
			for (auto& visual : visuals.states) {
				if (visual.defined) {
					visual.auto_box = false;
				}
			}
			button.MarkDirty(impl::ButtonDirty::Text | impl::ButtonDirty::TextLayout);
		}

		auto tween_move = [get_texts, move](V2_float offset, auto button) {
			auto texts{ get_texts(button) };
			TranslateTo<Text>(texts, offset, move.duration, move.ease);
		};

		button.OnHoverStart([tween_move, offset = move.offset](auto button) {
			tween_move(offset, button);
		});

		button.OnHoverStop([tween_move](auto button) { tween_move(V2_float{}, button); });
	}

	if (desc.scale.has_value()) {
		const auto& scale{ desc.scale.value() };

		button.OnHoverStart([get_texts, scale](auto button) {
			auto texts{ get_texts(button) };
			ScaleTo<Text>(texts, V2_float{ scale.scale }, scale.duration, scale.ease);
		});

		auto texts{ get_texts(button) };
		auto starting_scales{ texts | std::views::transform([](Text text) {
								  return std::pair{ text, GetScale(text) };
							  }) |
							  std::ranges::to<std::vector>() };

		button.OnHoverStop([get_texts, scale, starting_scales](auto button) {
			auto texts{ get_texts(button) };

			auto target_scales{ texts | std::views::transform([&](Text text) {
									auto it{ std::ranges::find(
										starting_scales, text, &std::pair<Text, V2_float>::first
									) };

									return it != starting_scales.end() ? it->second
																	   : V2_float{ 1.0f, 1.0f };
								}) |
								std::ranges::to<std::vector>() };

			ScaleTo<Text>(texts, target_scales, scale.duration, scale.ease);
		});
	}

	button.RefreshDirty();
	return button;
}

Button CreateAnimatedButton(Scene& scene, Transform transform, const AnimatedButtonConfig& config) {
	auto size{ config.size.value_or(scene.ctx().asset.GetTextureSize(config.texture)) };

	Button button{ CreateButton(scene, transform, size) };
	PTGN_DEFAULT_NAME(button, "Animated Button");

	button.Sprites(config.texture, config.texture_hover, config.texture_press);

	if (config.anchor.has_value()) {
		button.SpriteAnchor(config.anchor.value(), ButtonVisualState::Base);
	}

	if (config.animation_hover.has_value()) {
		button.Animation(config.animation_hover.value(), config.origin, ButtonVisualState::Hover);
	}

	if (config.animation_press.has_value()) {
		button.Animation(config.animation_press.value(), config.origin, ButtonVisualState::Press);
	}

	button.Sound(config.sound_hover, ButtonState::Hover);
	button.Sound(config.sound_press, ButtonState::Press);
	button.RefreshDirty();
	return button;
}

} // namespace ptgn
