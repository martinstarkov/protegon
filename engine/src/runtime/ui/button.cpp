#include "runtime/ui/button.h"

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
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
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"

namespace ptgn {

namespace {

constexpr Color kDefaultIdleButtonBackgroundColor{ color::DarkGray };
constexpr Color kDefaultHoverButtonBackgroundColor{ color::Gray };
constexpr Color kDefaultPressButtonBackgroundColor{ 32, 32, 32, 255 };

constexpr Color kDefaultIdleButtonBorderColor{ color::Gray };
constexpr Color kDefaultHoverButtonBorderColor{ color::LightGray };
constexpr Color kDefaultPressButtonBorderColor{ color::Gray };

constexpr float kDefaultButtonBorderWidth{ 2.0f };

constexpr ButtonVisualState NormalVisualState(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Idle;
		case Hover: return ButtonVisualState::Hover;
		case Press: return ButtonVisualState::Press;

		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

constexpr ButtonVisualState DisabledVisualState(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Disabled;
		case Hover: return ButtonVisualState::DisabledHover;
		case Press: return ButtonVisualState::DisabledPress;

		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

constexpr ButtonVisualState ToggledVisualState(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Toggled;
		case Hover: return ButtonVisualState::ToggledHover;
		case Press: return ButtonVisualState::ToggledPress;

		default:	PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

ButtonVisualState PressVisualState(Button button) {
	if (!button.IsEnabled(false)) {
		return ButtonVisualState::DisabledPress;
	}

	if (button.IsToggled()) {
		return ButtonVisualState::ToggledPress;
	}

	return ButtonVisualState::Press;
}

constexpr Color GetDefaultBackgroundColor(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;

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

constexpr Color GetDefaultShapeColor(impl::ButtonPart part, ButtonVisualState state) {
	switch (part) {
		case impl::ButtonPart::Background: return GetDefaultBackgroundColor(state);
		case impl::ButtonPart::Border:	   return GetDefaultBorderColor(state);

		default:						   PTGN_ERROR("Unsupported shape button part: ", std::to_underlying(part));
	}
}

constexpr FillStyle GetDefaultShapeFill(impl::ButtonPart part) {
	switch (part) {
		case impl::ButtonPart::Background: return Solid{};
		case impl::ButtonPart::Border:	   return kDefaultButtonBorderWidth;

		default:						   PTGN_ERROR("Unsupported shape button part: ", std::to_underlying(part));
	}
}

constexpr impl::ButtonDirty DirtyForPart(impl::ButtonPart part) {
	switch (part) {
		case impl::ButtonPart::Background: return impl::ButtonDirty::Background;
		case impl::ButtonPart::Border:	   return impl::ButtonDirty::Border;
		case impl::ButtonPart::Text:	   return impl::ButtonDirty::Text;
		case impl::ButtonPart::Sprite:	   return impl::ButtonDirty::Sprite;

		default:						   PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}
}

std::span<const ButtonVisualState> GetVisualStateFallbacks(ButtonVisualState state) {
	using enum ButtonVisualState;

	static constexpr std::array idle{ Idle };
	static constexpr std::array hover{ Hover, Idle };
	static constexpr std::array press{ Press, Hover, Idle };

	static constexpr std::array disabled{ Disabled, Idle };
	static constexpr std::array disabled_hover{ DisabledHover, Disabled, Hover, Idle };
	static constexpr std::array disabled_press{ DisabledPress, DisabledHover, Disabled,
												Press,		   Hover,		  Idle };

	static constexpr std::array toggled{ Toggled, Idle };
	static constexpr std::array toggled_hover{ ToggledHover, Toggled, Hover, Idle };
	static constexpr std::array toggled_press{ ToggledPress, ToggledHover, Toggled,
											   Press,		 Hover,		   Idle };

	switch (state) {
		case Idle:			return idle;
		case Hover:			return hover;
		case Press:			return press;
		case Disabled:		return disabled;
		case DisabledHover: return disabled_hover;
		case DisabledPress: return disabled_press;
		case Toggled:		return toggled;
		case ToggledHover:	return toggled_hover;
		case ToggledPress:	return toggled_press;

		default:			PTGN_ERROR("Unknown ButtonVisualState: ", std::to_underlying(state));
	}
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

	return Rect{ size, text_origin };
}

template <typename TVisual>
bool HasResolvedState(
	const std::array<TVisual, kButtonVisualStateCount>& states, ButtonVisualState state
) {
	return std::ranges::any_of(
		GetVisualStateFallbacks(state), [&states](ButtonVisualState fallback) {
			return states[std::to_underlying(fallback)].defined;
		}
	);
}

template <typename TVisual, typename T>
const T* ResolveProperty(
	const std::array<TVisual, kButtonVisualStateCount>& states, ButtonVisualState state,
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

void ApplyShapeSize(Entity entity, std::variant<V2_float, float> size) {
	std::visit(
		[entity]<typename T>(const T& value) mutable {
			if constexpr (std::same_as<T, V2_float>) {
				entity.Remove<Circle>();
				entity.Add<Rect>(value);
				SetDraw<RectDraw>(entity);
			} else if constexpr (std::same_as<T, float>) {
				entity.Remove<Rect>();
				entity.Add<Circle>(value);
				SetDraw<CircleDraw>(entity);
			} else {
				static_assert(false, "Incomplete visitor");
			}
		},
		size
	);
}

void EnsureDefaultShapeVisual(impl::ButtonPart part, ButtonShapeVisual& visual) {
	if (visual.defined) {
		return;
	}

	visual.defined	  = true;
	visual.color	  = GetDefaultShapeColor(part, ButtonVisualState::Idle);
	visual.fill_style = GetDefaultShapeFill(part);
}

void EnsureDefaultTextVisual(ButtonTextVisual& visual) {
	if (visual.defined) {
		return;
	}

	visual.defined	   = true;
	visual.styled_text = StyledText{};
	visual.styled_text->runs.emplace_back();
	visual.anchor	= Origin::Center;
	visual.origin	= Origin::Center;
	visual.auto_box = true;
	visual.padding	= Padding{};
}

void EnsureStyledTextHasRun(StyledText& styled_text) {
	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back();
	}
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

bool IsButtonPart(Entity entity, impl::ButtonPart part) {
	switch (part) {
		case impl::ButtonPart::Background: return entity.Has<ButtonBackgroundVisuals>();
		case impl::ButtonPart::Border:	   return entity.Has<ButtonBorderVisuals>();
		case impl::ButtonPart::Text:	   return entity.Has<ButtonTextVisuals>();
		case impl::ButtonPart::Sprite:	   return entity.Has<ButtonSpriteVisuals>();

		default:						   PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}
}

ButtonShapeVisuals& GetShapeVisuals(Entity entity, impl::ButtonPart part) {
	switch (part) {
		case impl::ButtonPart::Background: return entity.Get<ButtonBackgroundVisuals>();
		case impl::ButtonPart::Border:	   return entity.Get<ButtonBorderVisuals>();

		default:						   PTGN_ERROR("Unsupported shape button part: ", std::to_underlying(part));
	}
}

void ApplyButtonShapeConfig(ButtonShapeVisuals& visuals, const ButtonShapeConfig& config) {
	if (!(config.size.has_value() || config.origin.has_value() || config.anchor.has_value() ||
		  config.transform.has_value() || config.color.has_value() ||
		  config.color_hover.has_value() || config.color_press.has_value() ||
		  config.fill_style.has_value())) {
		return;
	}

	auto& visual{ visuals.states[std::to_underlying(ButtonVisualState::Idle)] };

	visual.defined = true;

	if (config.size.has_value()) {
		visual.size = config.size;
	}
	if (config.origin.has_value()) {
		visual.origin = config.origin;
	}
	if (config.anchor.has_value()) {
		visual.anchor = config.anchor;
	}
	if (config.transform.has_value()) {
		visual.transform = config.transform;
	}
	if (config.fill_style.has_value()) {
		visual.fill_style = config.fill_style;
	}
	if (config.color.has_value()) {
		visual.color = config.color;
	}

	if (config.color_hover.has_value()) {
		auto& hover{ visuals.states[std::to_underlying(ButtonVisualState::Hover)] };
		hover.defined = true;
		hover.color	  = config.color_hover;
	}

	if (config.color_press.has_value()) {
		auto& press{ visuals.states[std::to_underlying(ButtonVisualState::Press)] };
		press.defined = true;
		press.color	  = config.color_press;
	}
}

void ApplyButtonTextConfig(ButtonTextVisuals& visuals, const ButtonTextConfig& config) {
	if (!config.content.has_value()) {
		return;
	}

	auto apply_text_state = [&config, &visuals](ButtonVisualState state, Color color) {
		auto& visual{ visuals.states[std::to_underlying(state)] };

		TextRun run;

		run.text		= config.content.value_or(std::string{});
		run.font		= config.font;
		run.style.color = color;
		run.style.size	= config.font_size;

		if (config.outline_width.has_value()) {
			run.style.sdf.outline = DistanceFieldLayerStyle{
				.color	  = config.outline_color,
				.width	  = config.outline_width.value(),
				.softness = 1.0f,
			};
		}

		visual.defined	   = true;
		visual.styled_text = { run };
		visual.box		   = config.box;
		visual.origin	   = config.origin;
		visual.anchor	   = config.anchor;
		visual.transform   = config.transform;
		visual.auto_box	   = config.auto_box;
		visual.padding	   = config.padding;
	};

	apply_text_state(ButtonVisualState::Idle, config.color.value_or(color::White));

	if (config.color_hover.has_value()) {
		apply_text_state(ButtonVisualState::Hover, config.color_hover.value());
	}

	if (config.color_press.has_value()) {
		apply_text_state(ButtonVisualState::Press, config.color_press.value());
	}
}

void ApplyButtonSpriteConfig(ButtonSpriteVisuals& visuals, const ButtonSpriteConfig& config) {
	if (!(config.texture.has_value() || config.texture_hover.has_value() ||
		  config.texture_press.has_value() || config.tint.has_value() ||
		  config.tint_hover.has_value() || config.tint_press.has_value() ||
		  config.origin.has_value() || config.anchor.has_value() || config.size.has_value())) {
		return;
	}

	auto apply_sprite_state = [&config, &visuals](
								  ButtonVisualState state,
								  const std::optional<std::string>& texture,
								  const std::optional<Color>& tint
							  ) {
		if (!texture.has_value() && !tint.has_value()) {
			return;
		}

		auto& visual{ visuals.states[std::to_underlying(state)] };

		visual.defined = true;

		if (config.origin.has_value()) {
			visual.origin = config.origin;
		}
		if (config.anchor.has_value()) {
			visual.anchor = config.anchor;
		}

		visual.transform = config.transform;

		if (config.size.has_value()) {
			visual.size = config.size;
		}

		if (texture.has_value()) {
			visual.texture = texture;
		}
		if (tint.has_value()) {
			visual.tint = tint;
		}
	};

	apply_sprite_state(ButtonVisualState::Idle, config.texture, config.tint);
	apply_sprite_state(ButtonVisualState::Hover, config.texture_hover, config.tint_hover);
	apply_sprite_state(ButtonVisualState::Press, config.texture_press, config.tint_press);
}

void ApplyButtonSoundConfig(ButtonSoundVisuals& visuals, const ButtonSoundConfig& config) {
	auto set_button_sound = [](auto& sounds, auto state, const auto& sound) {
		if (sound.has_value()) {
			sounds.states[std::to_underlying(state)] = sound;
		}
	};

	set_button_sound(visuals, ButtonVisualState::Idle, config.idle);
	set_button_sound(visuals, ButtonVisualState::Hover, config.hover);
	set_button_sound(visuals, ButtonVisualState::Press, config.press);
	set_button_sound(visuals, ButtonVisualState::Disabled, config.disabled);
	set_button_sound(visuals, ButtonVisualState::DisabledHover, config.disabled_hover);
	set_button_sound(visuals, ButtonVisualState::DisabledPress, config.disabled_press);
	set_button_sound(visuals, ButtonVisualState::Toggled, config.toggled);
	set_button_sound(visuals, ButtonVisualState::ToggledHover, config.toggled_hover);
	set_button_sound(visuals, ButtonVisualState::ToggledPress, config.toggled_press);
}

void ApplyButtonAnimationConfig(
	ButtonSpriteVisuals& visuals, ButtonVisualState state,
	const std::optional<AnimationConfig>& config, const ButtonAnimationOptions& options
) {
	if (!config.has_value()) {
		return;
	}

	auto& visual{ visuals.states[std::to_underlying(state)] };

	visual.defined			 = true;
	visual.animation		 = config;
	visual.animation_options = options;
}

ButtonDesc ToButtonDesc(const ButtonConfig& config) {
	ButtonDesc desc;

	desc.size	  = config.size;
	desc.origin	  = config.origin;
	desc.ui_layer = config.ui_layer;
	desc.enabled  = config.enabled;
	desc.move	  = config.move;
	desc.scale	  = config.scale;

	ApplyButtonShapeConfig(desc.background, config.background);
	ApplyButtonShapeConfig(desc.border, config.border);
	ApplyButtonTextConfig(desc.text, config.text);
	ApplyButtonSpriteConfig(desc.sprite, config.sprite);
	ApplyButtonSoundConfig(desc.sounds, config.sounds);

	return desc;
}

ButtonDesc ToButtonDesc(const AnimatedButtonConfig& config) {
	ButtonDesc desc;

	if (config.size.has_value()) {
		desc.size = std::variant<V2_float, float>{ config.size.value() };
	}

	desc.origin	  = config.origin;
	desc.ui_layer = config.ui_layer;
	desc.enabled  = config.enabled;

	ApplyButtonSpriteConfig(desc.sprite, config.sprite);

	ApplyButtonAnimationConfig(
		desc.sprite, ButtonVisualState::Idle, config.animation, config.animation_options
	);
	ApplyButtonAnimationConfig(
		desc.sprite, ButtonVisualState::Hover, config.animation_hover,
		config.animation_options_hover
	);
	ApplyButtonAnimationConfig(
		desc.sprite, ButtonVisualState::Press, config.animation_press,
		config.animation_options_press
	);

	ApplyButtonSoundConfig(desc.sounds, config.sounds);

	return desc;
}

Button CreateBaseButton(Scene& scene, Transform transform, Origin origin) {
	Button button{ scene.CreateEntity() };

	PTGN_DEFAULT_NAME(button, "Button");

	button.Add<impl::Visible>(true);
	button.Add<impl::ButtonData>();

	SetUI(button, true);
	SetTransform(button, transform);
	SetDrawOrigin(button, origin);
	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

	return button;
}

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Button button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::AnimationComplete>([this]() {
		if (!button || !entity.Has<ButtonSpriteVisuals>()) {
			return;
		}

		auto& visuals{ entity.Get<ButtonSpriteVisuals>() };

		if (auto& data{ button.Get<ButtonData>() }; data.visual_lock.has_value()) {
			ButtonVisualState animation_state;
			auto animation{ ResolveProperty(
				visuals.states, data.visual_lock->state, &ButtonSpriteVisual::animation,
				&animation_state
			) };

			if (!animation) {
				return;
			}

			data.visual_lock.reset();
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
	const auto& button{ Get<impl::ButtonData>() };

	return check_for_hover_enabled ? button.hover_enabled : button.press_enabled;
}

bool Button::IsToggled() const {
	return Get<impl::ButtonData>().toggled;
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
	const auto& button{ Get<impl::ButtonData>() };
	auto state{ GetState() };

	if (!button.press_enabled) {
		return DisabledVisualState(state);
	}

	if (button.visual_lock.has_value()) {
		return button.visual_lock->state;
	}

	if (button.toggled) {
		return ToggledVisualState(state);
	}

	return NormalVisualState(state);
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

Button& Button::SetEnabled(bool enable_press, bool enable_hover, bool reset_state) {
	auto& button{ Get<impl::ButtonData>() };

	button.press_enabled = enable_press;
	button.hover_enabled = enable_hover;

	if (reset_state) {
		button.state = impl::InternalButtonState::IdleUp;
		button.visual_lock.reset();
	}

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

Button& Button::SetToggled(bool toggled) {
	auto& button{ Get<impl::ButtonData>() };

	if (button.toggled == toggled) {
		return *this;
	}

	button.toggled = toggled;

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

Button& Button::Toggle() {
	return SetToggled(!IsToggled());
}

Button& Button::Press() {
	if (!IsEnabled(false)) {
		return *this;
	}

	auto& button{ Get<impl::ButtonData>() };

	if (button.visual_lock.has_value() && button.visual_lock->block_press) {
		return *this;
	}

	auto press_visual_state{ PressVisualState(*this) };
	auto sprite{ FindPart(impl::ButtonPart::Sprite) };
	ButtonAnimationOptions animation_options;
	auto has_animation{ false };

	if (sprite.has_value()) {
		auto& visuals{ sprite->Get<ButtonSpriteVisuals>() };
		ButtonVisualState animation_state;
		auto animation{ ResolveProperty(
			visuals.states, press_visual_state, &ButtonSpriteVisual::animation, &animation_state
		) };

		if (animation) {
			has_animation = true;

			const auto& animation_visual{ visuals.states[std::to_underlying(animation_state)] };
			if (animation_visual.animation_options.has_value()) {
				animation_options = animation_visual.animation_options.value();
			}
		}
	}

	if (has_animation) {
		const auto& options{ animation_options };

		if (options.lock_visual_state) {
			button.visual_lock = {
				.state		 = press_visual_state,
				.block_press = options.block_press,
			};

			MarkDirty(impl::ButtonDirty::All);
			RefreshDirty();
		} else {
			ApplySpriteVisual(press_visual_state, true);
		}

		if (sprite.has_value() && sprite->Has<impl::AnimationData>()) {
			ptgn::Animation sprite_animation{ sprite.value() };
			ResetButtonAnimation(sprite_animation);
			sprite_animation.Start(true);
		}
	} else {
		PlayAnimation(ButtonState::Press);
	}

	PlaySound(press_visual_state);
	PushEvent<event::ButtonPress>(*this, *this);

	return *this;
}

Button& Button::StartHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PlaySound(GetVisualState());
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

	PlaySound(GetVisualState());
	PlayAnimation(ButtonState::Idle);
	PushEvent<event::ButtonHoverStop>(*this, *this);

	return *this;
}

Button& Button::Size(V2_float size) {
	Remove<Circle>();
	Add<Rect>(size);

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

Button& Button::Size(float radius) {
	Remove<Rect>();
	Add<Circle>(radius);

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

ButtonBackground Button::Background(ButtonVisualState state) {
	return ButtonBackground{ *this, state };
}

ButtonBorder Button::Border(ButtonVisualState state) {
	return ButtonBorder{ *this, state };
}

ButtonText Button::Text(ButtonVisualState state) {
	return ButtonText{ *this, state };
}

ButtonSprite Button::Sprite(ButtonVisualState state) {
	return ButtonSprite{ *this, state };
}

ButtonAnimation Button::Animation(ButtonVisualState state) {
	return ButtonAnimation{ *this, state };
}

Button& Button::RemoveBackgrounds() {
	return RemoveParts(impl::ButtonPart::Background);
}

Button& Button::RemoveBackground(ButtonVisualState state) {
	return RemovePart(impl::ButtonPart::Background, state);
}

Button& Button::RemoveBorders() {
	return RemoveParts(impl::ButtonPart::Border);
}

Button& Button::RemoveBorder(ButtonVisualState state) {
	return RemovePart(impl::ButtonPart::Border, state);
}

Button& Button::RemoveTexts() {
	return RemoveParts(impl::ButtonPart::Text);
}

Button& Button::RemoveText(ButtonVisualState state) {
	return RemovePart(impl::ButtonPart::Text, state);
}

Button& Button::RemoveSprites() {
	return RemoveParts(impl::ButtonPart::Sprite);
}

Button& Button::RemoveSprite(ButtonVisualState state) {
	return RemovePart(impl::ButtonPart::Sprite, state);
}

Button& Button::RemoveAnimations() {
	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity.has_value()) {
		return *this;
	}

	auto& visuals{ entity->Get<ButtonSpriteVisuals>() };

	for (auto& visual : visuals.states) {
		visual.animation.reset();
		visual.animation_options.reset();
	}

	visuals.applied_animation_state.reset();
	entity->Remove<impl::ButtonAnimationPart>();

	MarkDirty(impl::ButtonDirty::Sprite);
	RefreshDirty();

	return *this;
}

Button& Button::RemoveAnimation(ButtonVisualState state) {
	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity.has_value()) {
		return *this;
	}

	auto& visuals{ entity->Get<ButtonSpriteVisuals>() };
	auto& visual{ visuals.states[std::to_underlying(state)] };

	visual.animation.reset();
	visual.animation_options.reset();
	visuals.applied_animation_state.reset();

	entity->Remove<impl::ButtonAnimationPart>();

	MarkDirty(impl::ButtonDirty::Sprite);
	RefreshDirty();

	return *this;
}

Button& Button::Sound(std::optional<std::string_view> sound_key, ButtonVisualState state) {
	auto& sounds{ TryAdd<ButtonSounds>() };
	auto& slot{ sounds.states[std::to_underlying(state)] };

	if (!sound_key.has_value()) {
		slot.reset();
		return *this;
	}

	auto audio{ impl::AssetAccessor{ GetScene().ctx().asset }.Get<Audio>(sound_key.value()) };
	slot.emplace(std::move(audio));

	return *this;
}

Button& Button::RemoveSound(ButtonVisualState state) {
	if (auto sounds{ TryGet<ButtonSounds>() }) {
		sounds->states[std::to_underlying(state)].reset();
	}

	return *this;
}

Button& Button::RemoveSounds() {
	if (auto sounds{ Remove<ButtonSounds>() }) {
		sounds->states = {};
	}

	return *this;
}

Button& Button::ExclusiveAudio(bool enabled) {
	TryAdd<ButtonSounds>().exclusive = enabled;
	return *this;
}

std::optional<Audio> Button::GetSound(ButtonVisualState state) const {
	if (auto sounds{ TryGet<ButtonSounds>() }) {
		for (auto fallback : GetVisualStateFallbacks(state)) {
			const auto& sound{ sounds->states[std::to_underlying(fallback)] };
			if (sound.has_value()) {
				return sound;
			}
		}
	}

	return std::nullopt;
}

void Button::SetState(impl::InternalButtonState state) {
	auto& button{ Get<impl::ButtonData>() };

	if (button.state == state) {
		return;
	}

	button.state = state;

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();
}

void Button::MarkDirty(impl::ButtonDirty dirty) {
	Get<impl::ButtonData>().dirty |= dirty;
}

void Button::RefreshDirty() {
	auto& data{ Get<impl::ButtonData>() };
	auto visual_state{ GetVisualState() };

	if (!data.applied_visual_state.has_value() ||
		data.applied_visual_state.value() != visual_state) {
		data.dirty				  |= impl::ButtonDirty::All;
		data.applied_visual_state  = visual_state;
	}

	auto dirty{ data.dirty };
	data.dirty = impl::ButtonDirty::None;

	if (impl::HasDirty(dirty, impl::ButtonDirty::Background)) {
		ApplyShapeVisual(impl::ButtonPart::Background);
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Border)) {
		ApplyShapeVisual(impl::ButtonPart::Border);
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Text)) {
		ApplyTextVisual();
	}
	if (impl::HasDirty(dirty, impl::ButtonDirty::Sprite)) {
		ApplySpriteVisual();
	}
}

void Button::RefreshVisualState() const {
	auto& button{ const_cast<Button&>(*this) };

	button.MarkDirty(impl::ButtonDirty::All);
	button.RefreshDirty();
}

Button& Button::LockVisualState(ButtonVisualState state, bool block_press) {
	auto& button{ Get<impl::ButtonData>() };

	button.visual_lock = {
		.state		 = state,
		.block_press = block_press,
	};

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

Button& Button::UnlockVisualState() {
	auto& button{ Get<impl::ButtonData>() };

	if (!button.visual_lock.has_value()) {
		return *this;
	}

	button.visual_lock.reset();

	MarkDirty(impl::ButtonDirty::All);
	RefreshDirty();

	return *this;
}

Entity Button::EnsurePart(impl::ButtonPart part) const {
	auto existing{ FindPart(part) };

	if (existing.has_value()) {
		return existing.value();
	}

	Entity entity;

	switch (part) {
		case impl::ButtonPart::Background: {
			entity = GetScene().CreateEntity();
			entity.Add<ButtonBackgroundVisuals>();
			break;
		}

		case impl::ButtonPart::Border: {
			entity = GetScene().CreateEntity();
			entity.Add<ButtonBorderVisuals>();
			break;
		}

		case impl::ButtonPart::Text: {
			ptgn::Text text{ CreateText(GetScene()) };
			text.ClearAlignment();
			text.Remove<Origin>();
			entity = text;
			entity.Add<ButtonTextVisuals>();
			break;
		}

		case impl::ButtonPart::Sprite: {
			entity = CreateSprite(GetScene());
			entity.Add<ButtonSpriteVisuals>();

			if (!HasScript<impl::ButtonAnimationCompleteScript>(entity)) {
				AddScript<impl::ButtonAnimationCompleteScript>(entity, *this);
			}

			break;
		}

		default: PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}

	auto name{ std::string{ "Button " } };

	switch (part) {
		case impl::ButtonPart::Background: name += "Background"; break;
		case impl::ButtonPart::Border:	   name += "Border"; break;
		case impl::ButtonPart::Text:	   name += "Text"; break;
		case impl::ButtonPart::Sprite:	   name += "Sprite"; break;

		default:						   PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}

	PTGN_DEFAULT_NAME(entity, name);

	entity.Add<impl::Visible>(true);

	SetParent(entity, *this);
	SetUI(entity, true);

	return entity;
}

std::optional<Entity> Button::FindPart(impl::ButtonPart part) const {
	if (!HasChildren(*this)) {
		return std::nullopt;
	}

	auto children{ GetChildren(*this) };
	auto it{ std::ranges::find_if(children, [part](Entity entity) {
		return IsButtonPart(entity, part);
	}) };

	return it != children.end() ? std::optional<Entity>{ *it } : std::nullopt;
}

ButtonShapeVisual& Button::ShapeVisual(impl::ButtonPart part, ButtonVisualState state) {
	auto entity{ EnsurePart(part) };
	auto& visual{ GetShapeVisuals(entity, part).states[std::to_underlying(state)] };

	if (state == ButtonVisualState::Idle) {
		EnsureDefaultShapeVisual(part, visual);
	}

	return visual;
}

ButtonTextVisual& Button::TextVisual(ButtonVisualState state) {
	auto entity{ EnsurePart(impl::ButtonPart::Text) };
	auto& visual{ entity.Get<ButtonTextVisuals>().states[std::to_underlying(state)] };

	if (state == ButtonVisualState::Idle) {
		EnsureDefaultTextVisual(visual);
	}

	return visual;
}

ButtonSpriteVisual& Button::SpriteVisual(ButtonVisualState state) {
	auto entity{ EnsurePart(impl::ButtonPart::Sprite) };
	auto& visual{ entity.Get<ButtonSpriteVisuals>().states[std::to_underlying(state)] };

	visual.defined = true;

	return visual;
}

StyledText Button::GetTextFallback(ButtonVisualState state) const {
	auto entity{ FindPart(impl::ButtonPart::Text) };

	if (!entity.has_value()) {
		StyledText styled_text;
		styled_text.runs.emplace_back();
		return styled_text;
	}

	const auto& visuals{ entity->Get<ButtonTextVisuals>() };

	if (auto value{ ResolveProperty(visuals.states, state, &ButtonTextVisual::styled_text) }) {
		return *value;
	}

	StyledText styled_text;
	styled_text.runs.emplace_back();
	return styled_text;
}

void Button::ApplyShapeVisual(impl::ButtonPart part) const {
	auto entity{ FindPart(part) };

	if (!entity.has_value()) {
		return;
	}

	const auto& visuals{ GetShapeVisuals(entity.value(), part) };
	auto visual_state{ GetVisualState() };

	if (!HasResolvedState(visuals.states, visual_state)) {
		SetVisible(entity.value(), false);
		return;
	}

	SetVisible(entity.value(), IsVisible(*this));

	auto size{ HasAny<Rect, Circle>() ? GetSize() : std::variant<V2_float, float>{ V2_float{} } };
	auto origin{ GetDrawOrigin(*this) };
	auto anchor{ GetDrawOrigin(*this) };
	Transform transform;

	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::size) }) {
		size = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::origin) }) {
		origin = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::anchor) }) {
		anchor = *value;
	}
	if (auto value{
			ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::transform) }) {
		transform = *value;
	}

	ApplyShapeSize(entity.value(), size);

	auto button_rect{ GetButtonLocalRect(*this) };
	transform.position += button_rect.GetOriginPoint(anchor);

	SetTransform(entity.value(), transform);
	SetDrawOrigin(entity.value(), origin);

	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::color) }) {
		entity->Add<Color>(*value);
	}
	if (auto value{
			ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::fill_style) }) {
		entity->Add<FillStyle>(*value);
	}
}

