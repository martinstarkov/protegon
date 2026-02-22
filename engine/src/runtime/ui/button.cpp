#include "runtime/ui/button.h"

#include <functional>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/component.h"
#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/input/interactive.h"
#include "runtime/input/scene_input.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void InternalButtonScript::OnEvent(EventDispatcher d) {
	d.Dispatch<MouseMoveOver>([this](MouseMoveOver& e) { OnMouseMoveOver(); });
	d.Dispatch<MouseMoveOut>([this](MouseMoveOut& e) { OnMouseMoveOut(); });
	d.Dispatch<MousePressedOver>([this](MousePressedOver& e) { OnMousePressedOver(e.button); });
	d.Dispatch<MousePressedOut>([this](MousePressedOut& e) { OnMousePressedOut(e.button); });
	d.Dispatch<MouseReleasedOver>([this](MouseReleasedOver& e) { OnMouseReleasedOver(e.button); });
	d.Dispatch<MouseReleasedOut>([this](MouseReleasedOut& e) { OnMouseReleasedOut(e.button); });
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
//	PTGN_ASSERT(self.Has<ToggleButtonGroupKey>());
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

	if (button_texture) {
		auto texture_tint{ GetEffectiveColor<impl::ButtonTint, impl::ButtonTintToggled>(
			button, is_toggled, color::White
		) };

		if (texture_tint.a) {
			// TODO: Fix.
			/*renderer.DrawTexture(
				*button_texture, transform, button_size, button_origin,
				Tint{ texture_tint.Normalized() * tint_n }, depth, blend_mode, camera, pre_fx,
				post_fx, Sprite{ button }.GetTextureCoordinates(false)
			);*/
		}
	} else {
		auto line_width{ button.GetOrDefault<impl::ButtonBackgroundWidth>() };

		if (line_width != 0.0f) {
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

	if (line_width != 0.0f) {
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

	if (button.Has<impl::ButtonTextFixedSize>()) {
		text_size = button.Get<impl::ButtonTextFixedSize>();
		if (NearlyEqual(text_size.x, 0.0f)) {
			text_size.x = button_size.x;
		}
		if (NearlyEqual(text_size.y, 0.0f)) {
			text_size.y = button_size.y;
		}
	}

	auto text_camera{ GetNonPrimaryCamera(text).value_or(camera) };

	// impl::DrawText(text, text_size, text_camera, tint, button_origin, button_size);
}

void OnButtonActivate(const std::function<void()>& callback) {
	AddScript<impl::ButtonActivateScript>(*this, callback);
}

void OnButtonHover(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverScript>(*this, callback);
}

void OnButtonHoverStart(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStartScript>(*this, callback);
}

void OnButtonHoverStop(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStopScript>(*this, callback);
}

void EnableButton(bool enable_hover, bool reset_state) {
	return SetButtonEnabled(true, enable_hover, reset_state);
}

void DisableButton(bool disable_hover, bool reset_state) {
	return SetButtonEnabled(false, !disable_hover, reset_state);
}

void SetButtonEnabled(bool enable_activation, bool enable_hover, bool reset_state) {
	Add<impl::ButtonEnabled>(enable_activation, enable_hover);
	if (reset_state) {
		auto& state{ Get<impl::InternalButtonState>() };
		state = impl::InternalButtonState::IdleUp;
	}
}

bool Button::IsButtonEnabled(bool check_for_hover_enabled) const {
	if (!Has<impl::ButtonEnabled>()) {
		return false;
	}
	const auto& enabled{ Get<impl::ButtonEnabled>() };
	if (check_for_hover_enabled) {
		return enabled.hover;
	}
	return enabled.activate;
}

V2_float Button::GetSize() const {
	V2_float size;

	const impl::Texture* button_texture{ nullptr };

	Texture button_texture;

	if (Has<Texture>()) {
		button_texture = Get<Texture>();
	}

	if (Application::Get().texture.Has(button_texture)) {
		button_texture = &button_texture.GetTexture();
	}

	if (button_texture != nullptr && size.IsZero()) {
		size = button_texture->GetSize();
	}

	if (!size.IsZero()) {
		return size;
	}

	if (Has<Rect>()) {
		size = Get<Rect>().GetSize();
	} else if (Has<Circle>()) {
		size = V2_float{ Get<Circle>().radius * 2.0f };
	}

	return size;
}

void Button::SetSize(const V2_float& size) {
	Remove<Circle>();
	if (Has<Rect>()) {
		Get<Rect>() = Rect{ size };
	} else {
		Add<Rect>(size);
	}
	if (IsInteractive(*this)) {
		ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Rect>(size);
		AddInteractable(*this, std::move(shape));
	}
}

void Button::SetRadius(float radius) {
	Remove<Rect>();
	if (Has<Circle>()) {
		Get<Circle>() = Circle{ radius };
	} else {
		Add<Circle>(radius);
	}
	if (IsInteractive(*this)) {
		ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Circle>(radius);
		AddInteractable(*this, std::move(shape));
	}
}

Color Button::GetBackgroundColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonColor>() ? Get<impl::ButtonColor>() : impl::ButtonColor{} };
	return c.Get(state);
}

