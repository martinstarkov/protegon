#include "ui/button.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "components/generic.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "event/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

void ButtonColor::SetToState(ButtonState state) {
	switch (state) {
		case ButtonState::Default: current_ = default_; break;
		case ButtonState::Hover:   current_ = hover_; break;
		case ButtonState::Pressed: current_ = pressed_; break;
		case ButtonState::Current: break;
		default:				   PTGN_ERROR("Unrecognized button state");
	}
}

} // namespace impl

Button::Button(ecs::Manager& manager) : GameObject{ manager } {
	SetVisible(true);
	SetEnabled(true);

	Add<impl::ButtonTag>();
	Add<Interactive>();
	Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

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

	Add<callback::MouseUp>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Pressed) {
				StateChange(e, impl::InternalButtonState::Hover);
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
	auto c{ Has<impl::ButtonColor>() ? Get<impl::ButtonColor>() : impl::ButtonColor{} };
	switch (state) {
		case ButtonState::Current: return c.current_;
		case ButtonState::Default: return c.default_;
		case ButtonState::Hover:   return c.hover_;
		case ButtonState::Pressed: return c.pressed_;
		default:				   PTGN_ERROR("Invalid button state");
	}
}

Button& Button::SetBackgroundColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColor>()) {
		Add<impl::ButtonColor>(color);
	} else {
		auto& c{ Get<impl::ButtonColor>() };
		switch (state) {
			case ButtonState::Default: c.default_ = color; break;
			case ButtonState::Hover:   c.hover_ = color; break;
			case ButtonState::Pressed: c.pressed_ = color; break;
			case ButtonState::Current: c.current_ = color; break;
			default:				   PTGN_ERROR("Invalid button state");
		}
	}
	return *this;
}

Button& Button::SetText(
	std::string_view content, const Color& text_color = color::Black,
	std::string_view font_key = "", ButtonState state = ButtonState::Default
) {
	// TODO: Fix this.
	return *this;
}

Color Button::GetTextColor(ButtonState state) const {
	// TODO: Fix.
	return Color();
}

Button& Button::SetTextColor(const Color& color, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

ecs::Entity Button::GetTexture(ButtonState state) const {
	// TODO: insert return statement here
	// TODO: Fix.
	return {};
}

Button& Button::SetTexture(std::string_view texture_key, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

Color Button::GetTint(ButtonState state) const {
	// TODO: Fix.
	return Tint();
}

Button& Button::SetTint(const Color& color, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

std::string_view Button::GetTextContent(ButtonState state) const {
	// TODO: Fix.
	return std::string_view();
}

Button& Button::SetTextContent(std::string_view content, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

ecs::Entity Button::GetText(ButtonState state) const {
	// TODO: Fix.
	return {};
}

bool Button::IsBordered() const {
	// TODO: Fix.
	return false;
}

Button& Button::SetBordered(bool bordered) {
	// TODO: insert return statement here
	return *this;
}

Color Button::GetBorderColor(ButtonState state) const {
	// TODO: Fix.
	return Color();
}

Button& Button::SetBorderColor(const Color& color, ButtonState state) {
	// TODO: insert return statement here
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
	// TODO: Fix.
	return -1.0f;
}

Button& Button::SetBackgroundLineWidth(float line_width) {
	// TODO: insert return statement here
	return *this;
}

float Button::GetBorderWidth() const {
	// TODO: Fix.
	return 0.0f;
}

Button& Button::SetBorderWidth(float line_width) {
	// TODO: insert return statement here
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

ToggleButton::ToggleButton(ecs::Manager& manager, bool toggled) : Button{ manager } {
	if (!IsAlive()) {
		return;
	}
	Add<impl::ButtonToggled>(toggled);
}

void ToggleButton::Activate() {
	Toggle();
	Button::Activate();
}

bool ToggleButton::IsToggled() const {
	// TODO: Fix.
	return false;
}

ToggleButton& ToggleButton::Toggle() {
	// TODO: insert return statement here
	return *this;
}

Color ToggleButton::GetColorToggled(ButtonState state) const {
	// TODO: Fix.
	return Color();
}

ToggleButton& ToggleButton::SetColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColorToggled>()) {
		Add<impl::ButtonColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonColorToggled>() };
		switch (state) {
			case ButtonState::Default: c.default_ = color; break;
			case ButtonState::Hover:   c.hover_ = color; break;
			case ButtonState::Pressed: c.pressed_ = color; break;
			case ButtonState::Current: c.current_ = color; break;
			default:				   PTGN_ERROR("Invalid button state");
		}
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
	// TODO: Fix.
	return Color();
}

ToggleButton& ToggleButton::SetBorderColorToggled(const Color& color, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

ToggleButton& ToggleButton::OnToggle(const ButtonCallback& callback) {
	// TODO: insert return statement here
	return *this;
}

const impl::Texture& ToggleButton::GetTextureToggled(ButtonState state) const {
	// TODO: insert return statement here
	return {};
}

ToggleButton& ToggleButton::SetTextureToggled(std::string_view texture_key, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

Color ToggleButton::GetTintToggled(ButtonState state) const {
	// TODO: Fix.
	return Color();
}

ToggleButton& ToggleButton::SetTintToggled(const Color& color, ButtonState state) {
	// TODO: insert return statement here
	return *this;
}

} // namespace ptgn