void Button::ApplyTextVisual() const {
	auto entity{ FindPart(impl::ButtonPart::Text) };

	if (!entity.has_value()) {
		return;
	}

	const auto& visuals{ entity->Get<ButtonTextVisuals>() };
	auto visual_state{ GetVisualState() };

	if (!HasResolvedState(visuals.states, visual_state)) {
		SetVisible(entity.value(), false);
		return;
	}

	auto styled_text{
		ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::styled_text)
	};

	if (!styled_text || !styled_text->HasContent()) {
		SetVisible(entity.value(), false);
		return;
	}

	SetVisible(entity.value(), IsVisible(*this));

	TextBox box;
	auto anchor{ Origin::Center };
	auto origin{ Origin::Center };
	Transform transform;
	auto auto_box{ true };
	Padding padding;

	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::box) }) {
		box = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::anchor) }) {
		anchor = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::origin) }) {
		origin = *value;
	} else {
		origin = anchor;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::transform) }) {
		transform = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::auto_box) }) {
		auto_box = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::padding) }) {
		padding = *value;
	}

	auto button_rect{ GetButtonLocalRect(*this) };
	auto anchor_position{ button_rect.GetOriginPoint(anchor) };

	transform.position += anchor_position;

	if (!box.style.alignment.horizontal.has_value()) {
		box.style.alignment.horizontal = GetAlignment(origin).horizontal;
	}

	if (!box.style.alignment.vertical.has_value()) {
		box.style.alignment.vertical = GetAlignment(origin).vertical;
	}

	if (auto_box) {
		box.rect =
			GetButtonTextAutoBox(GetButtonTextContentRect(*this, padding), anchor_position, origin);
	}

	ptgn::Text text{ entity.value() };

	text.Clear();
	text.Content(*styled_text);
	text.Box(box);

	SetDrawOrigin(text, origin);
	SetTransform(text, transform);
}