void Button::SetBackgroundColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColor>()) {
		Add<impl::ButtonColor>(color);
	} else {
		auto& c{ Get<impl::ButtonColor>() };
		c.Get(state) = color;
	}
}

void Button::SetText(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetManager(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(*this, GetManager(), state, content, text_color, font_size, font, text_properties);
	}
}

Text Button::GetText(ButtonState state) const {
	return Get<impl::ButtonText>().GetValid(state);
}

Color Button::GetTextColor(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextColor(state);
}

void Button::SetTextColor(Color text_color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetManager(), state, TextContent{}, text_color, FontSize{}, Handle<Font>{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(text_color);
	}
}

TextContent Button::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextContent(state);
}

void Button::SetTextContent(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetManager(), state, content, Color{}, FontSize{}, Handle<Font>{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetContent(content);
	}
}

TextJustify Button::GetTextJustify(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextJustify(state);
}

void Button::SetTextJustify(const TextJustify& justify, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		TextProperties p{};
		p.justify = justify;
		Add<impl::ButtonText>(
			*this, GetManager(), state, TextContent{}, Color{}, FontSize{}, Handle<Font>{}, p
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetTextJustify(justify);
	}
}

V2_float Button::GetTextFixedSize() const {
	return GetOrDefault<impl::ButtonTextFixedSize>();
}

void Button::SetTextFixedSize(const V2_float& size) {
	Add<impl::ButtonTextFixedSize>(size);
}

void Button::ClearTextFixedSize() {
	Remove<impl::ButtonTextFixedSize>();
}

FontSize Button::GetFontSize(ButtonState state) const {
	return Get<impl::ButtonText>().GetFontSize(state);
}

void Button::SetFontSize(std::optional<float> font_size, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetManager(), state, TextContent{}, Color{}, font_size, Handle<Font>{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetFontSize(font_size);
	}
}

Texture Button::GetTextureKey(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<Texture>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return Get<impl::ButtonTexture>().Get(state);
}

void Button::SetTextureKey(Texture texture, ButtonState state) {
	if (IsInteractive(*this) && GetInteractables(*this).empty()) {
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		auto size{ texture.GetSize() };
		shape.Add<Rect>(size);
		AddInteractable(*this, std::move(shape));
	}
	if (!Has<Texture>()) {
		Add<Texture>(texture);
	} else if (state == ButtonState::Current) {
		Add<Texture>(texture);
		return;
	}
	if (!Has<impl::ButtonTexture>()) {
		Add<impl::ButtonTexture>(texture);
	} else {
		auto& c{ Get<impl::ButtonTexture>() };
		c.Get(state) = texture;
	}
}

void Button::SetDisabledTextureKey(Texture texture) {
	if (!texture) {
		Remove<impl::ButtonDisabledTexture>();
	} else {
		Add<impl::ButtonDisabledTexture>(texture);
	}
}

