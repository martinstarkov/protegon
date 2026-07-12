#include "runtime/ui/button.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <span>
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
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
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

constexpr Color GetDefaultBackgroundColor(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;

		case Idle:			[[fallthrough]];
		case Disabled:		[[fallthrough]];
		case Toggled:		return kDefaultIdleButtonBackgroundColor;

		case Hover:			[[fallthrough]];
		case DisabledHover: [[fallthrough]];
		case ToggledHover:	return kDefaultHoverButtonBackgroundColor;

		case Press:			[[fallthrough]];
		case DisabledPress: [[fallthrough]];
		case ToggledPress:	return kDefaultPressButtonBackgroundColor;

		default:			PTGN_ERROR("Unknown ButtonVisualState: ", std::to_underlying(state));
	}
}

constexpr Color GetDefaultBorderColor(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;

		case Idle:			[[fallthrough]];
		case Disabled:		[[fallthrough]];
		case Toggled:		return kDefaultIdleButtonBorderColor;

		case Hover:			[[fallthrough]];
		case DisabledHover: [[fallthrough]];
		case ToggledHover:	return kDefaultHoverButtonBorderColor;

		case Press:			[[fallthrough]];
		case DisabledPress: [[fallthrough]];
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

ButtonVisualState PressVisualState(Button button) {
	if (!button.IsEnabled(false)) {
		return ButtonVisualState::DisabledPress;
	}

	if (ToggleButton{ button }.IsToggled()) {
		return ButtonVisualState::ToggledPress;
	}

	return ButtonVisualState::Press;
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
			 button.GetOrDefault<Origin>(kDefaultOrigin) };
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

	return { size, text_origin };
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
	visual.styled_text.value().runs.emplace_back();
	visual.anchor	= Origin::Center;
	visual.origin	= Origin::Center;
	visual.auto_box = true;
	visual.padding	= Padding{};
}

