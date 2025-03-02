#include "ui/button.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "components/generic.h"
#include "components/input.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "event/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

void ButtonColor::SetToState(ButtonState state) {
	current_ = Get(state);
}

const Color& ButtonColor::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Current: return current_;
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		default:				   PTGN_ERROR("Invalid button state");
	}
}

Color& ButtonColor::Get(ButtonState state) {
	return const_cast<Color&>(std::as_const(*this).Get(state));
}

ButtonText::ButtonText(
	const ecs::Entity& parent, ecs::Manager& manager, ButtonState state,
	const TextContent& text_content, const TextColor& text_color, const FontKey& font_key
) {
	Set(parent, manager, ButtonState::Default, text_content, text_color, font_key);
	if (state != ButtonState::Default) {
		Set(parent, manager, state, text_content, text_color, font_key);
	}
}

const Text& ButtonText::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

const Text& ButtonText::GetValid(ButtonState state) const {
	auto* text{ &Get(state) };
	if (*text == Text{}) {
		text = &Get(ButtonState::Default);
	}
	return *text;
}

Text& ButtonText::GetValid(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).GetValid(state));
}

Text& ButtonText::Get(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).Get(state));
}

Color ButtonText::GetColor(ButtonState state) const {
	return GetValid(state).GetColor();
}

std::string_view ButtonText::GetContent(ButtonState state) const {
	return GetValid(state).GetContent();
}

void ButtonText::Set(
	const ecs::Entity& parent, ecs::Manager& manager, ButtonState state,
	const TextContent& text_content, const TextColor& text_color, const FontKey& font_key
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	auto& text{ Get(state) };
	if (text == Text{}) {
		text = Text{ manager, text_content, text_color, font_key };
		text.SetParent(parent);
	} else {
		text.SetParameter(TextColor{ text_color }, false);
		text.SetParameter(TextContent{ text_content }, false);
		text.SetParameter(FontKey{ font_key }, false);
		text.RecreateTexture();
	}
}

const TextureKey& ButtonTexture::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

TextureKey& ButtonTexture::Get(ButtonState state) {
	return const_cast<TextureKey&>(std::as_const(*this).Get(state));
}
} // namespace impl

Button::Button(ecs::Manager& manager, bool) : GameObject{ manager } {}

Button::Button(ecs::Manager& manager) : Button{ manager, true } {
	Setup();
	SetupCallbacks(nullptr);
}

void Button::Setup() {
	SetVisible(true);
	SetEnabled(true);

	Add<impl::ButtonTag>();
	Add<Interactive>();
	Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);
}

void Button::SetupCallbacks(const std::function<void()>& internal_on_activate) {
	Add<callback::MouseEnter>([e = GetEntity()](auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::IdleUp) {
			StateChange(e, impl::InternalButtonState::Hover);
			StartHover(e);
		} else if (state == impl::InternalButtonState::IdleDown) {
			StateChange(e, impl::InternalButtonState::HoverPressed);
			StartHover(e);
		} else if (state == impl::InternalButtonState::HeldOutside) {
			StateChange(e, impl::InternalButtonState::Pressed);
		}
	});

	Add<callback::MouseLeave>([e = GetEntity()](auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::Hover) {
			StateChange(e, impl::InternalButtonState::IdleUp);
			StopHover(e);
		} else if (state == impl::InternalButtonState::Pressed) {
			StateChange(e, impl::InternalButtonState::HeldOutside);
			StopHover(e);
		} else if (state == impl::InternalButtonState::HoverPressed) {
			StateChange(e, impl::InternalButtonState::IdleDown);
			StopHover(e);
		}
	});

	Add<callback::MouseDown>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Hover) {
				StateChange(e, impl::InternalButtonState::Pressed);
			}
		}
	});

	Add<callback::MouseDownOutside>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleUp) {
				StateChange(e, impl::InternalButtonState::IdleDown);
			}
		}
	});

	Add<callback::MouseUp>([internal_on_activate, e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Pressed) {
				StateChange(e, impl::InternalButtonState::Hover);
				if (internal_on_activate != nullptr) {
					std::invoke(internal_on_activate);
				}
				Activate(e);
			} else if (state == impl::InternalButtonState::HoverPressed) {
				StateChange(e, impl::InternalButtonState::Hover);
			}
		}
	});

	Add<callback::MouseUpOutside>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleDown) {
				StateChange(e, impl::InternalButtonState::IdleUp);
			} else if (state == impl::InternalButtonState::HeldOutside) {
				StateChange(e, impl::InternalButtonState::IdleUp);
			}
		}
	});
}