Texture Button::GetDisabledTextureKey() const {
	PTGN_ASSERT(
		Has<impl::ButtonDisabledTexture>(),
		"Cannot retrieve disabled texture key as it has not been set for the button"
	);
	return Get<impl::ButtonDisabledTexture>();
}

Color Button::GetButtonTint(ButtonState state) const {
	const auto c{ Has<impl::ButtonTint>() ? Get<impl::ButtonTint>() : impl::ButtonTint{} };
	return c.Get(state);
}

void Button::SetButtonTint(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTint>()) {
		auto& c{ Add<impl::ButtonTint>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTint>() };
		c.Get(state) = color;
	}
}

Color Button::GetBorderColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColor>() ? Get<impl::ButtonBorderColor>()
												 : impl::ButtonBorderColor{} };
	return c.Get(state);
}

void Button::SetBorderColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonBorderColor>()) {
		Add<impl::ButtonBorderColor>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColor>() };
		c.Get(state) = color;
	}
}

float Button::GetBackgroundLineWidth() const {
	return Has<impl::ButtonBackgroundWidth>() ? Get<impl::ButtonBackgroundWidth>()
											  : impl::ButtonBackgroundWidth{};
}

void Button::SetBackgroundLineWidth(float line_width) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Invalid button background line width");
	if (line_width != -1.0f && line_width < 1.0f) {
		Remove<impl::ButtonBackgroundWidth>();
	} else {
		Add<impl::ButtonBackgroundWidth>(line_width);
	}
}

float Button::GetBorderWidth() const {
	return Has<impl::ButtonBorderWidth>() ? Get<impl::ButtonBorderWidth>()
										  : impl::ButtonBorderWidth{};
}

void Button::SetBorderWidth(float line_width) {
	PTGN_ASSERT(line_width >= 1.0f || line_width == 0.0f, "Cannot set negative border width");
	if (line_width == 0.0f) {
		Remove<impl::ButtonBorderWidth>();
	} else {
		Add<impl::ButtonBorderWidth>(line_width);
	}
}

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

ButtonState Button::GetState() const {
	PTGN_ASSERT(Has<impl::InternalButtonState>());
	const auto& state{ Get<impl::InternalButtonState>() };
	if (state == impl::InternalButtonState::Hover ||
		state == impl::InternalButtonState::HoverPressed) {
		return ButtonState::Hover;
	} else if (state == impl::InternalButtonState::Pressed ||
			   state == impl::InternalButtonState::HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

void Button::Activate() {
	if (!IsButtonEnabled(false) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonActivate);
}

void Button::StartHover() {
	if (!IsButtonEnabled(true) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonHoverStart);
}

void Button::ContinueHover() {
	if (!IsButtonEnabled(true) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonHover);
}

void Button::StopHover() {
	if (!IsButtonEnabled(true) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonHoverStop);
}

bool ToggleButton::IsToggled() const {
	return Get<impl::ButtonToggled>();
}

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	auto& t{ Get<impl::ButtonToggled>() };
	t = toggled;
}

ToggleButton& ToggleButton::Toggle() {
	auto& toggled{ Get<impl::ButtonToggled>() };
	toggled = !toggled;
}

Color ToggleButton::GetBackgroundColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonColorToggled>() ? Get<impl::ButtonColorToggled>()
												  : impl::ButtonColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetBackgroundColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColorToggled>()) {
		Add<impl::ButtonColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonColorToggled>() };
		c.Get(state) = color;
	}
}

Color ToggleButton::GetTextColorToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextColor(state);
}

ToggleButton& ToggleButton::SetTextColorToggled(Color text_color, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetManager(), state, TextContent{}, text_color, FontSize{}, Handle<Font>{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(text_color);
	}
}

TextContent ToggleButton::GetTextContentToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextContent(state);
}