void ResetButtonAnimation(Entity animation_entity) {
	if (!animation_entity.Has<impl::AnimationData>()) {
		return;
	}

	Animation animation{ animation_entity };

	std::size_t static_frame{ 0 };

	if (auto animation_part{ animation.TryGet<impl::ButtonAnimationPart>() }) {
		static_frame = animation_part->options.static_frame;
	}

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
		  config.origin.has_value() || config.anchor.has_value() || config.size.has_value() ||
		  config.transform.has_value())) {
		return;
	}

	auto apply_sprite_state = [&config, &visuals](
								  ButtonVisualState state,
								  const std::optional<std::string>& texture,
								  const std::optional<Color>& tint
							  ) {
		auto& visual{ visuals.states[std::to_underlying(state)] };

		visual.defined = true;

		if (config.origin.has_value()) {
			visual.origin = config.origin;
		}
		if (config.anchor.has_value()) {
			visual.anchor = config.anchor;
		}

		if (config.transform.has_value()) {
			visual.transform = config.transform;
		}

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

void ApplyButtonSoundConfig(ButtonSounds& sounds, const ButtonSoundConfig& config) {
	auto set_button_sound = [&sounds](auto state, const auto& sound) {
		if (sound.has_value()) {
			sounds.states[std::to_underlying(state)] = sound;
		}
	};

	set_button_sound(ButtonVisualState::Idle, config.idle);
	set_button_sound(ButtonVisualState::Hover, config.hover);
	set_button_sound(ButtonVisualState::Press, config.press);
	set_button_sound(ButtonVisualState::Disabled, config.disabled);
	set_button_sound(ButtonVisualState::DisabledHover, config.disabled_hover);
	set_button_sound(ButtonVisualState::DisabledPress, config.disabled_press);
	set_button_sound(ButtonVisualState::Toggled, config.toggled);
	set_button_sound(ButtonVisualState::ToggledHover, config.toggled_hover);
	set_button_sound(ButtonVisualState::ToggledPress, config.toggled_press);
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

std::optional<Text> FindButtonText(Button button) {
	if (!HasChildren(button)) {
		return std::nullopt;
	}

	auto children{ GetChildren(button) };
	auto it{ std::ranges::find_if(children, [](Entity child) {
		return child.Has<ButtonTextVisuals>();
	}) };

	return it != children.end() ? std::optional<Text>{ Text{ *it } } : std::nullopt;
}

void ApplyButtonShapeVisual(ButtonShape shape, const ButtonShapeVisual& visual) {
	if (visual.size.has_value()) {
		std::visit(
			[&shape]<typename T>(const T& value) { shape.Size(value); }, visual.size.value()
		);
	}
	if (visual.origin.has_value()) {
		shape.Origin(visual.origin.value());
	}
	if (visual.anchor.has_value()) {
		shape.Anchor(visual.anchor.value());
	}
	if (visual.transform.has_value()) {
		shape.Transform(visual.transform.value());
	}
	if (visual.color.has_value()) {
		shape.Color(visual.color.value());
	}
	if (visual.fill_style.has_value()) {
		shape.Fill(visual.fill_style.value());
	}
}

void ApplyButtonTextVisual(ButtonText text, const ButtonTextVisual& visual) {
	if (visual.styled_text.has_value()) {
		text.Content(visual.styled_text.value());
	}
	if (visual.box.has_value()) {
		text.Box(visual.box.value());
	}
	if (visual.origin.has_value()) {
		text.Origin(visual.origin.value());
	}
	if (visual.anchor.has_value()) {
		text.Anchor(visual.anchor.value());
	}
	if (visual.transform.has_value()) {
		text.Transform(visual.transform.value());
	}
	if (visual.auto_box.has_value()) {
		text.AutoBox(visual.auto_box.value());
	}
	if (visual.padding.has_value()) {
		text.Padding(visual.padding.value());
	}
}

void ApplyButtonSpriteVisual(
	Button button, ButtonVisualState state, const ButtonSpriteVisual& visual
) {
	auto apply_common = [](auto& sprite, const ButtonSpriteVisual& visual) {
		if (visual.texture.has_value()) {
			sprite.Texture(visual.texture.value());
		}
		if (visual.origin.has_value()) {
			sprite.Origin(visual.origin.value());
		}
		if (visual.anchor.has_value()) {
			sprite.Anchor(visual.anchor.value());
		}
		if (visual.transform.has_value()) {
			sprite.Transform(visual.transform.value());
		}
		if (visual.size.has_value()) {
			sprite.Size(visual.size.value());
		}
		if (visual.tint.has_value()) {
			sprite.Tint(visual.tint.value());
		}
	};

	if (visual.animation.has_value()) {
		auto animation{ button.Animation(state) };

		apply_common(animation, visual);

		animation.Config(
			visual.animation.value(), visual.animation_options.value_or(ButtonAnimationOptions{})
		);

		return;
	}

	auto sprite{ button.Sprite(state) };

	apply_common(sprite, visual);
}

void ApplyButtonDescVisuals(Button button, const ButtonDesc& desc) {
	for (auto state : magic_enum::enum_values<ButtonVisualState>()) {
		auto index{ std::to_underlying(state) };

		const auto& background{ desc.background.states[index] };
		if (background.defined) {
			ApplyButtonShapeVisual(button.Background(state), background);
		}

		const auto& border{ desc.border.states[index] };
		if (border.defined) {
			ApplyButtonShapeVisual(button.Border(state), border);
		}

		const auto& text{ desc.text.states[index] };
		if (text.defined) {
			ApplyButtonTextVisual(button.Text(state), text);
		}

		const auto& sprite{ desc.sprite.states[index] };
		if (sprite.defined) {
			ApplyButtonSpriteVisual(button, state, sprite);
		}

		const auto& sound{ desc.sounds.states[index] };
		if (sound.has_value()) {
			button.Sound(
				std::optional<std::string_view>{ std::string_view{ sound.value() } }, state
			);
		}
	}
}

void ApplyButtonMoveConfig(Button button, const MoveButtonConfig& move) {
	button.OnHoverStart([move](Button button) {
		auto text{ FindButtonText(button) };

		if (!text.has_value()) {
			return;
		}

		std::vector<Text> texts{ text.value() };

		TranslateTo<Text>(texts, move.offset, move.duration, move.ease);
	});

	button.OnHoverStop([move](Button button) {
		auto text{ FindButtonText(button) };

		if (!text.has_value()) {
			return;
		}

		std::vector<Text> texts{ text.value() };

		TranslateTo<Text>(texts, V2_float{}, move.duration, move.ease);
	});
}

void ApplyButtonScaleConfig(Button button, const ScaleButtonConfig& scale) {
	auto text{ FindButtonText(button) };
	auto starting_scale{ text.has_value() ? GetScale(text.value()) : V2_float{ 1.0f, 1.0f } };

	button.OnHoverStart([scale](Button button) {
		auto text{ FindButtonText(button) };

		if (!text.has_value()) {
			return;
		}

		std::vector<Text> texts{ text.value() };

		ScaleTo<Text>(texts, V2_float{ scale.scale }, scale.duration, scale.ease);
	});

	button.OnHoverStop([scale, starting_scale](Button button) {
		auto text{ FindButtonText(button) };

		if (!text.has_value()) {
			return;
		}

		std::vector<Text> texts{ text.value() };
		std::vector<V2_float> target_scales{ starting_scale };

		ScaleTo<Text>(texts, target_scales, scale.duration, scale.ease);
	});
}

void ApplyButtonEffects(Button button, const ButtonDesc& desc) {
	if (desc.move.has_value()) {
		ApplyButtonMoveConfig(button, desc.move.value());
	}

	if (desc.scale.has_value()) {
		ApplyButtonScaleConfig(button, desc.scale.value());
	}
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

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Button button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::AnimationComplete>([this]() {
		if (!button || !entity.Has<ButtonSpriteVisuals>()) {
			return;
		}

		auto& data{ button.Get<ButtonData>() };

		if (!data.visual_lock.has_value()) {
			return;
		}

		const auto& visuals{ entity.Get<ButtonSpriteVisuals>() };

		ButtonVisualState animation_state;
		auto animation{ ResolveProperty(
			visuals.states, data.visual_lock.value().state, &ButtonSpriteVisual::animation,
			&animation_state
		) };

		if (!animation) {
			return;
		}

		data.visual_lock.reset();

		button.MarkDirty(ButtonDirty::All);
		button.RefreshDirty();
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
		// TODO: Come up with a better way to sync button origins to their parts. Keep in mind that
		// it may not be as simple as looping over the button parts because the origin may influence
		// things like transform, etc.
		Button{ entity }.MarkDirty(impl::ButtonDirty::All);
		Button{ entity }.RefreshDirty();
	}
}

} // namespace impl

Button::Button(Entity entity) : Entity{ entity } {}

bool Button::IsEnabled(bool check_for_hover_enabled) const {
	const auto& button{ Get<impl::ButtonData>() };

	return check_for_hover_enabled ? button.hover_enabled : button.press_enabled;
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

ButtonVisualState Button::GetVisualState(ButtonState state, bool check_for_visual_lock) const {
	if (!IsEnabled(false)) {
		return DisabledVisualState(state);
	}

	if (const auto& button{ Get<impl::ButtonData>() };
		check_for_visual_lock && button.visual_lock.has_value()) {
		return button.visual_lock.value().state;
	}

	if (ToggleButton{ *this }.IsToggled()) {
		return ToggledVisualState(state);
	}

	return NormalVisualState(state);
}

ButtonVisualState Button::GetVisualState() const {
	return GetVisualState(GetState(), true);
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

Button& Button::Press() {
	if (!IsEnabled(false)) {
		return *this;
	}

	auto& button{ Get<impl::ButtonData>() };

	if (button.visual_lock.has_value() && button.visual_lock.value().block_press) {
		return *this;
	}

	auto press_visual_state{ PressVisualState(*this) };
	auto sprite{ FindPart(impl::ButtonPart::Sprite) };
	ButtonAnimationOptions animation_options;
	auto has_animation{ false };

	if (sprite) {
		auto& visuals{ sprite.Get<ButtonSpriteVisuals>() };

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
		if (animation_options.playback == ButtonAnimationPlayback::StaticFrame) {
			ApplySpriteVisual(press_visual_state);

			if (sprite && sprite.Has<impl::AnimationData>()) {
				ResetButtonAnimation(sprite);
			}
		} else {
			button.visual_lock = {
				.state		 = press_visual_state,
				.block_press = animation_options.block_press,
			};

			MarkDirty(impl::ButtonDirty::All);
			RefreshDirty();

			if (sprite && sprite.Has<impl::AnimationData>()) {
				ResetButtonAnimation(sprite);
				ptgn::Animation{ sprite }.Start(true);
			}
		}
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

	if (!entity) {
		return *this;
	}

	auto& visuals{ entity.Get<ButtonSpriteVisuals>() };

	for (auto& visual : visuals.states) {
		visual.animation.reset();
		visual.animation_options.reset();
	}

	entity.Remove<impl::ButtonAnimationPart>();

	MarkDirty(impl::ButtonDirty::Sprite);
	RefreshDirty();

	return *this;
}

Button& Button::RemoveAnimation(ButtonVisualState state) {
	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity) {
		return *this;
	}

	auto& visuals{ entity.Get<ButtonSpriteVisuals>() };
	auto& visual{ visuals.states[std::to_underlying(state)] };

	visual.animation.reset();
	visual.animation_options.reset();

	entity.Remove<impl::ButtonAnimationPart>();

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

	slot.emplace(sound_key.value());

	return *this;
}

Button& Button::Sounds(
	std::optional<std::string_view> hover, std::optional<std::string_view> press
) {
	Sound(hover, ButtonVisualState::Hover);
	Sound(press, ButtonVisualState::Press);
	return *this;
}

Button& Button::RemoveSound(ButtonVisualState state) {
	if (auto sounds{ TryGet<ButtonSounds>() }) {
		sounds->states[std::to_underlying(state)].reset();
	}

	return *this;
}

Button& Button::RemoveSounds() {
	if (auto sounds{ TryGet<ButtonSounds>() }) {
		sounds->states = {};
	}

	return *this;
}

Button& Button::ExclusiveAudio(bool enabled) {
	TryAdd<ButtonSounds>().exclusive = enabled;
	return *this;
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

Entity Button::EnsurePart(impl::ButtonPart part) {
	auto existing{ FindPart(part) };

	if (existing) {
		return existing;
	}

	Entity entity;

	switch (part) {
		case impl::ButtonPart::Background: {
			entity = CreateRect(GetScene(), {}, {}, {});
			entity.Add<ButtonBackgroundVisuals>();
			break;
		}

		case impl::ButtonPart::Border: {
			entity = CreateRect(GetScene(), {}, {}, {});
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

	SetParent(entity, *this);
	SetUI(entity, true);

	return entity;
}

Entity Button::FindPart(impl::ButtonPart part) const {
	if (!HasChildren(*this)) {
		return {};
	}

	auto children{ GetChildren(*this) };
	auto it{ std::ranges::find_if(children, [part](Entity entity) {
		return IsButtonPart(entity, part);
	}) };

	if (it == children.end()) {
		return {};
	}

	return *it;
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

	if (!entity) {
		StyledText styled_text;
		styled_text.runs.emplace_back();
		return styled_text;
	}

	const auto& visuals{ entity.Get<ButtonTextVisuals>() };

	if (auto value{ ResolveProperty(visuals.states, state, &ButtonTextVisual::styled_text) }) {
		return *value;
	}

	StyledText styled_text;
	styled_text.runs.emplace_back();
	return styled_text;
}

void Button::ApplyShapeVisual(impl::ButtonPart part) const {
	auto entity{ FindPart(part) };

	if (!entity) {
		return;
	}

	const auto& visuals{ GetShapeVisuals(entity, part) };
	auto visual_state{ GetVisualState() };

	if (!HasResolvedState(visuals.states, visual_state)) {
		SetVisible(entity, false);
		return;
	}

	bool button_visible{ IsVisible(*this) };
	SetVisible(entity, button_visible);

	auto size{ HasAny<Rect, Circle>() ? GetSize() : std::variant<V2_float, float>{ V2_float{} } };
	auto origin{ GetOrDefault<Origin>(kDefaultOrigin) };
	auto anchor{ GetOrDefault<Origin>(kDefaultOrigin) };

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
		Transform transform{ *value };

		auto button_rect{ GetButtonLocalRect(*this) };
		transform.position += button_rect.GetOriginPoint(anchor);

		entity.Add<Transform>(transform);
		entity.Add<Origin>(origin);
	}

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

	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::color) }) {
		entity.Add<Color>(*value);
	}
	if (auto value{
			ResolveProperty(visuals.states, visual_state, &ButtonShapeVisual::fill_style) }) {
		entity.Add<FillStyle>(*value);
	}
}

void Button::ApplyTextVisual() const {
	auto entity{ FindPart(impl::ButtonPart::Text) };

	if (!entity) {
		return;
	}

	const auto& visuals{ entity.Get<ButtonTextVisuals>() };
	auto visual_state{ GetVisualState() };

	if (!HasResolvedState(visuals.states, visual_state)) {
		SetVisible(entity, false);
		return;
	}

	auto styled_text{
		ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::styled_text)
	};

	if (!styled_text || !styled_text->HasContent()) {
		SetVisible(entity, false);
		return;
	}

	bool button_visible{ IsVisible(*this) };
	SetVisible(entity, button_visible);

	TextBox box{ .style = { .alignment = { .horizontal = std::nullopt, .vertical = std::nullopt } } };
	auto anchor{ Origin::Center };
	auto origin{ anchor };
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

	auto button_rect{ GetButtonLocalRect(*this) };
	auto anchor_position{ button_rect.GetOriginPoint(anchor) };

	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::transform) }) {
		Transform transform{ *value };
		transform.position += anchor_position;
		entity.Add<Transform>(transform);
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::auto_box) }) {
		auto_box = *value;
	}
	if (auto value{ ResolveProperty(visuals.states, visual_state, &ButtonTextVisual::padding) }) {
		padding = *value;
	}

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

	ptgn::Text text{ entity };

	text.Clear();
	text.Content(*styled_text);
	text.Box(box);

	text.Add<Origin>(origin);
}

