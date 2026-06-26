#include "runtime/ui/button.h"

#include <ecs/ecs.h>

#include <algorithm>
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
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
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

ButtonDesc MakeButtonDesc(V2_float size, const ButtonConfig& config, Origin origin) {
	ButtonDesc desc{
		.shape	= Rect{ size },
		.origin = origin,
	};

	auto add_background = [&](ButtonVisualState state, const std::optional<Color>& color) {
		if (!color.has_value()) {
			return;
		}

		desc.shapes.emplace_back(
			ButtonShapeConfig{
				.part		= ButtonPart::Background,
				.state		= state,
				.shape		= config.background_size.value_or(size),
				.origin		= origin,
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
				.origin	 = origin,
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
				.origin		   = Origin::Center,
				.alignment	   = config.text_alignment.value_or({}),
				.wrap		   = config.text_wrap,
				.overflow	   = config.text_overflow,
				.max_lines	   = config.text_max_lines,
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

constexpr bool IsPressVisualState(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;
		case Press:			[[fallthrough]];
		case ToggledPress:	[[fallthrough]];
		case DisabledPress: return true;
		default:			return false;
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

ButtonVisualState ToVisualState(ButtonState state) {
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

std::optional<Entity> FindButtonPart(Button button, impl::ButtonChild child) {
	if (!HasChildren(button)) {
		return std::nullopt;
	}

	auto children{ GetChildren(button) };

	auto it{ std::ranges::find_if(children, [child](Entity entity) {
		return IsButtonChild(entity) && entity.Get<impl::ButtonChild>() == child;
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

V2_float GetButtonShapeSize(Button button) {
	auto shape{ button.GetShape() };

	auto transform{ GetWorldTransform(button) };

	return std::visit([&](const auto& value) { return value.GetSize(transform); }, shape);
}

std::optional<Animation> TryAnimationForVisualState(Button button, ButtonVisualState state) {
	for (auto fallback_state : GetVisualStateFallbacks(state)) {
		std::optional<Animation> animation{
			FindButtonPart(button, { ButtonPart::Sprite, fallback_state })
		};
		if (animation.has_value() && animation.value().Has<impl::AnimationData>()) {
			return animation;
		}
	}

	return std::nullopt;
}

template <ShapeType T>
Button& ModifyShape(Button& button, T shape, ButtonPart part, ButtonVisualState state) {
	auto entity{ button.Part(part, state) };
	entity.Remove<impl::ButtonShapeSync>();
	entity.Remove<Rect>();
	entity.Remove<Circle>();
	entity.Add<T>(shape);
	return button;
}

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Button button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::AnimationComplete>([this]() {
		if (!button || !IsButtonChild(entity) || !button.Has<ButtonVisualOverride>()) {
			return;
		}

		const auto& child{ entity.Get<ButtonChild>() };
		auto visual_override{ button.Get<ButtonVisualOverride>() };

		if (visual_override.state == child.state) {
			button.Remove<ButtonVisualOverride>();
			button.RefreshVisualState();

			ResetButtonAnimation(Animation{ entity });
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
		Button button{ entity };

		button.UpdateChildLayouts();
		button.RefreshVisualState();
	}
}

} // namespace impl

Button::Button(Entity entity) : Entity{ entity } {}

bool Button::IsEnabled(bool check_for_hover_enabled) const {
	auto enabled{ TryGet<impl::ButtonEnabled>() };
	if (!enabled) {
		return false;
	}

	if (check_for_hover_enabled) {
		return enabled->hover;
	}

	return enabled->press;
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

std::variant<Rect, Circle> Button::GetShape() const {
	if (auto rect{ TryGet<Rect>() }) {
		return *rect;
	}

	if (auto circle{ TryGet<Circle>() }) {
		return *circle;
	}

	PTGN_ERROR("Button has no shape. Use Button::Shape() to set a shape.");
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
		SetState(impl::InternalButtonState::IdleUp);
	}

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

	if (auto animation{ TryAnimationForVisualState(*this, press_visual_state) }) {
		auto part{ animation->TryGet<impl::ButtonAnimationPart>() };

		if (part && part->options.lock_visual_state) {
			auto& visual_override{ TryAdd<impl::ButtonVisualOverride>() };
			visual_override.state		= press_visual_state;
			visual_override.block_press = part->options.block_press;

			RefreshVisualState();
		}

		animation->Reset();
		animation->SetCurrentFrame(part ? part->options.static_frame : 0uz);
		animation->Start(true);
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
	return Shape(size);
}

Button& Button::Shape(Rect rect) {
	Remove<Circle>();
	Add<Rect>(rect);
	UpdateChildShapes();
	UpdateChildLayouts();
	return *this;
}

Button& Button::Shape(Circle circle) {
	Remove<Rect>();
	Add<Circle>(circle);
	UpdateChildShapes();
	UpdateChildLayouts();
	return *this;
}

bool Button::HasPart(ButtonPart part, ButtonVisualState state) const {
	return FindButtonPart(*this, { part, state }).has_value();
}

Entity Button::Part(ButtonPart part, ButtonVisualState state) {
	if (auto existing{ FindButtonPart(*this, { part, state }) }) {
		return existing.value();
	}

	Entity entity{ GetScene().CreateEntity() };

	entity.Add<impl::ButtonChild>(part, state);
	SetParent(entity, *this);

	Show(entity, false);

	return entity;
}

std::vector<Entity> Button::Parts(ButtonPart part) const {
	return FindButtonParts(*this, part);
}

std::vector<Entity> Button::Parts() const {
	return FindButtonParts(*this);
}

Button& Button::RemovePart(ButtonPart part, ButtonVisualState state) {
	if (auto entity{ FindButtonPart(*this, { part, state }) }) {
		entity.value().Destroy();
	}

	return *this;
}

Button& Button::RemoveParts(ButtonPart part) {
	std::ranges::for_each(Parts(part), [](Entity child) { child.Destroy(); });
	return *this;
}

Text Button::GetText(ButtonVisualState state) {
	if (auto text{ FindButtonPart(*this, { ButtonPart::Text, state }) }) {
		return ptgn::Text{ text.value() };
	}

	ptgn::Text text{ CreateText(GetScene()) };

	text.Add<impl::ButtonChild>(ButtonPart::Text, state);
	text.Add<impl::ButtonTextAutoBox>();

	SetParent(text, *this);
	Show(text);

	UpdateChildLayouts();
	RefreshVisualState();

	return text;
}

Text Button::Text(ButtonVisualState state) {
	return Text({}, state);
}

Text Button::Text(std::string_view content, Color color, float font_size, ButtonVisualState state) {
	return Text(
		{ { .text = std::string{ content }, .style = { .color = color, .size = font_size } } },
		state
	);
}

Text Button::Text(StyledText styled_text, ButtonVisualState state) {
	auto text{ GetText(state) };
	text.Content(std::move(styled_text));
	return text;
}

Button& Button::Sprite(
	std::string_view texture_key, std::optional<Origin> origin, ButtonVisualState state
) {
	if (auto sprite{ FindButtonPart(*this, { ButtonPart::Sprite, state }) }) {
		ptgn::Sprite{ sprite.value() }.SetTexture(texture_key);
		if (origin.has_value()) {
			SetDrawOrigin(sprite.value(), origin.value());
			sprite.value().Remove<impl::ButtonOriginSync>();
		} else {
			SetDrawOrigin(sprite.value(), GetDrawOrigin(*this));
		}
		return *this;
	}

	auto sprite{ CreateSprite(GetScene(), {}, texture_key, origin.value_or(GetDrawOrigin(*this))) };

	if (!origin.has_value()) {
		sprite.Add<impl::ButtonOriginSync>();
	}
	sprite.Add<impl::ButtonChild>(ButtonPart::Sprite, state);

	SetParent(sprite, *this);

	RefreshVisualState();

	return *this;
}

Button& Button::TextOrigin(Origin origin, ButtonVisualState state) {
	ptgn::Text text{ GetText(state) };

	auto& auto_box{ text.TryAdd<impl::ButtonTextAutoBox>() };
	auto_box.origin = origin;

	UpdateChildLayouts();

	return *this;
}

Button& Button::RemoveBackground() {
	return RemoveParts(ButtonPart::Background);
}

Button& Button::RemoveBackground(ButtonVisualState state) {
	return RemovePart(ButtonPart::Background, state);
}

Button& Button::RemoveBorder() {
	return RemoveParts(ButtonPart::Border);
}

Button& Button::RemoveBorder(ButtonVisualState state) {
	return RemovePart(ButtonPart::Border, state);
}

Button& Button::RemoveText() {
	return RemoveParts(ButtonPart::Text);
}

Button& Button::RemoveText(ButtonVisualState state) {
	return RemovePart(ButtonPart::Text, state);
}

Button& Button::RemoveSprite() {
	return RemoveParts(ButtonPart::Sprite);
}

Button& Button::RemoveSprite(ButtonVisualState state) {
	return RemovePart(ButtonPart::Sprite, state);
}

Button& Button::ShapePart(ButtonPart part, ButtonVisualState state, Color color, FillStyle fill) {
	if (HasPart(part, state)) {
		return *this;
	}

	auto entity{ Part(part, state) };

	entity.Add<impl::ButtonOriginSync>();
	entity.Add<impl::ButtonShapeSync>();
	entity.Add<Color>(color);
	entity.Add<FillStyle>(fill);

	if (Has<Rect>()) {
		SetDraw<RectDraw>(entity);
		entity.Add<Rect>(Get<Rect>());
	} else if (Has<Circle>()) {
		SetDraw<CircleDraw>(entity);
		entity.Add<Circle>(Get<Circle>());
	} else {
		PTGN_ERROR("Button must have a valid shape");
	}

	RefreshVisualState();

	return *this;
}

Button& Button::Background(ButtonVisualState state) {
	return ShapePart(ButtonPart::Background, state, GetDefaultBackgroundColor(state), Solid{});
}

Button& Button::Background() {
	Background(ButtonVisualState::Idle);
	Background(ButtonVisualState::Hover);
	Background(ButtonVisualState::Press);
	return *this;
}

Button& Button::BackgroundOrigin(Origin origin) {
	for (Entity child : Parts(ButtonPart::Background)) {
		SetDrawOrigin(child, origin);
		child.Remove<impl::ButtonOriginSync>();
	}
	return *this;
}

Button& Button::ClearBackgroundOrigin() {
	auto origin{ GetDrawOrigin(*this) };
	for (Entity child : Parts(ButtonPart::Background)) {
		SetDrawOrigin(child, origin);
		child.Add<impl::ButtonOriginSync>();
	}
	return *this;
}

Button& Button::BackgroundColor(Color color, ButtonVisualState state) {
	Background(state);
	Part(ButtonPart::Background, state).Add<Color>(color);
	RefreshVisualState();
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

Button& Button::BackgroundShape(Rect rect, ButtonVisualState state) {
	return ModifyShape(*this, rect, ButtonPart::Background, state);
}

Button& Button::BackgroundShape(Circle circle, ButtonVisualState state) {
	return ModifyShape(*this, circle, ButtonPart::Background, state);
}

Button& Button::Border() {
	Border(ButtonVisualState::Idle);
	Border(ButtonVisualState::Hover);
	Border(ButtonVisualState::Press);
	return *this;
}

Button& Button::Border(ButtonVisualState state) {
	return ShapePart(
		ButtonPart::Border, state, GetDefaultBorderColor(state), kDefaultButtonBorderWidth
	);
}

Button& Button::BorderOrigin(Origin origin) {
	for (Entity child : Parts(ButtonPart::Border)) {
		SetDrawOrigin(child, origin);
		child.Remove<impl::ButtonOriginSync>();
	}
	return *this;
}

Button& Button::ClearBorderOrigin() {
	auto origin{ GetDrawOrigin(*this) };
	for (Entity child : Parts(ButtonPart::Border)) {
		SetDrawOrigin(child, origin);
		child.Add<impl::ButtonOriginSync>();
	}
	return *this;
}

Button& Button::BorderColor(Color color, ButtonVisualState state) {
	Border(state);
	Part(ButtonPart::Border, state).Add<Color>(color);
	RefreshVisualState();
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

Button& Button::BorderShape(Rect rect, ButtonVisualState state) {
	return ModifyShape(*this, rect, ButtonPart::Border, state);
}

Button& Button::BorderShape(Circle circle, ButtonVisualState state) {
	return ModifyShape(*this, circle, ButtonPart::Border, state);
}

Button& Button::BorderWidth(FillStyle fill, ButtonVisualState state) {
	Border(state);
	Part(ButtonPart::Border, state).Add<FillStyle>(fill);
	RefreshVisualState();
	return *this;
}

Button& Button::Animation(
	ptgn::Animation animation, ButtonVisualState state, ButtonAnimationOptions options
) {
	RemoveAnimation(state);

	animation.Add<impl::ButtonOriginSync>();
	animation.Add<impl::ButtonChild>(ButtonPart::Sprite, state);
	animation.Add<impl::ButtonAnimationPart>(options);

	SetParent(animation, *this);
	SetDrawOrigin(animation, Origin::Center);

	animation.Reset();

	if (options.playback == ButtonAnimationPlayback::StaticFrame) {
		animation.SetCurrentFrame(options.static_frame);
	}

	Hide(animation);

	if (!HasScript<impl::ButtonAnimationCompleteScript>(animation)) {
		AddScript<impl::ButtonAnimationCompleteScript>(animation, *this);
	}

	RefreshVisualState();

	return *this;
}

Button& Button::Animation(ptgn::Animation animation, ButtonVisualState state) {
	ButtonAnimationOptions options;

	if (state == ButtonVisualState::Press || state == ButtonVisualState::ToggledPress ||
		state == ButtonVisualState::DisabledPress) {
		options.playback		  = ButtonAnimationPlayback::PlayOnce;
		options.lock_visual_state = true;
		options.block_press		  = false;
	}

	return Animation(animation, state, options);
}

Button& Button::StaticAnimationFrame(
	ptgn::Animation animation, ButtonVisualState state, std::size_t frame
) {
	return Animation(
		animation, state,
		ButtonAnimationOptions{
			.playback	  = ButtonAnimationPlayback::StaticFrame,
			.static_frame = frame,
		}
	);
}

Button& Button::RemoveAnimation() {
	return RemoveParts(ButtonPart::Sprite);
}

Button& Button::RemoveAnimation(ButtonVisualState state) {
	std::optional<ptgn::Animation> animation{
		FindButtonPart(*this, { ButtonPart::Sprite, state })
	};

	if (!animation.has_value()) {
		return *this;
	}

	animation.value().Stop(true);
	animation.value().Remove<impl::AnimationData>();
	animation.value().Remove<impl::ButtonChild>();
	Hide(animation.value());

	return *this;
}

Button& Button::TextAutoBox(bool enabled, ButtonVisualState state) {
	ptgn::Text text{ GetText(state) };
	auto& auto_box{ text.TryAdd<impl::ButtonTextAutoBox>() };
	auto_box.enabled = enabled;
	UpdateChildLayouts();
	return *this;
}

Button& Button::TextPadding(Padding padding, ButtonVisualState state) {
	ptgn::Text text{ GetText(state) };
	auto& auto_box{ text.TryAdd<impl::ButtonTextAutoBox>() };
	auto_box.padding = padding;
	UpdateChildLayouts();
	return *this;
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

void Button::RefreshVisualState() const {
	auto parts{ Parts() };

	for (const auto& part : parts) {
		if (!part.Has<impl::ButtonOriginSync>()) {
			continue;
		}
		SetDrawOrigin(part, GetDrawOrigin(*this));
	}

	if (!IsVisible(*this)) {
		for (const auto& part : parts) {
			Hide(part);
		}
		return;
	}

	auto active_state{ GetVisualState() };
	auto fallback_states{ GetVisualStateFallbacks(active_state) };

	for (const auto& part : parts) {
		Hide(part);
	}

	auto show_part = [&](ButtonPart part) {
		for (auto state : fallback_states) {
			bool found{ false };

			for (const auto& entity : parts) {
				if (const auto& info{ entity.Get<impl::ButtonChild>() };
					info.part != part || info.state != state) {
					continue;
				}

				Show(entity);
				found = true;
			}

			if (found) {
				return;
			}
		}
	};

	show_part(ButtonPart::Background);
	show_part(ButtonPart::Border);
	show_part(ButtonPart::Sprite);
	show_part(ButtonPart::Text);
}

void Button::SetState(impl::InternalButtonState state) {
	auto& data{ Get<impl::ButtonData>() };

	if (data.state == state) {
		return;
	}

	auto old_visual_state{ GetVisualState() };

	data.state = state;

	if (auto new_visual_state{ GetVisualState() };
		old_visual_state != new_visual_state && IsPressVisualState(new_visual_state)) {
		if (auto animation{ TryAnimationForVisualState(*this, new_visual_state) }) {
			ResetButtonAnimation(animation.value());
		}
	}

	RefreshVisualState();
}

void Button::PlaySound(ButtonState active) {
	auto sound{ GetSound(active) };

	if (!sound.has_value()) {
		return;
	}

	auto& audio{ GetScene().ctx().audio };
	audio.Play(sound.value().GetEntity().Get<impl::AssetName>().value);
}

void Button::PlayAnimation(ButtonState active) const {
	auto active_state{ ToVisualState(active) };

	std::optional<ptgn::Animation> override_animation;

	if (auto visual_override{ TryGet<impl::ButtonVisualOverride>() }) {
		override_animation = TryAnimationForVisualState(*this, visual_override->state);
	}

	for (Entity part : Parts(ButtonPart::Sprite)) {
		if (!part.Has<impl::ButtonChild, impl::AnimationData>()) {
			continue;
		}

		const auto& part_data{ part.Get<impl::ButtonChild>() };

		ptgn::Animation animation{ part };

		auto animation_part{ part.TryGet<impl::ButtonAnimationPart>() };
		auto playback{ animation_part ? animation_part->options.playback
									  : ButtonAnimationPlayback::Play };

		if (part_data.state != active_state) {
			if (bool is_override_animation{ override_animation.has_value() &&
											Entity{ override_animation.value() } == part };
				!is_override_animation) {
				ResetButtonAnimation(animation);
			}

			continue;
		}

		switch (playback) {
			using enum ButtonAnimationPlayback;

			case StaticFrame: ResetButtonAnimation(animation); break;

			case Play:		  [[fallthrough]];

			case PlayOnce:	  animation.Start(true); break;
		}
	}
}

void Button::UpdateChildShapes() const {
	for (Entity part : Parts()) {
		if (!part.Has<impl::ButtonShapeSync>()) {
			continue;
		}
		if (Has<Rect>()) {
			part.Remove<Circle>();
			part.Add<Rect>(Get<Rect>());
		} else if (Has<Circle>()) {
			part.Remove<Rect>();
			part.Add<Circle>(Get<Circle>());
		} else {
			PTGN_ERROR("Button must have a valid shape");
		}
	}
}

void Button::UpdateChildLayouts() const {
	auto size{ GetButtonShapeSize(*this) };

	if (!size.IsPositive()) {
		return;
	}

	Rect button_rect{ size, GetDrawOrigin(*this) };

	for (Entity part : Parts(ButtonPart::Text)) {
		if (!part.Has<impl::ButtonTextAutoBox>()) {
			continue;
		}
		const auto& auto_box{ part.Get<impl::ButtonTextAutoBox>() };

		if (!auto_box.enabled) {
			continue;
		}

		auto content_rect{
			button_rect.Expanded(auto_box.padding.GetLeftTop(), auto_box.padding.GetRightBottom())
		};

		if (!content_rect.GetSize().IsPositive()) {
			continue;
		}

		Origin origin{ auto_box.origin };
		V2_float content_size{ content_rect.GetSize() };
		V2_float text_position{ content_rect.GetOriginPoint(origin) };

		SetPosition(part, text_position);
		SetDrawOrigin(part, origin);

		ptgn::Text text{ part };

		if (Rect text_box{ {}, content_size }; text.GetTextBox().rect != text_box) {
			text.Box(text_box);
		}

		auto alignment{ GetAlignment(origin) };

		text.OverrideAlignment(alignment);
	}
}

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc) {
	Button button{ scene.CreateEntity() };

	button.Add<impl::ButtonData>();
	button.Add<impl::ButtonEnabled>();

	if (desc.ui_layer) {
		SetUI(button, true);
	}

	Show(button, false);

	std::visit([button](const auto& shape) mutable { button.Shape(shape); }, desc.shape);

	SetTransform(button, transform);
	SetDrawOrigin(button, desc.origin);
	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

	if (!desc.enabled) {
		button.Disable();
	}

	return button;
}

Button CreateButton(Scene& scene, Transform transform, Rect rect, Origin origin) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.shape	= rect,
			.origin = origin,
		}
	);
}

Button CreateButton(Scene& scene, Transform transform, Circle circle, Origin origin) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.shape	= circle,
			.origin = origin,
		}
	);
}

Button CreateButton(
	Scene& scene, Transform transform, V2_float size, const ButtonConfig& config, Origin origin
) {
	Button button{ CreateButton(scene, transform, Rect{ size }, origin) };

	if (config.background_color.has_value()) {
		button.BackgroundColor(config.background_color.value());
	}

	if (config.texture.has_value()) {
		button.Sprite(config.texture.value());
	}

	if (config.content.has_value()) {
		Text text{ button.Text() };

		text.Content(config.content.value())
			.Color(config.text_color.value_or(kDefaultButtonTextColor))
			.Size(config.font_size)
			.Font(config.font);

		if (config.text_alignment.has_value()) {
			text.Align(config.text_alignment.value());
		}
	}

	button.Sound(config.sound_hover, ButtonState::Hover);
	button.Sound(config.sound_press, ButtonState::Press);

	return button;
}

Button CreateAnimatedButton(
	Scene& scene, Transform transform, std::optional<V2_float> size,
	const AnimatedButtonConfig& config, Origin origin
) {
	V2_float resolved_size{ size.value_or(V2_float{}) };

	Button button{ CreateButton(scene, transform, Rect{ resolved_size }, origin) };
	button.Sprite(config.texture);

	button.Sound(config.sound_hover, ButtonState::Hover);
	button.Sound(config.sound_press, ButtonState::Press);

	return button;
}

} // namespace ptgn