ToggleButton& ToggleButton::SetTextContentToggled(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetManager(), state, content, Color{}, FontSize{}, Handle<Font>{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
}

ToggleButton& ToggleButton::SetTextToggled(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetManager(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(*this, GetManager(), state, content, text_color, font_size, font, text_properties);
	}
}

Text ToggleButton::GetTextToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetValid(state);
}

Color ToggleButton::GetBorderColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColorToggled>() ? Get<impl::ButtonBorderColorToggled>()
														: impl::ButtonBorderColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetBorderColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonBorderColorToggled>()) {
		Add<impl::ButtonBorderColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColorToggled>() };
		c.Get(state) = color;
	}
}

Texture ToggleButton::GetTextureKeyToggled(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<Texture>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return Get<impl::ButtonTextureToggled>().Get(state);
}

ToggleButton& ToggleButton::SetTextureKeyToggled(Texture texture, ButtonState state) {
	if (!Has<Texture>()) {
		Add<Texture>(texture);
	} else if (state == ButtonState::Current && Get<impl::ButtonToggled>()) {
		Add<Texture>(texture);
		return;
	}
	if (!Has<impl::ButtonTextureToggled>()) {
		Add<impl::ButtonTextureToggled>(texture);
	} else {
		auto& c{ Get<impl::ButtonTextureToggled>() };
		c.Get(state) = texture;
	}
}

Color ToggleButton::GetButtonTintToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonTintToggled>() ? Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetButtonTintToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTintToggled>()) {
		auto& c{ Add<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	}
}

ToggleButton& ToggleButtonGroup::Load(
	const ToggleButtonGroupKey& button_key, ToggleButton&& toggle_button
) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

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

void ToggleButtonGroup::Unload(const ToggleButtonGroupKey& button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(button_key) };

	if (it == info.buttons.end()) {
		return;
	}

	info.buttons.erase(it);
}

ToggleButton ToggleButtonGroup::GetActive() const {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(info.active) };

	if (it == info.buttons.end() || !it->second.IsToggled()) {
		return {};
	}

	return it->second;
}

void ToggleButtonGroup::SetActive(const ToggleButtonGroupKey& button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

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

void ToggleButtonGroup::AddToggleScript(ToggleButton& target) {
	AddScript<impl::ToggleButtonGroupScript>(target, *this);
}

Button CreateButton(Scene& scene) {
	Button button{ scene.CreateEntity() };

	Show(button);
	SetDraw<Button>(button);

	SetInteractive(button);
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

Button CreateTextButton(Scene& scene, std::string_view text_content, Color text_color) {
	Button text_button{ CreateButton(scene) };

	text_button.SetText(text_content, text_color);

	return text_button;
}

ToggleButton CreateToggleButton(Scene& scene, bool toggled) {
	ToggleButton toggle_button{ CreateButton(scene) };

	AddScript<impl::ToggleButtonScript>(toggle_button);
	toggle_button.Add<impl::ButtonToggled>(toggled);

	return toggle_button;
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup toggle_button_group{ scene.CreateEntity() };

	toggle_button_group.Add<impl::ToggleButtonGroupInfo>();

	return toggle_button_group;
}

Button CreateAnimatedButton(
	Scene& scene, const V2_float& button_size, const Animation& activate_animation,
	const Animation& hover_animation, bool force_start_on_activate, bool force_start_on_hover_start,
	bool stop_on_hover_stop
) {
	auto button{ CreateButton(scene) };

	if (activate_animation) {
		// TODO: Change this once AddChild takes an Entity and not Entity&.
		Entity activate{ activate_animation };
		AddChild(button, activate, "activate_animation");
	}
	if (hover_animation) {
		// TODO: Change this once AddChild takes an Entity and not Entity&.
		Entity hover{ hover_animation };
		AddChild(button, hover, "hover_animation");
	}

	button.SetSize(button_size);

	AddScript<impl::AnimatedButtonScript>(
		button, activate_animation, hover_animation, force_start_on_activate,
		force_start_on_hover_start, stop_on_hover_stop
	);

	return button;
}

} // namespace ptgn