void Button::ApplySpriteVisual() const {
	ApplySpriteVisual(GetVisualState());
}

void Button::ApplySpriteVisual(ButtonVisualState state) const {
	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity) {
		return;
	}

	auto& visuals{ entity.Get<ButtonSpriteVisuals>() };

	auto visible{ HasResolvedState(visuals.states, state) };

	bool button_visible{ IsVisible(*this) };
	SetVisible(entity, button_visible && visible);

	ptgn::Sprite sprite{ entity };

	if (!visible) {
		if (sprite.Has<impl::AnimationData>()) {
			ptgn::Animation{ sprite }.Stop();
		}

		entity.Remove<impl::ButtonAnimationPart>();
		return;
	}

	std::string texture;
	auto origin{ GetOrDefault<Origin>(kDefaultOrigin) };
	auto anchor{ GetOrDefault<Origin>(kDefaultOrigin) };
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
		Transform transform{ *value };
		transform.position += GetButtonLocalRect(*this).GetOriginPoint(anchor);
		sprite.Add<Transform>(transform);
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
		sprite.Add<TextureKey>(texture);
	}

	sprite.Add<impl::Tint>(tint);
	sprite.Add<Origin>(origin);

	if (size.has_value()) {
		sprite.Add<impl::TextureSize>(size.value());
	} else {
		sprite.Remove<impl::TextureSize>();
	}

	bool has_animation{ animation && animation_state.has_value() };

	if (!has_animation) {
		if (sprite.Has<impl::AnimationData>()) {
			ptgn::Animation{ sprite }.Stop();
		}

		entity.Remove<impl::ButtonAnimationPart>();
		return;
	}

	PTGN_ASSERT(!size.has_value(), "Animations cannot have a custom fixed texture size");

	ptgn::Animation{ sprite }.SetConfig(*animation);

	entity.Add<impl::ButtonAnimationPart>(animation_options);

	if (animation_options.playback == ButtonAnimationPlayback::StaticFrame) {
		ptgn::Animation{ sprite }.Reset();
		ptgn::Animation{ sprite }.SetCurrentFrame(animation_options.static_frame);
	}
}