Button& Button::AddInteractableRect(const V2_float& size, Origin origin, const V2_float& offset) {
	if (Has<InteractiveRects>()) {
		auto& interactives{ Get<InteractiveRects>() };
		interactives.rects.push_back({ Rect{ size, origin }, offset });
	} else {
		Add<InteractiveRects>(size, origin, offset);
	}
	return *this;
}

Button& Button::AddInteractableCircle(float radius, const V2_float& offset) {
	if (Has<InteractiveCircles>()) {
		auto& interactives{ Get<InteractiveCircles>() };
		interactives.circles.push_back({ Circle{ radius }, offset });
	} else {
		Add<InteractiveCircles>(radius, offset);
	}
	return *this;
}

Button& Button::SetRect(const V2_float& size, Origin origin) {
	Remove<Circle>();
	if (Has<Rect>()) {
		auto& rect{ Get<Rect>() };
		rect.size	= size;
		rect.origin = origin;
	} else {
		Add<Rect>(size, origin);
	}
	return *this;
}

Button& Button::SetCircle(float radius) {
	Remove<Rect>();
	if (Has<Circle>()) {
		Get<Circle>().radius = radius;
	} else {
		Add<Circle>(radius);
	}
	return *this;
}

void Button::StateChange(impl::InternalButtonState new_state) {
	StateChange(GetEntity(), new_state);
}

void Button::Activate() {
	Activate(GetEntity());
}

void Button::StartHover() {
	StartHover(GetEntity());
}

void Button::StopHover() {
	StopHover(GetEntity());
}

ButtonState Button::GetState() const {
	return GetState(GetEntity());
}

Color Button::GetBackgroundColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonColor>() ? Get<impl::ButtonColor>() : impl::ButtonColor{} };
	return c.Get(state);
}

Button& Button::SetBackgroundColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColor>()) {
		Add<impl::ButtonColor>(color);
	} else {
		auto& c{ Get<impl::ButtonColor>() };
		c.Get(state) = color;
	}
	return *this;
}

Button& Button::SetText(
	std::string_view content, const Color& text_color, std::string_view font_key, ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	}
	return *this;
}

const Text& Button::GetText(ButtonState state) const {
	return Get<impl::ButtonText>().Get(state);
}

Text& Button::GetText(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).GetText(state));
}

Color Button::GetTextColor(ButtonState state) const {
	return Get<impl::ButtonText>().GetColor(state);
}

Button& Button::SetTextColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{}, TextColor{ color }, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(color);
	}
	return *this;
}

std::string_view Button::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetContent(state);
}

Button& Button::SetTextContent(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{}, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

TextureKey Button::GetTextureKey(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureKey>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureKey>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return Get<impl::ButtonTexture>().Get(state);
}

Button& Button::SetTextureKey(std::string_view texture_key, ButtonState state) {
	TextureKey tk{ texture_key };
	if (!Has<TextureKey>()) {
		Add<TextureKey>(tk);
	} else if (state == ButtonState::Current) {
		Add<TextureKey>(tk);
		return *this;
	}
	if (!Has<impl::ButtonTexture>()) {
		Add<impl::ButtonTexture>(tk);
	} else {
		auto& c{ Get<impl::ButtonTexture>() };
		c.Get(state) = tk;
	}
	return *this;
}

Color Button::GetTint(ButtonState state) const {
	const auto c{ Has<impl::ButtonTint>() ? Get<impl::ButtonTint>() : impl::ButtonTint{} };
	return c.Get(state);
}

Button& Button::SetTint(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTint>()) {
		auto& c{ Add<impl::ButtonTint>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTint>() };
		c.Get(state) = color;
	}
	return *this;
}