void Button::ApplySpriteVisual() const {
	ApplySpriteVisual(GetVisualState(), false);
}

void Button::ApplySpriteVisual(ButtonVisualState state, bool transient) const {
	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity.has_value()) {
		return;
	}

	auto& visuals{ entity->Get<ButtonSpriteVisuals>() };
	visuals.transient_animation = transient;

	auto visible{ HasResolvedState(visuals.states, state) };

	SetVisible(entity.value(), IsVisible(*this) && visible);

	ptgn::Sprite sprite{ entity.value() };

	if (!visible) {
		if (sprite.Has<impl::AnimationData>()) {
			ptgn::Animation{ sprite }.Stop();
		}

		entity->Remove<impl::ButtonAnimationPart>();
		visuals.applied_animation_state.reset();
		visuals.transient_animation = false;
		return;
	}

	std::string texture;
	auto origin{ GetDrawOrigin(*this) };
	auto anchor{ GetDrawOrigin(*this) };
	Transform transform;
	std::optional<V2_float> size;
	Color tint{ color::White };
	const AnimationConfig* animation{ nullptr };
	ButtonAnimationOptions animation_options;
	std::optional<ButtonVisualState> animation_state;

	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::texture) }) {
		texture = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::origin) }) {
		origin = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::anchor) }) {
		anchor = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::transform) }) {
		transform = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::size) }) {
		size = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, state, &ButtonSpriteVisual::tint) }) {
		tint = *value;
	}

	ButtonVisualState resolved_animation_state;
	animation = ResolveProperty(
		visuals.states, state, &ButtonSpriteVisual::animation, &resolved_animation_state
	);

	if (animation) {
		animation_state = resolved_animation_state;

		const auto& animation_visual{
			visuals.states[std::to_underlying(resolved_animation_state)]
		};
		if (animation_visual.animation_options.has_value()) {
			animation_options = animation_visual.animation_options.value();
		}
	}

	if (!texture.empty()) {
		sprite.SetTexture(texture);
	}

	SetTint(sprite, tint);
	SetDrawOrigin(sprite, origin);

	transform.position += GetButtonLocalRect(*this).GetOriginPoint(anchor);
	SetTransform(sprite, transform);

	if (size.has_value()) {
		SetDisplaySize(sprite, size.value());
	} else {
		SetDisplaySize(sprite, std::nullopt);
	}

	bool has_animation{ animation && animation_state.has_value() };

	if (!has_animation) {
		if (sprite.Has<impl::AnimationData>()) {
			ptgn::Animation{ sprite }.Stop();
		}

		entity->Remove<impl::ButtonAnimationPart>();
		visuals.applied_animation_state.reset();
		return;
	}

	PTGN_ASSERT(!size.has_value(), "Animations cannot have a custom fixed texture size");

	if (visuals.applied_animation_state != animation_state) {
		ptgn::Animation{ sprite }.SetConfig(*animation);
		visuals.applied_animation_state = animation_state;
	}

	entity->Add<impl::ButtonAnimationPart>(animation_options);

	if (animation_options.playback == ButtonAnimationPlayback::StaticFrame) {
		ptgn::Animation animation{ sprite };
		animation.Reset();
		animation.SetCurrentFrame(animation_options.static_frame);
	}
}

