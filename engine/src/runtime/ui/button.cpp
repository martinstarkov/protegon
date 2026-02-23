#include "runtime/ui/button.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/input/interactive.h"
#include "runtime/input/scene_input.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

ButtonDisabledTexture::ButtonDisabledTexture(const Texture& t) : Texture{ t } {}

ButtonDisabledTexture::ButtonDisabledTexture(Texture&& t) : Texture{ std::move(t) } {}

void InternalButtonScript::OnEvent(EventDispatcher d) {
	d.Dispatch<MouseMoveOver>([this](const MouseMoveOver&) { OnMouseMoveOver(); });
	d.Dispatch<MouseMoveOut>([this](const MouseMoveOut&) { OnMouseMoveOut(); });
	d.Dispatch<MousePressedOver>([this](const MousePressedOver& e) { OnMousePressedOver(e.button); }
	);
	d.Dispatch<MousePressedOut>([this](const MousePressedOut& e) { OnMousePressedOut(e.button); });
	d.Dispatch<MouseReleasedOver>([this](const MouseReleasedOver& e) {
		OnMouseReleasedOver(e.button);
	});
	d.Dispatch<MouseReleasedOut>([this](const MouseReleasedOut& e) { OnMouseReleasedOut(e.button); }
	);
}

void InternalButtonScript::OnMouseMoveOver() {
	auto& state{ entity.Get<InternalButtonState>() };
	if (!IsButtonEnabled(entity, true)) {
		return;
	}
	if (state == InternalButtonState::IdleUp) {
		state = InternalButtonState::Hover;
		ButtonStartHover(entity);
	} else if (state == InternalButtonState::IdleDown) {
		state = InternalButtonState::HoverPressed;
		ButtonStartHover(entity);
	} else if (state == InternalButtonState::HeldOutside) {
		state = InternalButtonState::Pressed;
		return;
	}
	ButtonContinueHover(entity);
}

void InternalButtonScript::OnMouseMoveOut() {
	auto& state{ entity.Get<InternalButtonState>() };
	if (!IsButtonEnabled(entity, true)) {
		return;
	}
	using enum InternalButtonState;
	if (state == Hover) {
		state = IdleUp;
		ButtonStopHover(entity);
	} else if (state == Pressed) {
		state = HeldOutside;
		ButtonStopHover(entity);
	} else if (state == HoverPressed) {
		state = IdleDown;
		ButtonStopHover(entity);
	}
}

void InternalButtonScript::OnMousePressedOver(Mouse mouse) {
	if (!IsButtonEnabled(entity, false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::Hover) {
			state = InternalButtonState::Pressed;
		}
	}
}

void InternalButtonScript::OnMousePressedOut(Mouse mouse) {
	if (!IsButtonEnabled(entity, false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::IdleUp) {
			state = InternalButtonState::IdleDown;
		}
	}
}

void InternalButtonScript::OnMouseReleasedOver(Mouse mouse) {
	if (!IsButtonEnabled(entity, false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum ptgn::impl::InternalButtonState;
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == Pressed) {
			state = Hover;
			ptgn::ButtonActivate(entity);
		} else if (state == HoverPressed) {
			state = Hover;
		}
	}
}

void InternalButtonScript::OnMouseReleasedOut(Mouse mouse) {
	if (!IsButtonEnabled(entity, false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum ptgn::impl::InternalButtonState;
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == IdleDown) {
			state = IdleUp;
		} else if (state == HeldOutside) {
			state = IdleUp;
		}
	}
}

// TODO: Fix.
// void ToggleButtonScript::OnButtonActivate() {
//	ToggleButton self{ entity };
//	if (!self.IsButtonEnabled(false)) {
//		return;
//	}
//	self.Toggle();
// }
// TODO: Fix.
// ToggleButtonGroupScript::ToggleButtonGroupScript(const ToggleButtonGroup& group) :
//	toggle_button_group{ group } {}
//
// void ToggleButtonGroupScript::OnButtonActivate() {
//	ToggleButton self{ entity };
//	if (!self.IsButtonEnabled(false)) {
//		return;
//	}
//
//	PTGN_ASSERT(self.button.Has<ToggleButtonGroupKey>());
//
//	PTGN_ASSERT(toggle_button_group);
//	toggle_button_group.SetActive(self.Get<ToggleButtonGroupKey>());
// }