Color Button::GetBorderColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColor>() ? Get<impl::ButtonBorderColor>()
												 : impl::ButtonBorderColor{} };
	return c.Get(state);
}

Button& Button::SetBorderColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonBorderColor>()) {
		Add<impl::ButtonBorderColor>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColor>() };
		c.Get(state) = color;
	}
	return *this;
}

TextJustify Button::GetTextJustify() const {
	// TODO: Fix.
	return TextJustify();
}

Button& Button::SetTextJustify(const TextJustify& justify) {
	// TODO: insert return statement here
	return *this;
}

V2_float Button::GetTextSize() const {
	// TODO: Fix.
	return V2_float();
}

Button& Button::SetTextSize(const V2_float& size) {
	// TODO: insert return statement here
	return *this;
}

std::int32_t Button::GetFontSize() const {
	// TODO: Fix.
	return std::int32_t();
}

Button& Button::SetFontSize(std::int32_t font_size) {
	// TODO: insert return statement here
	return *this;
}

float Button::GetBackgroundLineWidth() const {
	return Has<impl::ButtonBackgroundWidth>() ? Get<impl::ButtonBackgroundWidth>()
											  : impl::ButtonBackgroundWidth{};
}

Button& Button::SetBackgroundLineWidth(float line_width) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Invalid button background line width");
	if (line_width != -1.0f && line_width < 1.0f) {
		Remove<impl::ButtonBackgroundWidth>();
	} else {
		Add<impl::ButtonBackgroundWidth>(line_width);
	}
	return *this;
}

float Button::GetBorderWidth() const {
	return Has<impl::ButtonBorderWidth>() ? Get<impl::ButtonBorderWidth>()
										  : impl::ButtonBorderWidth{};
}

Button& Button::SetBorderWidth(float line_width) {
	PTGN_ASSERT(line_width >= 1.0f || line_width == 0.0f, "Cannot set negative border width");
	if (line_width == 0.0f) {
		Remove<impl::ButtonBorderWidth>();
	} else {
		Add<impl::ButtonBorderWidth>(line_width);
	}
	return *this;
}

Button& Button::OnHoverStart(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonHoverStart>();
	} else {
		Add<impl::ButtonHoverStart>(callback);
	}
	return *this;
}

Button& Button::OnHoverStop(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonHoverStop>();
	} else {
		Add<impl::ButtonHoverStop>(callback);
	}
	return *this;
}

Button& Button::OnActivate(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonActivate>();
	} else {
		Add<impl::ButtonActivate>(callback);
	}
	return *this;
}

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