void Button::PlaySound(ButtonVisualState state) {
	auto active_sound{ GetSound(state) };

	if (!active_sound.has_value()) {
		return;
	}

	auto& audio{ GetScene().ctx().audio };

	auto play_sound = [&audio](Audio sound) {
		auto& asset_name{ sound.GetEntity().Get<impl::AssetName>().value };
		audio.Play(asset_name);
	};

	auto stop_sound = [&audio](Audio sound) {
		auto& asset_name{ sound.GetEntity().Get<impl::AssetName>().value };
		audio.Stop(asset_name);
	};

	if (auto sounds{ TryGet<ButtonSounds>() }; sounds && sounds->exclusive) {
		for (const auto& sound : sounds->states) {
			if (sound.has_value()) {
				stop_sound(sound.value());
			}
		}
	}

	play_sound(active_sound.value());
}

void Button::PlayAnimation(ButtonState state) const {
	(void)state;

	auto& button{ const_cast<Button&>(*this) };
	button.RefreshDirty();

	auto entity{ FindPart(impl::ButtonPart::Sprite) };

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

Button& Button::RemovePart(impl::ButtonPart part, ButtonVisualState state) {
	auto entity{ FindPart(part) };

	if (!entity.has_value()) {
		return *this;
	}

	bool any_defined{ false };

	auto has_any_defined = [](const auto& states) {
		return std::ranges::any_of(states, [](const auto& visual) { return visual.defined; });
	};

	switch (part) {
		case impl::ButtonPart::Background:
		case impl::ButtonPart::Border:	   {
			auto& visuals{ GetShapeVisuals(entity.value(), part) };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);
			break;
		}

		case impl::ButtonPart::Text: {
			auto& visuals{ entity->Get<ButtonTextVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);
			break;
		}

		case impl::ButtonPart::Sprite: {
			auto& visuals{ entity->Get<ButtonSpriteVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			visuals.applied_animation_state.reset();
			visuals.transient_animation = false;
			any_defined					= has_any_defined(visuals.states);

			Get<impl::ButtonData>().visual_lock.reset();
			break;
		}

		default: PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}

	if (!any_defined) {
		entity->Destroy();
	} else {
		MarkDirty(DirtyForPart(part));
		RefreshDirty();
	}

	return *this;
}

Button& Button::RemoveParts(impl::ButtonPart part) {
	if (part == impl::ButtonPart::Sprite) {
		Get<impl::ButtonData>().visual_lock.reset();
	}

	if (auto entity{ FindPart(part) }) {
		entity->Destroy();
	}

	MarkDirty(DirtyForPart(part));

	return *this;
}

ButtonShape::ButtonShape(Button button, impl::ButtonPart part, ButtonVisualState state) :
	button_{ button }, part{ part }, state{ state } {
	PTGN_ASSERT(
		part == impl::ButtonPart::Background || part == impl::ButtonPart::Border,
		"ButtonShape can only edit background or border parts"
	);

	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();
}

ButtonShape& ButtonShape::Size(V2_float size) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;
	visual.size	   = size;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Size(float radius) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;
	visual.size	   = radius;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearSize() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.size.reset();

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Origin(ptgn::Origin origin) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;
	visual.origin  = origin;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearOrigin() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.origin.reset();

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;
	visual.anchor  = anchor;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearAnchor() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.anchor.reset();

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Transform(ptgn::Transform transform) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined	 = true;
	visual.transform = transform;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Color(ptgn::Color color) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined = true;
	visual.color   = color;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearColor() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.color.reset();

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Fill(FillStyle fill_style) {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.defined	  = true;
	visual.fill_style = fill_style;

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearFill() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual.fill_style.reset();

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Clear() {
	auto& visual{ button_.ShapeVisual(part, state) };
	visual = {};

	button_.MarkDirty(DirtyForPart(part));
	button_.RefreshDirty();

	return *this;
}

ButtonBackground::ButtonBackground(Button button, ButtonVisualState state) :
	ButtonShape{ button, impl::ButtonPart::Background, state } {}

ButtonBorder::ButtonBorder(Button button, ButtonVisualState state) :
	ButtonShape{ button, impl::ButtonPart::Border, state } {}

ButtonText::ButtonText(Button button, ButtonVisualState state) : button_{ button }, state{ state } {
	auto& visual{ button_.TextVisual(state) };
	visual.defined = true;

	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();
}

ButtonText& ButtonText::Clear() {
	auto& visual{ button_.TextVisual(state) };
	visual = {};

	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();

	return *this;
}

ButtonText& ButtonText::Select(std::size_t index) {
	auto& styled_text{ StyledTextForEdit() };
	auto& visual{ button_.TextVisual(state) };

	if (index >= styled_text.runs.size()) {
		index = styled_text.runs.size() - 1;
	}

	visual.current_run_index = index;

	return *this;
}

ButtonText& ButtonText::Content(std::string_view content) {
	auto& styled_text{ StyledTextForEdit() };
	auto& visual{ button_.TextVisual(state) };

	if (styled_text.runs.size() == 1 && styled_text.runs.front().text.empty()) {
		visual.current_run_index	  = 0;
		styled_text.runs.front().text = std::string{ content };
	} else {
		auto& run{ styled_text.runs.emplace_back() };
		run.text				 = std::string{ content };
		visual.current_run_index = styled_text.runs.size() - 1;
	}

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Content(StyledText styled_text) {
	auto& visual{ button_.TextVisual(state) };

	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back();
	}

	visual.defined			 = true;
	visual.styled_text		 = std::move(styled_text);
	visual.current_run_index = 0;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearContent() {
	auto& visual{ button_.TextVisual(state) };

	visual.styled_text = StyledText{};
	visual.styled_text->runs.emplace_back();
	visual.current_run_index = 0;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Box(TextBox box) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;
	visual.box	   = std::move(box);

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearBox() {
	auto& visual{ button_.TextVisual(state) };
	visual.box.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Origin(ptgn::Origin origin) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;
	visual.origin  = origin;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearOrigin() {
	auto& visual{ button_.TextVisual(state) };
	visual.origin.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;
	visual.anchor  = anchor;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearAnchor() {
	auto& visual{ button_.TextVisual(state) };
	visual.anchor.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Transform(ptgn::Transform transform) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined	 = true;
	visual.transform = transform;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::AutoBox(bool enabled) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined	= true;
	visual.auto_box = enabled;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearAutoBox() {
	auto& visual{ button_.TextVisual(state) };
	visual.auto_box.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Padding(ptgn::Padding padding) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;
	visual.padding = padding;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearPadding() {
	auto& visual{ button_.TextVisual(state) };
	visual.padding.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Font(std::string_view font) {
	auto& run{ CurrentRun() };

	if (run.font != font) {
		run.font = std::string{ font };
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Color(ptgn::Color color) {
	auto& run{ CurrentRun() };

	if (run.style.color != color) {
		run.style.color = color;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Size(float font_size) {
	auto& run{ CurrentRun() };

	if (!NearlyEqual(run.style.size, font_size)) {
		run.style.size = font_size;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Style(FontStyle flags) {
	auto& run{ CurrentRun() };

	if (run.style.flags != flags) {
		run.style.flags = flags;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Bold(bool enabled, float weight) {
	auto& run{ CurrentRun() };

	bool was_enabled{ HasFontFlag(run.style.flags, FontStyle::Bold) };
	bool changed{ was_enabled != enabled ||
				  (enabled && !NearlyEqual(run.style.bold_weight, weight)) };

	run.style.flags		  = SetFontFlag(run.style.flags, FontStyle::Bold, enabled);
	run.style.bold_weight = weight;

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Italic(bool enabled) {
	auto& run{ CurrentRun() };

	if (HasFontFlag(run.style.flags, FontStyle::Italic) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Italic, enabled);
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Underline(bool enabled) {
	auto& run{ CurrentRun() };

	if (HasFontFlag(run.style.flags, FontStyle::Underline) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Underline, enabled);
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Strikethrough(bool enabled) {
	auto& run{ CurrentRun() };

	if (HasFontFlag(run.style.flags, FontStyle::Strikethrough) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Strikethrough, enabled);
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Outline(ptgn::Color color, float width, float softness) {
	auto& run{ CurrentRun() };

	DistanceFieldLayerStyle outline{ .color = color, .width = width, .softness = softness };

	if (run.style.sdf.outline != outline) {
		run.style.sdf.outline = outline;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

ButtonText& ButtonText::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	auto& run{ CurrentRun() };

	DistanceFieldLayerStyle shadow{ .color = color, .width = width, .softness = softness };

	if (run.style.sdf.shadow != shadow || run.style.sdf.shadow_offset != offset) {
		run.style.sdf.shadow		= shadow;
		run.style.sdf.shadow_offset = offset;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::OuterGlow(ptgn::Color color, float width, float softness) {
	auto& run{ CurrentRun() };

	DistanceFieldLayerStyle outer_glow{ .color = color, .width = width, .softness = softness };

	if (run.style.sdf.outer_glow != outer_glow) {
		run.style.sdf.outer_glow = outer_glow;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::InnerGlow(ptgn::Color color, float width, float softness) {
	auto& run{ CurrentRun() };

	DistanceFieldLayerStyle inner_glow{ .color = color, .width = width, .softness = softness };

	if (run.style.sdf.inner_glow != inner_glow) {
		run.style.sdf.inner_glow = inner_glow;
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::ClearSdfEffects() {
	auto& run{ CurrentRun() };

	if (run.style.sdf != DistanceFieldStyle{}) {
		run.style.sdf = {};
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	auto& run{ CurrentRun() };

	GlyphEffectStyle effect{
		.type	   = type,
		.amplitude = amplitude,
		.frequency = frequency,
		.speed	   = speed,
		.phase	   = phase,
	};

	if (run.style.effect != effect) {
		run.style.effect = effect;
		MarkTextDirty();
	}

	return *this;
}

StyledText& ButtonText::StyledTextForEdit() {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;

	if (!visual.styled_text.has_value()) {
		visual.styled_text = button_.GetTextFallback(state);
	}

	EnsureStyledTextHasRun(visual.styled_text.value());

	if (visual.current_run_index >= visual.styled_text->runs.size()) {
		visual.current_run_index = visual.styled_text->runs.size() - 1;
	}

	return visual.styled_text.value();
}

TextRun& ButtonText::CurrentRun() {
	auto& styled_text{ StyledTextForEdit() };
	auto& visual{ button_.TextVisual(state) };

	PTGN_ASSERT(
		visual.current_run_index < styled_text.runs.size(), "Invalid current text run index"
	);

	return styled_text.runs[visual.current_run_index];
}

void ButtonText::MarkTextDirty() {
	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();
}

ButtonSprite::ButtonSprite(Button button, ButtonVisualState state) :
	button_{ button }, state{ state } {
	auto& visual{ button_.SpriteVisual(state) };
	visual.defined = true;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();
}

ButtonSprite& ButtonSprite::Texture(std::string_view texture_key) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.texture = std::string{ texture_key };

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearTexture() {
	auto& visual{ button_.SpriteVisual(state) };
	visual.texture.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Origin(ptgn::Origin origin) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.origin  = origin;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearOrigin() {
	auto& visual{ button_.SpriteVisual(state) };
	visual.origin.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.anchor  = anchor;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearAnchor() {
	auto& visual{ button_.SpriteVisual(state) };
	visual.anchor.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Transform(ptgn::Transform transform) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined	 = true;
	visual.transform = transform;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Size(V2_float size) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.size	   = size;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearSize() {
	auto& visual{ button_.SpriteVisual(state) };
	visual.size.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Tint(ptgn::Color tint) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.tint	   = tint;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearTint() {
	auto& visual{ button_.SpriteVisual(state) };
	visual.tint.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Clear() {
	auto& visual{ button_.SpriteVisual(state) };
	visual = {};

	auto entity{ button_.FindPart(impl::ButtonPart::Sprite) };

	if (entity.has_value()) {
		auto& visuals{ entity->Get<ButtonSpriteVisuals>() };
		visuals.applied_animation_state.reset();
		visuals.transient_animation = false;
		entity->Remove<impl::ButtonAnimationPart>();
	}

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonAnimation::ButtonAnimation(Button button, ButtonVisualState state) :
	ButtonSprite{ button, state } {}

ButtonAnimation& ButtonAnimation::Texture(std::string_view texture_key) {
	ButtonSprite::Texture(texture_key);
	return *this;
}

ButtonAnimation& ButtonAnimation::ClearTexture() {
	ButtonSprite::ClearTexture();
	return *this;
}

ButtonAnimation& ButtonAnimation::Origin(ptgn::Origin origin) {
	ButtonSprite::Origin(origin);
	return *this;
}

ButtonAnimation& ButtonAnimation::ClearOrigin() {
	ButtonSprite::ClearOrigin();
	return *this;
}

ButtonAnimation& ButtonAnimation::Anchor(ptgn::Origin anchor) {
	ButtonSprite::Anchor(anchor);
	return *this;
}

ButtonAnimation& ButtonAnimation::ClearAnchor() {
	ButtonSprite::ClearAnchor();
	return *this;
}

ButtonAnimation& ButtonAnimation::Transform(ptgn::Transform transform) {
	ButtonSprite::Transform(transform);
	return *this;
}

ButtonAnimation& ButtonAnimation::Size(V2_float size) {
	ButtonSprite::Size(size);
	return *this;
}

ButtonAnimation& ButtonAnimation::ClearSize() {
	ButtonSprite::ClearSize();
	return *this;
}

ButtonAnimation& ButtonAnimation::Tint(ptgn::Color tint) {
	ButtonSprite::Tint(tint);
	return *this;
}

ButtonAnimation& ButtonAnimation::ClearTint() {
	ButtonSprite::ClearTint();
	return *this;
}

ButtonAnimation& ButtonAnimation::Config(AnimationConfig config, ButtonAnimationOptions options) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined			 = true;
	visual.animation		 = std::move(config);
	visual.animation_options = options;

	auto entity{ button_.EnsurePart(impl::ButtonPart::Sprite) };
	auto& visuals{ entity.Get<ButtonSpriteVisuals>() };
	visuals.applied_animation_state.reset();

	if (!HasScript<impl::ButtonAnimationCompleteScript>(entity)) {
		AddScript<impl::ButtonAnimationCompleteScript>(entity, button_);
	}

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonAnimation& ButtonAnimation::StaticFrame(AnimationConfig config, std::size_t frame) {
	return Config(
		std::move(config), ButtonAnimationOptions{
							   .playback	 = ButtonAnimationPlayback::StaticFrame,
							   .static_frame = frame,
						   }
	);
}

ButtonAnimation& ButtonAnimation::ClearConfig() {
	auto& visual{ button_.SpriteVisual(state) };

	visual.animation.reset();
	visual.animation_options.reset();

	auto entity{ button_.FindPart(impl::ButtonPart::Sprite) };

	if (entity.has_value()) {
		auto& visuals{ entity->Get<ButtonSpriteVisuals>() };
		visuals.applied_animation_state.reset();
		entity->Remove<impl::ButtonAnimationPart>();
	}

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonAnimation& ButtonAnimation::Clear() {
	ButtonSprite::Clear();
	return *this;
}

Button CreateButton(Scene& scene, Transform transform, Origin origin) {
	ButtonDesc desc;

	desc.origin = origin;

	return CreateButton(scene, transform, desc);
}

Button CreateButton(Scene& scene, Transform transform, V2_float size, Origin origin) {
	return CreateButton(scene, transform, ButtonDesc{ .size = size, .origin = origin });
}

Button CreateButton(Scene& scene, Transform transform, float radius, Origin origin) {
	return CreateButton(scene, transform, ButtonDesc{ .size = radius, .origin = origin });
}

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc) {
	auto resolved_desc{ desc };

	if (resolved_desc.move.has_value()) {
		DisableButtonTextAutoBoxForMove(resolved_desc);
	}

	auto button{ CreateBaseButton(scene, transform, resolved_desc.origin) };

	if (resolved_desc.size.has_value()) {
		std::visit(
			[&button]<typename T>(const T& value) { button.Size(value); },
			resolved_desc.size.value()
		);
	}

	SetUI(button, resolved_desc.ui_layer);
	button.SetEnabled(resolved_desc.enabled, resolved_desc.enabled, true);

	ApplyButtonDescVisuals(button, resolved_desc);
	ApplyButtonEffects(button, resolved_desc);

	button.RefreshDirty();

	return button;
}

Button CreateButton(Scene& scene, Transform transform, V2_float size, const ButtonConfig& config) {
	auto desc{ ToButtonDesc(config) };

	desc.size = std::variant<V2_float, float>{ size };

	return CreateButton(scene, transform, desc);
}

Button CreateAnimatedButton(Scene& scene, Transform transform, const AnimatedButtonConfig& config) {
	return CreateButton(scene, transform, ToButtonDesc(config));
}

} // namespace ptgn