void ButtonColor::SetToState(ButtonState state) {
	current_ = Get(state);
}

const Color& ButtonColor::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Current: return current_;
		case Default: return default_;
		case Hover:	  return hover_;
		case Pressed: return pressed_;
		default:	  PTGN_ERROR("Invalid button state");
	}
}

Color& ButtonColor::Get(ButtonState state) {
	return const_cast<Color&>(std::as_const(*this).Get(state));
}

ButtonText::ButtonText(
	Entity parent, Scene& scene, ButtonState state, std::string_view text_content, Color text_color,
	std::optional<float> font_size, std::optional<Font> font, const TextProperties& text_properties
) {
	Set(parent, scene, ButtonState::Default, text_content, text_color, font_size, font,
		text_properties);
	if (state != ButtonState::Default) {
		Set(parent, scene, state, text_content, text_color, font_size, font, text_properties);
	}
}

Entity ButtonText::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Default: return default_;
		case Hover:	  return hover_;
		case Pressed: return pressed_;
		case Current: [[fallthrough]];
		default:	  PTGN_ERROR("Invalid button state");
	}
}

Entity ButtonText::GetValid(ButtonState state) const {
	using enum ptgn::ButtonState;
	if (state == Current) {
		return Get(Default);
	}
	auto text{ Get(state) };
	if (!text) {
		return Get(Default);
	}
	return text;
}

Color ButtonText::GetTextColor(ButtonState state) const {
	return ptgn::GetTextColor(GetValid(state));
}

std::string ButtonText::GetTextContent(ButtonState state) const {
	return ptgn::GetTextContent(GetValid(state));
}

float ButtonText::GetFontSize(ButtonState state) const {
	return ptgn::GetTextFontSize(GetValid(state), false, {});
}

TextJustify ButtonText::GetTextJustify(ButtonState state) const {
	return ptgn::GetTextJustify(GetValid(state));
}

void ButtonText::Set(
	Entity parent, Scene& scene, ButtonState state, std::string_view text_content, Color text_color,
	std::optional<float> font_size, std::optional<Font> font, const TextProperties& text_properties
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	auto text{ Get(state) };
	if (!text) {
		text = CreateText(scene, text_content, text_color, font_size, font, text_properties);
		Hide(text);
		SetParent(text, parent);
		switch (state) {
			using enum ptgn::ButtonState;
			case Default: {
				PTGN_ASSERT(!default_);
				default_ = GameObject{ std::move(text) };
				break;
			}
			case Hover: {
				PTGN_ASSERT(!hover_);
				hover_ = GameObject{ std::move(text) };
				break;
			}
			case Pressed: {
				PTGN_ASSERT(!pressed_);
				pressed_ = GameObject{ std::move(text) };
				break;
			}
			case Current: [[fallthrough]];
			default:	  PTGN_ERROR("Invalid button state");
		}
	} else {
		impl::SetTextParameter(text, impl::TextColor{ text_color }, false);
		impl::SetTextParameter(text, impl::TextContent{ text_content }, false);
		impl::SetTextParameter(text, font.value_or(Font{}), false);
		impl::SetTextParameter(text, FontSize{ font_size.value_or({}) }, false);
		SetTextProperties(text, text_properties, true, {});
	}
}

Texture ButtonTexture::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Default: return default_;
		case Hover:	  return hover_;
		case Pressed: return pressed_;
		case Current: [[fallthrough]];
		default:	  PTGN_ERROR("Invalid button state");
	}
}

} // namespace impl

template <typename TProperty>
static void UpdateStateProperty(Entity entity, const ButtonState& state) {
	if (auto property{ entity.TryGet<TProperty>() }) {
		property->SetToState(state);
	}
}

static void SetTextureState(Entity button, bool is_toggled, const ButtonState& state) {
	auto key{ button.TryGet<Texture>() };
	if (!key) {
		return;
	}
	if (!IsButtonEnabled(button, false) && button.Has<impl::ButtonDisabledTexture>()) {
		*key = button.Get<impl::ButtonDisabledTexture>();
	} else if (is_toggled && button.Has<impl::ButtonTextureToggled>()) {
		*key = button.Get<impl::ButtonTextureToggled>().Get(state);
	} else if (button.Has<impl::ButtonTexture>()) {
		*key = button.Get<impl::ButtonTexture>().Get(state);
	}
}

