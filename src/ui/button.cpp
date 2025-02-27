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
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "utility/log.h"

// #include <cstdint>
// #include <limits>
// #include <type_traits>
// #include <unordered_map>
//
// #include "core/game.h"
// #include "event/event.h"
// #include "event/event_handler.h"
// #include "event/events.h"
// #include "event/input_handler.h"
// #include "event/mouse.h"
// #include "math/geometry/polygon.h"
// #include "math/math.h"
// #include "math/vector2.h"
// #include "renderer/color.h"
// #include "renderer/renderer.h"
// #include "renderer/text.h"
// #include "renderer/texture.h"
// #include "utility/assert.h"
// #include "utility/handle.h"
//
// namespace ptgn {
//
// void ButtonInstance::Activate() {
//	if (!enabled_) {
//		return;
//	}
//	bool recheck{ false };
//	if (internal_on_activate_ != nullptr) {
//		std::invoke(internal_on_activate_);
//		recheck = true;
//	}
//	if (on_activate_ != nullptr) {
//		std::invoke(on_activate_);
//		recheck = true;
//	}
//	if (recheck) {
//		RecheckState();
//	}
// }
//
// void ButtonInstance::StartHover() {
//	if (!enabled_) {
//		return;
//	}
//	if (on_hover_start_ != nullptr) {
//		std::invoke(on_hover_start_);
//	}
// }
//
// void ButtonInstance::StopHover() {
//	if (!enabled_) {
//		return;
//	}
//	if (on_hover_stop_ != nullptr) {
//		std::invoke(on_hover_stop_);
//	}
// }
//
// void ButtonInstance::Toggle() {
//	if (!enabled_) {
//		return;
//	}
//	if (toggleable_) {
//		toggled_ = !toggled_;
//		if (on_toggle_ != nullptr) {
//			std::invoke(on_toggle_);
//		}
//	}
//	Activate();
// }
//
// void ButtonInstance::Enable() {
//	enabled_ = true;
//	RecheckState();
//	if (on_enable_) {
//		std::invoke(on_enable_);
//	}
// }
//
// void ButtonInstance::Disable() {
//	button_state_ = impl::InternalButtonState::IdleUp;
//	if (on_hover_stop_ != nullptr) {
//		StopHover();
//	}
//	enabled_ = false;
//	if (on_disable_) {
//		std::invoke(on_disable_);
//	}
// }
//
// void ButtonInstance::Show() {
//	visibility_ = true;
//	RecheckState();
//	if (on_show_) {
//		std::invoke(on_show_);
//	}
// }
//
// void ButtonInstance::Hide() {
//	visibility_ = false;
//	if (on_hide_) {
//		std::invoke(on_hide_);
//	}
// }
//
// void ButtonInstance::RecheckState() {
//	if (!enabled_) {
//		return;
//	}
//	// Simulate a mouse move event to refresh button state.
//	MouseMoveEvent e{};
//	MouseMotionUpdate(
//		GetMousePosition(),
//		V2_int{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max() }, e
//	);
// }
//
// V2_float ButtonInstance::GetMousePosition() const {
//	return game.input.GetMousePosition(render_target_);
// }
//
// V2_float ButtonInstance::GetMousePositionPrevious() const {
//	return game.input.GetMousePositionPrevious(render_target_);
// }
//
// bool ButtonInstance::InsideRect(const V2_int& position) const {
//	return rect_.Overlaps(position);
// }
//
// void ButtonInstance::MouseMotionUpdate(
//	const V2_int& current, const V2_int& previous, const MouseMoveEvent& e
//) {
//	bool is_in{ InsideRect(current) };
//	if (is_in) {
//		OnMouseMove(e);
//	} else {
//		OnMouseMoveOutside(e);
//	}
//	if (bool was_in{ InsideRect(previous) }; !was_in && is_in) {
//		OnMouseEnter(e);
//	} else if (was_in && !is_in) {
//		OnMouseLeave(e);
//	}
// }
//
// void ButtonInstance::OnMouseEvent(MouseEvent type, const Event& event) {
//	if (!enabled_) {
//		return;
//	}
//	switch (type) {
//		case MouseEvent::Move: {
//			const auto& e{ static_cast<const MouseMoveEvent&>(event) };
//			MouseMotionUpdate(GetMousePosition(), GetMousePositionPrevious(), e);
//			break;
//		}
//		case MouseEvent::Down: {
//			if (const auto& e{ static_cast<const MouseDownEvent&>(event) };
//				InsideRect(GetMousePosition())) {
//				OnMouseDown(e);
//			} else {
//				OnMouseDownOutside(e);
//			}
//			break;
//		}
//		case MouseEvent::Up: {
//			if (const auto& e{ static_cast<const MouseUpEvent&>(event) };
//				InsideRect(GetMousePosition())) {
//				OnMouseUp(e);
//			} else {
//				OnMouseUpOutside(e);
//			}
//			break;
//		}
//		case MouseEvent::Scroll:
//			[[fallthrough]]; // TODO: Consider adding button where scrolling does something.
//		default: break;
//	}
// }
//
// } // namespace impl
//
// Button::Button(const Rect& rect) {
//	Create();
//	SetRect(rect);
// }
//
// void Button::Draw() {
//	PTGN_ASSERT(IsValid(), "Cannot draw invalid or uninitialized button");
//
//	auto& i{ Handle::Get() };
//	i.render_target_ = game.renderer.GetRenderTarget();
//
//	if (!i.visibility_) {
//		return;
//	}
//	// Current button resource state.
//	auto c{ impl::GetButtonResourceState(GetState(), i.toggled_, !i.enabled_) };
//	// Default button resource state.
//	auto d{ impl::ButtonResourceState::Default };
//
//	if (auto texture{ GetFinalResource(c, d, i.textures_) }; texture.IsValid()) {
//		TextureInfo info;
//		info.tint = GetFinalResource(c, d, i.texture_tint_colors_, color::White);
//		texture.Draw(i.rect_, info, i.render_layer_);
//	} else if (auto bg{ GetFinalResource(c, d, i.bg_colors_) }; bg != Color{}) {
//		if (i.radius_ > 0.0f) {
//			RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
//						   i.rect_.rotation };
//			r.Draw(bg, i.line_thickness_, i.render_layer_);
//		} else {
//			i.rect_.Draw(bg, i.line_thickness_, i.render_layer_);
//		}
//	}
//
//	if (auto text{ GetFinalResource(c, d, i.texts_) }; text.IsValid()) {
//		V2_float text_size{ i.text_size_ };
//		if (NearlyEqual(text_size.x, 0.0f)) {
//			text_size.x = i.rect_.size.x;
//		}
//		if (NearlyEqual(text_size.y, 0.0f)) {
//			text_size.y = i.rect_.size.y;
//		}
//		auto og_text_color{ text.GetColor() };
//		if (auto text_color{ GetFinalResource(c, d, i.text_colors_) }; text_color != Color{}) {
//			text.SetColor(text_color);
//		}
//		text.Draw(
//			{ i.rect_.Center(), text_size, i.text_alignment_, i.rect_.rotation },
//			i.render_layer_ + 1
//		);
//		text.SetColor(og_text_color);
//	}
//	if (i.bordered_) {
//		if (auto border_color{ GetFinalResource(c, d, i.border_colors_) };
//			border_color != Color{}) {
//			if (i.radius_ > 0.0f) {
//				RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
//							   i.rect_.rotation };
//				r.Draw(border_color, i.border_thickness_, i.render_layer_ + 2);
//			} else {
//				i.rect_.Draw(border_color, i.border_thickness_, i.render_layer_ + 2);
//			}
//		}
//	}
//	if (i.dropdown_.IsValid()) {
//		i.dropdown_.Draw(i.rect_, i.render_layer_);
//	}
// }
//
// bool Button::IsEnabled() const {
//	return IsValid() && Handle::Get().enabled_;
// }
//
// Button& Button::SetEnabled(bool enabled) {
//	Create();
//	auto& i{ Handle::Get() };
//	if (enabled == i.enabled_) {
//		return *this;
//	}
//	enabled ? i.Enable() : i.Disable();
//	return *this;
// }
//
// bool Button::IsVisible() const {
//	return IsValid() && Handle::Get().visibility_;
// }
//
// Button& Button::SetVisibility(bool enabled) {
//	Create();
//	auto& i{ Handle::Get() };
//	if (enabled == i.visibility_) {
//		return *this;
//	}
//	enabled ? i.Show() : i.Hide();
//	return *this;
// }
//
// Button& Button::Activate() {
//	Create();
//	Handle::Get().Activate();
//	return *this;
// }
//
// Button& Button::StartHover() {
//	Create();
//	Handle::Get().StartHover();
//	return *this;
// }
//
// Button& Button::StopHover() {
//	Create();
//	Handle::Get().StopHover();
//	return *this;
// }
//
// Button& Button::Toggle() {
//	Create();
//	Handle::Get().Toggle();
//	return *this;
// }
//
// Button& Button::Disable() {
//	return SetEnabled(false);
// }
//
// Button& Button::Enable() {
//	return SetEnabled(true);
// }
//
// Button& Button::Show() {
//	return SetVisibility(true);
// }
//
// Button& Button::Hide() {
//	return SetVisibility(false);
// }
//
// Rect Button::GetRect() const {
//	PTGN_ASSERT(IsValid(), "Cannot get rectangle of invalid or uninitialized button");
//	auto& i{ Handle::Get() };
//	return i.rect_;
// }
//
// Button& Button::SetRect(const Rect& new_rectangle) {
//	Create();
//	auto& i{ Handle::Get() };
//	i.rect_ = new_rectangle;
//	i.RecheckState();
//	return *this;
// }
//
// impl::InternalButtonState Button::GetInternalState() const {
//	PTGN_ASSERT(IsValid(), "Cannot get internal state of invalid or uninitialized button");
//	return Handle::Get().button_state_;
// }
//
// void Button::SetInternalOnActivate(const ButtonCallback& internal_on_activate) {
//	Create();
//	Handle::Get().internal_on_activate_ = internal_on_activate;
// }
//
// ButtonState Button::GetState() const {
//	PTGN_ASSERT(IsValid(), "Cannot get state of invalid or uninitialized button");
//	auto& i{ Handle::Get() };
//	if (i.button_state_ == impl::InternalButtonState::Hover) {
//		return ButtonState::Hover;
//	} else if (i.button_state_ == impl::InternalButtonState::Pressed ||
//			   i.button_state_ == impl::InternalButtonState::HeldOutside) {
//		return ButtonState::Pressed;
//	} else {
//		return ButtonState::Default;
//	}
// }
//
// void ToggleButtonGroup::Draw() {
//	ForEachValue([](Button& button) { button.Draw(); });
// }
//
// } // namespace ptgn

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
	Add<Interactive>();
	Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	Add<callback::MouseEnter>([*this](auto mouse) mutable {
		const auto& state{ Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::IdleUp) {
			StateChange(impl::InternalButtonState::Hover);
			StartHover();
		} else if (state == impl::InternalButtonState::IdleDown) {
			StateChange(impl::InternalButtonState::HoverPressed);
			StartHover();
		} else if (state == impl::InternalButtonState::HeldOutside) {
			StateChange(impl::InternalButtonState::Pressed);
		}
	});

	Add<callback::MouseLeave>([*this](auto mouse) mutable {
		const auto& state{ Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::Hover) {
			StateChange(impl::InternalButtonState::IdleUp);
			StopHover();
		} else if (state == impl::InternalButtonState::Pressed) {
			StateChange(impl::InternalButtonState::HeldOutside);
			StopHover();
		} else if (state == impl::InternalButtonState::HoverPressed) {
			StateChange(impl::InternalButtonState::IdleDown);
			StopHover();
		}
	});

	Add<callback::MouseDown>([*this](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Hover) {
				StateChange(impl::InternalButtonState::Pressed);
			}
		}
	});

	Add<callback::MouseDownOutside>([*this](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleUp) {
				StateChange(impl::InternalButtonState::IdleDown);
			}
		}
	});

	Add<callback::MouseUp>([*this](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Pressed) {
				StateChange(impl::InternalButtonState::Hover);
				Activate();
			} else if (state == impl::InternalButtonState::HoverPressed) {
				StateChange(impl::InternalButtonState::Hover);
			}
		}
	});

	Add<callback::MouseUpOutside>([*this](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleDown) {
				StateChange(impl::InternalButtonState::IdleUp);
			} else if (state == impl::InternalButtonState::HeldOutside) {
				StateChange(impl::InternalButtonState::IdleUp);
			}
		}
	});
}

void Button::StateChange(impl::InternalButtonState new_state) {
	Get<impl::InternalButtonState>() = new_state;
	auto state{ GetState() };
	if (Has<impl::ButtonColor>()) {
		Get<impl::ButtonColor>().SetToState(state);
	}
}

void Button::Activate() {
	if (Has<impl::ButtonActivate>()) {
		if (const auto& callback{ Get<impl::ButtonActivate>() }; callback != nullptr) {
			callback();
		}
	}
}

void Button::StartHover() {
	if (Has<impl::ButtonHoverStart>()) {
		if (const auto& callback{ Get<impl::ButtonHoverStart>() }; callback != nullptr) {
			callback();
		}
	}
}

void Button::StopHover() {
	if (Has<impl::ButtonHoverStop>()) {
		if (const auto& callback{ Get<impl::ButtonHoverStop>() }; callback != nullptr) {
			callback();
		}
	}
}

ButtonState Button::GetState() const {
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

std::string Button::GetTextContent(ButtonState state) const {
	// TODO: Fix.
	return std::string();
}

Button& Button::SetTextContent(const std::string& content, ButtonState state) {
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