void Button::PlaySound(ButtonVisualState state) {
	if (!Has<ButtonSounds>()) {
		return;
	}

	const auto& sounds{ Get<ButtonSounds>() };

	auto& audio{ GetScene().ctx().audio };

	auto get_sound = [&sounds](auto state) -> std::optional<std::string> {
		for (auto fallback : GetVisualStateFallbacks(state)) {
			const auto& sound{ sounds.states[std::to_underlying(fallback)] };
			if (sound.has_value()) {
				return sound.value();
			}
		}
		return std::nullopt;
	};

	auto active_sound{ get_sound(state) };

	if (!active_sound.has_value()) {
		return;
	}

	if (sounds.exclusive) {
		for (const auto& sound : sounds.states) {
			if (sound.has_value()) {
				audio.Stop(sound.value());
			}
		}
	}

	audio.Play(active_sound.value());
}

void Button::PlayAnimation(ButtonState state) const {
	auto visual_state{ GetVisualState(state, false) };

	ApplySpriteVisual(visual_state);

	auto entity{ FindPart(impl::ButtonPart::Sprite) };

	if (!entity) {
		return;
	}

	if (!entity.Has<impl::AnimationData, impl::ButtonAnimationPart>()) {
		return;
	}

	const auto& animation_part{ entity.Get<impl::ButtonAnimationPart>() };

	switch (animation_part.options.playback) {
		using enum ButtonAnimationPlayback;

		case StaticFrame: ResetButtonAnimation(entity); break;
		case Play:		  [[fallthrough]];
		case PlayOnce:	  ptgn::Animation{ entity }.Start(true); break;
	}
}