static bool IsToggled(Entity button) {
	return button.Has<impl::ButtonToggled>();
}

template <typename DefaultComponent, typename ToggledComponent>
static Color GetEffectiveColor(
	Entity button, bool is_toggled, Color fallback_color = color::Transparent
) {
	if (is_toggled && button.Has<ToggledComponent>()) {
		return button.Get<ToggledComponent>().current_;
	} else if (button.Has<DefaultComponent>()) {
		return button.Get<DefaultComponent>().current_;
	}
	return fallback_color;
}

// @return Button texture, or {} is button has no valid texture.
static std::optional<Texture> GetButtonTexture(
	Entity button, bool is_toggled, const ButtonState& state
) {
	SetTextureState(button, is_toggled, state);

	auto texture{ button.TryGet<Texture>() };
	return texture ? std::optional<Texture>{ *texture } : std::nullopt;
}

// @return Button text, or empty text object if button has no text.
static Entity GetButtonText(Entity button, bool is_toggled, const ButtonState& state) {
	Entity text;

	if (is_toggled && button.Has<impl::ButtonTextToggled>()) {
		const auto& button_text_toggled{ button.Get<impl::ButtonTextToggled>() };
		text = button_text_toggled.GetValid(state);
	} else if (button.Has<impl::ButtonText>()) {
		const auto& button_text{ button.Get<impl::ButtonText>() };
		text = button_text.GetValid(state);
	}

	return text;
}

void impl::ButtonDraw::Draw(Renderer& renderer, Entity button) {
	Color tint{ GetTint(button) };

	if (tint.a == 0) {
		return;
	}

	auto transform{ GetDrawTransform(button) };
	auto depth{ GetDepth(button) };
	auto blend_mode{ GetBlendMode(button) };
	auto camera{ GetCamera(button) };

	auto tint_n{ tint.Normalized() };
	const auto state{ GetButtonState(button) };
	auto button_size{ GetButtonSize(button) };
	bool is_toggled{ IsToggled(button) };
	PTGN_ASSERT(!button_size.IsZero(), "Buttons must have a non-zero size");
	auto button_origin{ GetDrawOrigin(button) };
	auto text{ GetButtonText(button, is_toggled, state) };

	UpdateStateProperty<impl::ButtonColor>(button, state);
	UpdateStateProperty<impl::ButtonColorToggled>(button, state);
	UpdateStateProperty<impl::ButtonTint>(button, state);
	UpdateStateProperty<impl::ButtonTintToggled>(button, state);
	UpdateStateProperty<impl::ButtonBorderColor>(button, state);
	UpdateStateProperty<impl::ButtonBorderColorToggled>(button, state);

	auto button_texture{ GetButtonTexture(button, is_toggled, state) };

	if (button_texture.has_value()) {
		auto texture_tint{ GetEffectiveColor<impl::ButtonTint, impl::ButtonTintToggled>(
			button, is_toggled, color::White
		) };

		if (texture_tint.a) {
			impl::DrawQuadTexture(
				renderer, *button_texture, transform, button_size, button_origin,
				Tint{ texture_tint.Normalized() * tint_n }, depth, blend_mode,
				GetTextureCoordinates(button, false), camera
			);
		}
	} else {
		auto line_width{ button.GetOrDefault<impl::ButtonBackgroundWidth>() };

		if (line_width > 0.0f) {
			auto color{
				GetEffectiveColor<impl::ButtonColor, impl::ButtonColorToggled>(button, is_toggled)
			};

			if (color.a) {
				// TODO: Fix.
				/*renderer.DrawRect(
					transform, button_size, Tint{ color.Normalized() * tint_n },
					line_width.GetValue(), button_origin, depth, blend_mode, camera, post_fx
				);*/
			}
		}
	}

	auto line_width{ button.GetOrDefault<impl::ButtonBorderWidth>() };

	if (line_width > 0.0f) {
		auto color{ GetEffectiveColor<impl::ButtonBorderColor, impl::ButtonBorderColorToggled>(
			button, is_toggled
		) };

		if (color.a) {
			// TODO: Fix.
			/*renderer.DrawRect(
				transform, button_size, Tint{ color.Normalized() * tint_n }, line_width.GetValue(),
				button_origin, depth, blend_mode, camera, post_fx
			);*/
		}
	}

	if (!text) {
		return;
	}

	V2_float text_size;

	if (auto fixed_size{ button.TryGet<ButtonTextFixedSize>() }) {
		text_size = { fixed_size->x.value_or(button_size.x),
					  fixed_size->y.value_or(button_size.y) };
	}

	auto text_camera{ GetNonPrimaryCamera(text).value_or(camera) };

	impl::DrawText(renderer, text, text_size, text_camera, tint, button_origin, button_size);
}