ButtonState Button::GetState(const ecs::Entity& e) {
	PTGN_ASSERT(e.Has<impl::InternalButtonState>());
	const auto& state{ e.Get<impl::InternalButtonState>() };
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

void Button::StateChange(const ecs::Entity& e, impl::InternalButtonState new_state) {
	e.Get<impl::InternalButtonState>() = new_state;
	auto state{ GetState(e) };
	if (e.Has<impl::ButtonColor>()) {
		e.Get<impl::ButtonColor>().SetToState(state);
	}
	if (e.Has<impl::ButtonColorToggled>()) {
		e.Get<impl::ButtonColorToggled>().SetToState(state);
	}
	if (e.Has<impl::ButtonTint>()) {
		e.Get<impl::ButtonTint>().SetToState(state);
	}
	if (e.Has<impl::ButtonTintToggled>()) {
		e.Get<impl::ButtonTintToggled>().SetToState(state);
	}
	if (e.Has<impl::ButtonBorderColor>()) {
		e.Get<impl::ButtonBorderColor>().SetToState(state);
	}
	if (e.Has<impl::ButtonBorderColorToggled>()) {
		e.Get<impl::ButtonBorderColorToggled>().SetToState(state);
	}
	if (e.Has<TextureKey>()) {
		if (e.Has<impl::ButtonToggled>() && e.Has<impl::ButtonTextureToggled>()) {
			e.Get<TextureKey>() = e.Get<impl::ButtonTextureToggled>().Get(state);
		} else if (e.Has<impl::ButtonTexture>()) {
			e.Get<TextureKey>() = e.Get<impl::ButtonTexture>().Get(state);
		}
	}
}

void Button::Activate(const ecs::Entity& e) {
	if (e.Has<impl::ButtonActivate>()) {
		if (const auto& callback{ e.Get<impl::ButtonActivate>() }; callback != nullptr) {
			callback();
		}
	}
}

void Button::StartHover(const ecs::Entity& e) {
	if (e.Has<impl::ButtonHoverStart>()) {
		if (const auto& callback{ e.Get<impl::ButtonHoverStart>() }; callback != nullptr) {
			callback();
		}
	}
}

void Button::StopHover(const ecs::Entity& e) {
	if (e.Has<impl::ButtonHoverStop>()) {
		if (const auto& callback{ e.Get<impl::ButtonHoverStop>() }; callback != nullptr) {
			callback();
		}
	}
}

ToggleButton::ToggleButton(ecs::Manager& manager, bool toggled) : Button{ manager, true } {
	Button::Setup();
	Add<impl::ButtonToggled>(toggled);
	Button::SetupCallbacks([e = GetEntity()]() { Toggle(e); });
}

void ToggleButton::Activate() {
	Toggle();
	Button::Activate();
}

bool ToggleButton::IsToggled() const {
	return Get<impl::ButtonToggled>();
}

ToggleButton& ToggleButton::Toggle() {
	Toggle(GetEntity());
	return *this;
}

void ToggleButton::Toggle(const ecs::Entity& e) {
	auto& toggled{ e.Get<impl::ButtonToggled>() };
	toggled = !toggled;
	if (e.Has<impl::ButtonToggle>()) {
		if (const auto& callback{ e.Get<impl::ButtonToggle>() }; callback != nullptr) {
			callback();
		}
	}
}

Color ToggleButton::GetColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonColorToggled>() ? Get<impl::ButtonColorToggled>()
												  : impl::ButtonColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColorToggled>()) {
		Add<impl::ButtonColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonColorToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

Color ToggleButton::GetTextColorToggled(ButtonState state) const {
	// TODO: Fix.
	return Color();
}

ToggleButton& ToggleButton::SetTextColorToggled(const Color& color, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

std::string ToggleButton::GetTextContentToggled(ButtonState state) const {
	// TODO: Fix.
	return std::string();
}

ToggleButton& ToggleButton::SetTextContentToggled(const std::string& content, ButtonState state) {
	// TODO: insert return statement here
	return *this;
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
	return *this;
}

ToggleButton& ToggleButton::OnToggle(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonToggle>();
	} else {
		Add<impl::ButtonToggle>(callback);
	}
	return *this;
}

TextureKey ToggleButton::GetTextureKeyToggled(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureKey>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureKey>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return Get<impl::ButtonTextureToggled>().Get(state);
}

ToggleButton& ToggleButton::SetTextureKeyToggled(std::string_view texture_key, ButtonState state) {
	TextureKey tk{ texture_key };
	if (!Has<TextureKey>()) {
		Add<TextureKey>(tk);
	} else if (state == ButtonState::Current && Get<impl::ButtonToggled>()) {
		Add<TextureKey>(tk);
		return *this;
	}
	if (!Has<impl::ButtonTextureToggled>()) {
		Add<impl::ButtonTextureToggled>(tk);
	} else {
		auto& c{ Get<impl::ButtonTextureToggled>() };
		c.Get(state) = tk;
	}
	return *this;
}

Color ToggleButton::GetTintToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonTintToggled>() ? Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetTintToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTintToggled>()) {
		auto& c{ Add<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

} // namespace ptgn