Button& Button::RemovePart(impl::ButtonPart part, ButtonVisualState state) {
	auto entity{ FindPart(part) };

	if (!entity) {
		return *this;
	}

	bool any_defined{ false };

	auto has_any_defined = [](const auto& states) {
		return std::ranges::any_of(states, [](const auto& visual) { return visual.defined; });
	};

	switch (part) {
		case impl::ButtonPart::Background:
		case impl::ButtonPart::Border:	   {
			auto& visuals{ GetShapeVisuals(entity, part) };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);
			break;
		}

		case impl::ButtonPart::Text: {
			auto& visuals{ entity.Get<ButtonTextVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);
			break;
		}

		case impl::ButtonPart::Sprite: {
			auto& visuals{ entity.Get<ButtonSpriteVisuals>() };
			visuals.states[std::to_underlying(state)] = {};
			any_defined								  = has_any_defined(visuals.states);

			Get<impl::ButtonData>().visual_lock.reset();
			break;
		}

		default: PTGN_ERROR("Unsupported button part: ", std::to_underlying(part));
	}

	if (!any_defined) {
		entity.Destroy();
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
		entity.Destroy();
	}

	MarkDirty(DirtyForPart(part));

	return *this;
}

ButtonShape::ButtonShape(ptgn::Button button, impl::ButtonPart part, ButtonVisualState state) :
	button_{ button }, part_{ part }, state_{ state } {
	PTGN_ASSERT(
		part_ == impl::ButtonPart::Background || part_ == impl::ButtonPart::Border,
		"ButtonShape can only edit background or border parts"
	);

	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined = true;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();
}

ButtonShape::operator ptgn::Button() const {
	return button_;
}

ptgn::Button ButtonShape::Button() const {
	return button_;
}