void OnButtonActivate(Entity button, const std::function<void()>& callback) {
	AddScript<impl::ButtonActivateScript>(button, callback);
}

void OnButtonHover(Entity button, const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverScript>(button, callback);
}

void OnButtonHoverStart(Entity button, const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStartScript>(button, callback);
}

void OnButtonHoverStop(Entity button, const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStopScript>(button, callback);
}

void EnableButton(Entity button, bool enable_hover, bool reset_state) {
	return SetButtonEnabled(button, true, enable_hover, reset_state);
}

void DisableButton(Entity button, bool disable_hover, bool reset_state) {
	return SetButtonEnabled(button, false, !disable_hover, reset_state);
}

void SetButtonEnabled(Entity button, bool enable_activation, bool enable_hover, bool reset_state) {
	button.Add<impl::ButtonEnabled>(enable_activation, enable_hover);
	if (reset_state) {
		auto& state{ button.Get<impl::InternalButtonState>() };
		state = impl::InternalButtonState::IdleUp;
	}
}

bool IsButtonEnabled(Entity button, bool check_for_hover_enabled) {
	if (!button.Has<impl::ButtonEnabled>()) {
		return false;
	}
	const auto& enabled{ button.Get<impl::ButtonEnabled>() };
	if (check_for_hover_enabled) {
		return enabled.hover;
	}
	return enabled.activate;
}

V2_float GetButtonSize(Entity button) {
	if (auto texture{ button.TryGet<Texture>() }) {
		return texture->GetSize();
	}

	if (auto rect{ button.TryGet<Rect>() }) {
		return rect->GetSize();
	}

	if (auto circle{ button.TryGet<Circle>() }) {
		return V2_float{ circle->radius * 2.0f };
	}

	PTGN_ERROR("Button has no valid size");
}

void SetButtonSize(Entity button, V2_float size) {
	button.Remove<Circle>();
	if (button.Has<Rect>()) {
		button.Get<Rect>() = Rect{ size };
	} else {
		button.Add<Rect>(size);
	}
	if (IsInteractive(button)) {
		ClearInteractiveShapes(button);
		auto shape{ button.GetScene().CreateEntity() };
		AddChild(button, shape);
		shape.Add<Rect>(size);
		AddInteractiveShape(button, GameObject{ std::move(shape) });
	}
}

void SetButtonRadius(Entity button, float radius) {
	button.Remove<Rect>();
	if (button.Has<Circle>()) {
		button.Get<Circle>() = Circle{ radius };
	} else {
		button.Add<Circle>(radius);
	}
	if (IsInteractive(button)) {
		ClearInteractiveShapes(button);
		auto shape{ button.GetScene().CreateEntity() };
		AddChild(button, shape);
		shape.Add<Circle>(radius);
		AddInteractiveShape(button, GameObject{ std::move(shape) });
	}
}

Color GetButtonBackgroundColor(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonColor>() ? button.Get<impl::ButtonColor>()
												  : impl::ButtonColor{} };
	return c.Get(state);
}

void SetButtonBackgroundColor(Entity button, Color color, ButtonState state) {
	if (!button.Has<impl::ButtonColor>()) {
		button.Add<impl::ButtonColor>(color);
	} else {
		auto& c{ button.Get<impl::ButtonColor>() };
		c.Get(state) = color;
	}
}

void SetButtonText(
	Entity button, std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!button.Has<impl::ButtonText>()) {
		button.Add<impl::ButtonText>(
			button, button.GetScene(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ button.Get<impl::ButtonText>() };
		c.Set(
			button, button.GetScene(), state, content, text_color, font_size, font, text_properties
		);
	}
}

Entity GetButtonText(Entity button, ButtonState state) {
	return button.Get<impl::ButtonText>().GetValid(state);
}

Color GetButtonTextColor(Entity button, ButtonState state) {
	return button.Get<impl::ButtonText>().GetTextColor(state);
}

void SetButtonTextColor(Entity button, Color text_color, ButtonState state) {
	if (!button.Has<impl::ButtonText>()) {
		button.Add<impl::ButtonText>(
			button, button.GetScene(), state, std::string_view{}, text_color, std::nullopt,
			std::nullopt, TextProperties{}
		);
	} else {
		const auto& c{ button.Get<impl::ButtonText>() };
		SetTextColor(c.Get(state), text_color);
	}
}

std::string GetButtonTextContent(Entity button, ButtonState state) {
	return button.Get<impl::ButtonText>().GetTextContent(state);
}

void SetButtonTextContent(Entity button, std::string_view content, ButtonState state) {
	if (!button.Has<impl::ButtonText>()) {
		button.Add<impl::ButtonText>(
			button, button.GetScene(), state, content, Color{}, std::nullopt, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ button.Get<impl::ButtonText>() };
		SetTextContent(c.Get(state), content);
	}
}

TextJustify GetButtonTextJustify(Entity button, ButtonState state) {
	return button.Get<impl::ButtonText>().GetTextJustify(state);
}

void SetButtonTextJustify(Entity button, TextJustify justify, ButtonState state) {
	if (!button.Has<impl::ButtonText>()) {
		TextProperties p{};
		p.justify = justify;
		button.Add<impl::ButtonText>(
			button, button.GetScene(), state, std::string_view{}, Color{}, std::nullopt,
			std::nullopt, p
		);
	} else {
		const auto& c{ button.Get<impl::ButtonText>() };
		SetTextJustify(c.Get(state), justify);
	}
}

ButtonTextFixedSize GetButtonTextFixedSize(Entity button) {
	return button.GetOrDefault<ButtonTextFixedSize>();
}

void SetButtonTextFixedSize(Entity button, ButtonTextFixedSize size) {
	if (!size.x.has_value() && !size.y.has_value()) {
		button.Remove<ButtonTextFixedSize>();
		return;
	}
	button.Add<ButtonTextFixedSize>(size);
}

float GetButtonFontSize(Entity button, ButtonState state) {
	return button.Get<impl::ButtonText>().GetFontSize(state);
}

void SetButtonFontSize(Entity button, float font_size, ButtonState state) {
	if (!button.Has<impl::ButtonText>()) {
		button.Add<impl::ButtonText>(
			button, button.GetScene(), state, std::string_view{}, Color{}, font_size, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ button.Get<impl::ButtonText>() };
		SetTextFontSize(c.Get(state), font_size);
	}
}

Texture GetButtonTexture(Entity button, ButtonState state) {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			button.Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return button.Get<Texture>();
	}
	PTGN_ASSERT(
		button.Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return button.Get<impl::ButtonTexture>().Get(state);
}

void SetButtonTexture(Entity button, Texture texture, ButtonState state) {
	if (IsInteractive(button) && GetInteractiveShapes(button).empty()) {
		auto shape{ button.GetScene().CreateEntity() };
		AddChild(button, shape);
		auto size{ texture.GetSize() };
		shape.Add<Rect>(size);
		AddInteractiveShape(button, GameObject{ std::move(shape) });
	}
	if (!button.Has<Texture>()) {
		button.Add<Texture>(texture);
	} else if (state == ButtonState::Current) {
		button.Add<Texture>(texture);
		return;
	}
	if (!button.Has<impl::ButtonTexture>()) {
		button.Add<impl::ButtonTexture>(texture);
	} else {
		const auto& c{ button.Get<impl::ButtonTexture>() };
		c.Get(state) = texture;
	}
}