ButtonShape& ButtonShape::Size(V2_float size) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined = true;
	visual.size	   = size;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Size(float radius) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined = true;
	visual.size	   = radius;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearSize() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.size.reset();

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Origin(ptgn::Origin origin) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined = true;
	visual.origin  = origin;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearOrigin() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.origin.reset();

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined = true;
	visual.anchor  = anchor;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearAnchor() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.anchor.reset();

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Transform(ptgn::Transform transform) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined	 = true;
	visual.transform = transform;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Color(ptgn::Color color, ButtonVisualState state) {
	auto& visual{ button_.ShapeVisual(part_, state) };
	visual.defined = true;
	visual.color   = color;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Color(ptgn::Color color) {
	return Color(color, state_);
}

ButtonShape& ButtonShape::Colors(
	std::optional<ptgn::Color> idle, std::optional<ptgn::Color> hover,
	std::optional<ptgn::Color> press
) {
	if (idle.has_value()) {
		Color(idle.value(), ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		Color(hover.value(), ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		Color(press.value(), ButtonVisualState::Press);
	}
	return *this;
}

ButtonShape& ButtonShape::ClearColor() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.color.reset();

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Fill(FillStyle fill_style) {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.defined	  = true;
	visual.fill_style = fill_style;

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::ClearFill() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual.fill_style.reset();

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonShape& ButtonShape::Clear() {
	auto& visual{ button_.ShapeVisual(part_, state_) };
	visual = {};

	button_.MarkDirty(DirtyForPart(part_));
	button_.RefreshDirty();

	return *this;
}

ButtonBackground::ButtonBackground(ptgn::Button button, ButtonVisualState state) :
	ButtonShape{ button, impl::ButtonPart::Background, state } {}

ButtonBorder::ButtonBorder(ptgn::Button button, ButtonVisualState state) :
	ButtonShape{ button, impl::ButtonPart::Border, state } {}

ButtonText::ButtonText(ptgn::Button button, ButtonVisualState state) :
	button_{ button }, state_{ state } {
	auto& visual{ button_.TextVisual(state_) };
	visual.defined = true;

	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();
}

ButtonText::operator ptgn::Button() const {
	return button_;
}

ptgn::Button ButtonText::Button() const {
	return button_;
}

ButtonText& ButtonText::Clear() {
	auto& visual{ button_.TextVisual(state_) };
	visual = {};

	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();

	return *this;
}

ButtonText& ButtonText::Content(std::string_view content) {
	auto& visual{ button_.TextVisual(state_) };

	TextRun run;

	if (visual.styled_text.has_value() && !visual.styled_text.value().runs.empty()) {
		run = visual.styled_text.value().runs.front();
	} else {
		auto fallback{ button_.GetTextFallback(state_) };

		if (!fallback.runs.empty()) {
			run = fallback.runs.front();
		}
	}

	run.text = std::string{ content };

	visual.defined	   = true;
	visual.styled_text = StyledText{};
	visual.styled_text.value().runs.emplace_back(std::move(run));

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Content(StyledText styled_text) {
	auto& visual{ button_.TextVisual(state_) };

	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back();
	}

	visual.defined	   = true;
	visual.styled_text = std::move(styled_text);

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearContent() {
	auto& visual{ button_.TextVisual(state_) };

	if (!visual.styled_text.has_value()) {
		return *this;
	}

	visual.styled_text.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Box(TextBox box) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined = true;
	visual.box	   = std::move(box);

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearBox() {
	auto& visual{ button_.TextVisual(state_) };

	if (!visual.box.has_value()) {
		return *this;
	}

	visual.box.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Align(ptgn::Origin origin) {
	return Align(GetAlignment(origin));
}

ButtonText& ButtonText::Align(Alignment alignment) {
	auto& visual{ button_.TextVisual(state_) };

	bool changed{ !visual.defined };
	visual.defined = true;

	if (!visual.box.has_value()) {
		visual.box = TextBox{};
		changed	   = true;
	}

	if (alignment.horizontal.has_value()) {
		visual.box.value().style.alignment.horizontal = alignment.horizontal;

		changed = true;
	}

	if (alignment.vertical.has_value()) {
		visual.box.value().style.alignment.vertical = alignment.vertical;

		changed = true;
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	return Align({ .horizontal = horizontal, .vertical = vertical });
}

ButtonText& ButtonText::HorizontalAlign(ptgn::HorizontalAlign align) {
	return Align({ .horizontal = align, .vertical = std::nullopt });
}

ButtonText& ButtonText::VerticalAlign(ptgn::VerticalAlign align) {
	return Align({ .horizontal = std::nullopt, .vertical = align });
}

ButtonText& ButtonText::ClearAlignment() {
	auto& visual{ button_.TextVisual(state_) };

	if (visual.box.has_value()) {
		visual.box.value().style.alignment.horizontal.reset();
		visual.box.value().style.alignment.vertical.reset();

		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Origin(ptgn::Origin origin) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined = true;
	visual.origin  = origin;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearOrigin() {
	auto& visual{ button_.TextVisual(state_) };
	visual.origin.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined = true;
	visual.anchor  = anchor;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearAnchor() {
	auto& visual{ button_.TextVisual(state_) };
	visual.anchor.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Transform(ptgn::Transform transform) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined	 = true;
	visual.transform = transform;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::AutoBox(bool enabled) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined	= true;
	visual.auto_box = enabled;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearAutoBox() {
	auto& visual{ button_.TextVisual(state_) };
	visual.auto_box.reset();

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::Padding(ptgn::Padding padding) {
	auto& visual{ button_.TextVisual(state_) };

	visual.defined = true;
	visual.padding = padding;

	MarkTextDirty();

	return *this;
}

ButtonText& ButtonText::ClearPadding() {
	auto& visual{ button_.TextVisual(state_) };
	visual.padding.reset();

	MarkTextDirty();

	return *this;
}

StyledText& ButtonText::StyledTextForEdit() {
	return StyledTextForEdit(state_);
}

StyledText& ButtonText::StyledTextForEdit(ButtonVisualState state) {
	auto& visual{ button_.TextVisual(state) };

	visual.defined = true;

	if (!visual.styled_text.has_value()) {
		visual.styled_text = button_.GetTextFallback(state);
	}

	if (visual.styled_text.value().runs.empty()) {
		visual.styled_text.value().runs.emplace_back();
	}

	return visual.styled_text.value();
}

ButtonText& ButtonText::Font(std::string_view font) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.font != font) {
			run.font = std::string{ font };
			changed	 = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Color(ptgn::Color color, ButtonVisualState state) {
	auto& styled_text{ StyledTextForEdit(state) };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.color != color) {
			run.style.color = color;
			changed			= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Color(ptgn::Color color) {
	return Color(color, state_);
}

ButtonText& ButtonText::Colors(
	std::optional<ptgn::Color> idle, std::optional<ptgn::Color> hover,
	std::optional<ptgn::Color> press
) {
	if (idle.has_value()) {
		Color(idle.value(), ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		Color(hover.value(), ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		Color(press.value(), ButtonVisualState::Press);
	}
	return *this;
}

ButtonText& ButtonText::Size(float font_size) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (!NearlyEqual(run.style.size, font_size)) {
			run.style.size = font_size;
			changed		   = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Style(FontStyle flags) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.flags != flags) {
			run.style.flags = flags;
			changed			= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Bold(bool enabled, float weight) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		bool was_enabled{ HasFontFlag(run.style.flags, FontStyle::Bold) };
		bool run_changed{ was_enabled != enabled ||
						  (enabled && !NearlyEqual(run.style.bold_weight, weight)) };

		run.style.flags		  = SetFontFlag(run.style.flags, FontStyle::Bold, enabled);
		run.style.bold_weight = weight;

		changed = changed || run_changed;
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Italic(bool enabled) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (HasFontFlag(run.style.flags, FontStyle::Italic) != enabled) {
			run.style.flags = SetFontFlag(run.style.flags, FontStyle::Italic, enabled);
			changed			= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Underline(bool enabled) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (HasFontFlag(run.style.flags, FontStyle::Underline) != enabled) {
			run.style.flags = SetFontFlag(run.style.flags, FontStyle::Underline, enabled);
			changed			= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Strikethrough(bool enabled) {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (HasFontFlag(run.style.flags, FontStyle::Strikethrough) != enabled) {
			run.style.flags = SetFontFlag(run.style.flags, FontStyle::Strikethrough, enabled);
			changed			= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Outline(ptgn::Color color, float width, float softness) {
	auto& styled_text{ StyledTextForEdit() };

	DistanceFieldLayerStyle outline{ .color = color, .width = width, .softness = softness };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.sdf.outline != outline) {
			run.style.sdf.outline = outline;
			changed				  = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

ButtonText& ButtonText::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	auto& styled_text{ StyledTextForEdit() };

	DistanceFieldLayerStyle shadow{ .color = color, .width = width, .softness = softness };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.sdf.shadow != shadow || run.style.sdf.shadow_offset != offset) {
			run.style.sdf.shadow		= shadow;
			run.style.sdf.shadow_offset = offset;
			changed						= true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::OuterGlow(ptgn::Color color, float width, float softness) {
	auto& styled_text{ StyledTextForEdit() };

	DistanceFieldLayerStyle outer_glow{ .color = color, .width = width, .softness = softness };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.sdf.outer_glow != outer_glow) {
			run.style.sdf.outer_glow = outer_glow;
			changed					 = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::InnerGlow(ptgn::Color color, float width, float softness) {
	auto& styled_text{ StyledTextForEdit() };

	DistanceFieldLayerStyle inner_glow{ .color = color, .width = width, .softness = softness };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.sdf.inner_glow != inner_glow) {
			run.style.sdf.inner_glow = inner_glow;
			changed					 = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::ClearSdfEffects() {
	auto& styled_text{ StyledTextForEdit() };

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.sdf != DistanceFieldStyle{}) {
			run.style.sdf = {};
			changed		  = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

ButtonText& ButtonText::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	auto& styled_text{ StyledTextForEdit() };

	GlyphEffectStyle effect{
		.type	   = type,
		.amplitude = amplitude,
		.frequency = frequency,
		.speed	   = speed,
		.phase	   = phase,
	};

	bool changed{ false };

	for (auto& run : styled_text.runs) {
		if (run.style.effect != effect) {
			run.style.effect = effect;
			changed			 = true;
		}
	}

	if (changed) {
		MarkTextDirty();
	}

	return *this;
}

void ButtonText::MarkTextDirty() {
	button_.MarkDirty(impl::ButtonDirty::Text);
	button_.RefreshDirty();
}

ButtonSprite::ButtonSprite(ptgn::Button button, ButtonVisualState state) :
	button_{ button }, state_{ state } {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.defined = true;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();
}

ButtonSprite::operator ptgn::Button() const {
	return button_;
}

ptgn::Button ButtonSprite::Button() const {
	return button_;
}

ButtonSprite& ButtonSprite::Texture(std::string_view texture_key, ButtonVisualState state) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined = true;
	visual.texture = std::string{ texture_key };

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Texture(std::string_view texture_key) {
	return Texture(texture_key, state_);
}

ButtonSprite& ButtonSprite::Textures(
	std::optional<std::string_view> idle, std::optional<std::string_view> hover,
	std::optional<std::string_view> press
) {
	if (idle.has_value()) {
		Texture(idle.value(), ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		Texture(hover.value(), ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		Texture(press.value(), ButtonVisualState::Press);
	}
	return *this;
}

ButtonSprite& ButtonSprite::ClearTexture() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.texture.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Origin(ptgn::Origin origin) {
	auto& visual{ button_.SpriteVisual(state_) };

	visual.defined = true;
	visual.origin  = origin;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearOrigin() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.origin.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Anchor(ptgn::Origin anchor) {
	auto& visual{ button_.SpriteVisual(state_) };

	visual.defined = true;
	visual.anchor  = anchor;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearAnchor() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.anchor.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Transform(ptgn::Transform transform) {
	auto& visual{ button_.SpriteVisual(state_) };

	visual.defined	 = true;
	visual.transform = transform;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Size(V2_float size) {
	auto& visual{ button_.SpriteVisual(state_) };

	visual.defined = true;
	visual.size	   = size;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearSize() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.size.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Tint(ptgn::Color tint) {
	auto& visual{ button_.SpriteVisual(state_) };

	visual.defined = true;
	visual.tint	   = tint;

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::ClearTint() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual.tint.reset();

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonSprite& ButtonSprite::Clear() {
	auto& visual{ button_.SpriteVisual(state_) };
	visual = {};

	if (auto entity{ button_.FindPart(impl::ButtonPart::Sprite) }) {
		entity.Remove<impl::ButtonAnimationPart>();
	}

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonAnimation::ButtonAnimation(ptgn::Button button, ButtonVisualState state) :
	ButtonSprite{ button, state } {}

ButtonAnimation& ButtonAnimation::Texture(std::string_view texture_key) {
	ButtonSprite::Texture(texture_key);
	return *this;
}

ButtonAnimation& ButtonAnimation::Textures(
	std::optional<std::string_view> idle, std::optional<std::string_view> hover,
	std::optional<std::string_view> press
) {
	ButtonSprite::Textures(idle, hover, press);
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

ButtonAnimation& ButtonAnimation::Config(
	AnimationConfig config, ButtonAnimationOptions options, ButtonVisualState state
) {
	auto& visual{ button_.SpriteVisual(state) };

	visual.defined			 = true;
	visual.animation		 = std::move(config);
	visual.animation_options = options;

	auto entity{ button_.EnsurePart(impl::ButtonPart::Sprite) };

	if (!HasScript<impl::ButtonAnimationCompleteScript>(entity)) {
		AddScript<impl::ButtonAnimationCompleteScript>(entity, button_);
	}

	button_.MarkDirty(impl::ButtonDirty::Sprite);
	button_.RefreshDirty();

	return *this;
}

ButtonAnimation& ButtonAnimation::Config(AnimationConfig config, ButtonAnimationOptions options) {
	return Config(config, options, state_);
}

ButtonAnimation& ButtonAnimation::Configs(
	std::optional<AnimationConfig> idle, std::optional<AnimationConfig> hover,
	std::optional<AnimationConfig> press
) {
	if (idle.has_value()) {
		Config(idle.value(), {}, ButtonVisualState::Idle);
	}
	if (hover.has_value()) {
		Config(hover.value(), {}, ButtonVisualState::Hover);
	}
	if (press.has_value()) {
		Config(press.value(), {}, ButtonVisualState::Press);
	}
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
	auto& visual{ button_.SpriteVisual(state_) };

	visual.animation.reset();
	visual.animation_options.reset();

	if (auto entity{ button_.FindPart(impl::ButtonPart::Sprite) }) {
		entity.Remove<impl::ButtonAnimationPart>();
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
		for (auto& visual : resolved_desc.text.states) {
			if (visual.defined) {
				visual.auto_box = false;
			}
		}
	}

	Button button{ scene.CreateEntity() };

	PTGN_DEFAULT_NAME(button, "Button");

	button.Add<Visible>(true);
	button.Add<impl::ButtonData>();
	button.Add<Transform>(transform);
	button.Add<Origin>(resolved_desc.origin);

	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

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