void SetButtonDisabledTexture(Entity button, Texture texture) {
	if (!texture) {
		button.Remove<impl::ButtonDisabledTexture>();
	} else {
		button.Add<impl::ButtonDisabledTexture>(texture);
	}
}

Texture GetButtonDisabledTexture(Entity button) {
	PTGN_ASSERT(
		button.Has<impl::ButtonDisabledTexture>(),
		"Cannot retrieve disabled texture key as it has not been set for the button"
	);
	return button.Get<impl::ButtonDisabledTexture>();
}

Color GetButtonTint(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonTint>() ? button.Get<impl::ButtonTint>()
												 : impl::ButtonTint{} };
	return c.Get(state);
}

void SetButtonTint(Entity button, Color color, ButtonState state) {
	if (!button.Has<impl::ButtonTint>()) {
		auto& c{ button.Add<impl::ButtonTint>() };
		c.Get(state) = color;
	} else {
		auto& c{ button.Get<impl::ButtonTint>() };
		c.Get(state) = color;
	}
}

Color GetButtonBorderColor(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonBorderColor>() ? button.Get<impl::ButtonBorderColor>()
														: impl::ButtonBorderColor{} };
	return c.Get(state);
}

void SetButtonBorderColor(Entity button, Color color, ButtonState state) {
	if (!button.Has<impl::ButtonBorderColor>()) {
		button.Add<impl::ButtonBorderColor>(color);
	} else {
		auto& c{ button.Get<impl::ButtonBorderColor>() };
		c.Get(state) = color;
	}
}

float GetButtonBackgroundLineWidth(Entity button) {
	return button.Has<impl::ButtonBackgroundWidth>() ? button.Get<impl::ButtonBackgroundWidth>()
													 : impl::ButtonBackgroundWidth{};
}

void SetButtonBackgroundLineWidth(Entity button, float line_width) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Invalid button background line width");
	if (line_width != -1.0f && line_width < 1.0f) {
		button.Remove<impl::ButtonBackgroundWidth>();
	} else {
		button.Add<impl::ButtonBackgroundWidth>(line_width);
	}
}

float GetButtonBorderWidth(Entity button) {
	return button.Has<impl::ButtonBorderWidth>() ? button.Get<impl::ButtonBorderWidth>()
												 : impl::ButtonBorderWidth{};
}

void SetButtonBorderWidth(Entity button, float line_width) {
	PTGN_ASSERT(line_width >= 1.0f || line_width == 0.0f, "Cannot set negative border width");
	if (line_width == 0.0f) {
		button.Remove<impl::ButtonBorderWidth>();
	} else {
		button.Add<impl::ButtonBorderWidth>(line_width);
	}
}

impl::InternalButtonState GetButtonInternalState(Entity button) {
	return button.Get<impl::InternalButtonState>();
}

ButtonState GetButtonState(Entity button) {
	PTGN_ASSERT(button.Has<impl::InternalButtonState>());
	const auto& state{ button.Get<impl::InternalButtonState>() };
	using enum impl::InternalButtonState;
	if (state == Hover || state == HoverPressed) {
		return ButtonState::Hover;
	} else if (state == Pressed || state == HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

void ButtonActivate(Entity button) {
	if (!IsButtonEnabled(button, false)) {
		return;
	}
	if (auto scripts{ button.TryGet<impl::Scripts>() }) {
		impl::ButtonActivate event;
		scripts->Emit(event);
	}
}

void ButtonStartHover(Entity button) {
	if (!IsButtonEnabled(button, true)) {
		return;
	}
	if (auto scripts{ button.TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStart event;
		scripts->Emit(event);
	}
}

void ButtonContinueHover(Entity button) {
	if (!IsButtonEnabled(button, true)) {
		return;
	}
	if (auto scripts{ button.TryGet<impl::Scripts>() }) {
		impl::ButtonHover event;
		scripts->Emit(event);
	}
}

void ButtonStopHover(Entity button) {
	if (!IsButtonEnabled(button, true)) {
		return;
	}
	if (auto scripts{ button.TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStop event;
		scripts->Emit(event);
	}
}

// TODO: Fix.
/*
bool IsToggled(Entity button) {
	return button.Get<impl::ButtonToggled>();
}

Entity SetToggled(Entity button, bool toggled) {
	auto& t{ button.Get<impl::ButtonToggled>() };
	t = toggled;
}

Entity Toggle(Entity button) {
	auto& toggled{ button.Get<impl::ButtonToggled>() };
	toggled = !toggled;
}

Color GetBackgroundColorToggled(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonColorToggled>() ? button.Get<impl::ButtonColorToggled>()
												  : impl::ButtonColorToggled{} };
	return c.Get(state);
}

Entity SetBackgroundColorToggled(
	Entity button, Color color, ButtonState state
) {
	if (!button.Has<impl::ButtonColorToggled>()) {
		button.Add<impl::ButtonColorToggled>(color);
	} else {
		auto& c{ button.Get<impl::ButtonColorToggled>() };
		c.Get(state) = color;
	}
}

Color GetTextColorToggled(Entity button, ButtonState state) {
	return button.Get<impl::ButtonTextToggled>().GetTextColor(state);
}

Entity SetTextColorToggled(
	Entity button, Color text_color, ButtonState state
) {
	if (!button.Has<impl::ButtonTextToggled>()) {
		button.Add<impl::ButtonTextToggled>(
			button, button.GetScene(), state, std::string_view{}, text_color, std::nullopt,
std::nullopt, TextProperties{}
		);
	} else {
		const auto& c{ button.Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(text_color);
	}
}

TextContent GetTextContentToggled(Entity button, ButtonState state) {
	return button.Get<impl::ButtonTextToggled>().GetTextContent(state);
}

Entity SetTextContentToggled(
	Entity button, std::string_view content, ButtonState state
) {
	if (!button.Has<impl::ButtonTextToggled>()) {
		button.Add<impl::ButtonTextToggled>(
			button, button.GetScene(), state, content, Color{}, std::nullopt, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ button.Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
}

Entity SetTextToggled(
	Entity button, std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!button.Has<impl::ButtonTextToggled>()) {
		button.Add<impl::ButtonTextToggled>(
			button, button.GetScene(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ button.Get<impl::ButtonTextToggled>() };
		c.Set(button, button.GetScene(), state, content, text_color, font_size, font,
text_properties);
	}
}

Text GetTextToggled(Entity button, ButtonState state) {
	return button.Get<impl::ButtonTextToggled>().GetValid(state);
}

Color GetBorderColorToggled(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonBorderColorToggled>() ?
button.Get<impl::ButtonBorderColorToggled>() : impl::ButtonBorderColorToggled{} }; return
c.Get(state);
}

Entity SetBorderColorToggled(
	Entity button, Color color, ButtonState state
) {
	if (!button.Has<impl::ButtonBorderColorToggled>()) {
		button.Add<impl::ButtonBorderColorToggled>(color);
	} else {
		auto& c{ button.Get<impl::ButtonBorderColorToggled>() };
		c.Get(state) = color;
	}
}

Texture GetTextureKeyToggled(Entity button, ButtonState state) {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			button.Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return button.Get<Texture>();
	}
	PTGN_ASSERT(
		button.Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return button.Get<impl::ButtonTextureToggled>().Get(state);
}

Entity SetTextureKeyToggled(
	Entity button, Texture texture, ButtonState state
) {
	if (!button.Has<Texture>()) {
		button.Add<Texture>(texture);
	} else if (state == ButtonState::Current && button.Get<impl::ButtonToggled>()) {
		button.Add<Texture>(texture);
		return;
	}
	if (!button.Has<impl::ButtonTextureToggled>()) {
		button.Add<impl::ButtonTextureToggled>(texture);
	} else {
		auto& c{ button.Get<impl::ButtonTextureToggled>() };
		c.Get(state) = texture;
	}
}

Color GetButtonTintToggled(Entity button, ButtonState state) {
	const auto c{ button.Has<impl::ButtonTintToggled>() ? button.Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

Entity SetButtonTintToggled(
	Entity button, Color color, ButtonState state
) {
	if (!button.Has<impl::ButtonTintToggled>()) {
		auto& c{ button.Add<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	} else {
		auto& c{ button.Get<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	}
}
*/

// TODO: Fix.
/*
Entity Load(
	Entity button, std::string_view button_key, Entity toggle_button
) {
	PTGN_ASSERT(button.Has<impl::ToggleButtonGroupInfo>());

	auto& info{ button.Get<impl::ToggleButtonGroupInfo>() };

	toggle_button.Add<ToggleButtonGroupKey>(button_key);

	if (auto it{ info.buttons.find(button_key) }; it == info.buttons.end()) {
		auto [new_it, inserted] = info.buttons.try_emplace(button_key, std::move(toggle_button));
		PTGN_ASSERT(inserted, "Failed to insert toggle button");
		AddToggleScript(new_it->second);
		return new_it->second;
	} else {
		it->second = std::move(toggle_button);
		return it->second;
	}
}

void Unload(Entity button, std::string_view button_key) {
	PTGN_ASSERT(button.Has<impl::ToggleButtonGroupInfo>());

	auto& info{ button.Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(button_key) };

	if (it == info.buttons.end()) {
		return;
	}

	info.buttons.erase(it);
}

Entity GetActive(Entity button) {
	PTGN_ASSERT(button.Has<impl::ToggleButtonGroupInfo>());

	auto& info{ button.Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(info.active) };

	if (it == info.buttons.end() || !it->second.IsToggled()) {
		return {};
	}

	return it->second;
}

void SetActive(Entity button, std::string_view button_key) {
	PTGN_ASSERT(button.Has<impl::ToggleButtonGroupInfo>());

	auto& info{ button.Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(button_key) };

	PTGN_ASSERT(
		it != info.buttons.end(),
		"Cannot set non-existent toggle button key to active: ", button_key.GetKey()
	);

	for (auto& [key, toggle_button] : info.buttons) {
		toggle_button.SetToggled(false);
	}

	info.active = button_key;

	it->second.SetToggled(true);
}

void AddToggleScript(Entity toggle_button_group, Entity toggle_button) {
	AddScript<impl::ToggleButtonGroupScript>(toggle_button, toggle_button_group);
}
*/

Entity CreateButton(Scene& scene) {
	auto button{ scene.CreateEntity() };

	Show(button);
	SetDraw<impl::ButtonDraw>(button);

	SetInteractive(button);
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	AddScript<impl::InternalButtonScript>(button);
	EnableButton(button);

	return button;
}

Entity CreateTextButton(Scene& scene, std::string_view text_content, Color text_color) {
	auto text_button{ CreateButton(scene) };

	SetButtonText(text_button, text_content, text_color);

	return text_button;
}

// TODO: Fix.
// Entity CreateToggleButton(Scene& scene, bool toggled) {
//	auto toggle_button{ CreateButton(scene) };
//
//	AddScript<impl::ToggleButtonScript>(toggle_button);
//	toggle_button.Add<impl::ButtonToggled>(toggled);
//
//	return toggle_button;
//}

// TODO: Fix.
// Entity CreateToggleButtonGroup(Scene& scene) {
//	auto toggle_button_group{ scene.CreateEntity() };
//
//	toggle_button_group.Add<impl::ToggleButtonGroupInfo>();
//
//	return toggle_button_group;
//}

// TODO: Fix.
// Entity CreateAnimatedButton(
//	Scene& scene, const V2_float& button_size, const Animation& activate_animation,
//	const Animation& hover_animation, bool force_start_on_activate, bool force_start_on_hover_start,
//	bool stop_on_hover_stop
//) {
//	auto button{ CreateButton(scene) };
//
//	if (activate_animation) {
//		// TODO: Change this once AddChild takes an Entity and not Entity&.
//		Entity activate{ activate_animation };
//		AddChild(button, activate, "activate_animation");
//	}
//	if (hover_animation) {
//		// TODO: Change this once AddChild takes an Entity and not Entity&.
//		Entity hover{ hover_animation };
//		AddChild(button, hover, "hover_animation");
//	}
//
//	SetButtonSize(button, button_size);
//
//	AddScript<impl::AnimatedButtonScript>(
//		button, activate_animation, hover_animation, force_start_on_activate,
//		force_start_on_hover_start, stop_on_hover_stop
//	);
//
//	return button;
//}

} // namespace ptgn