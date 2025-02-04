// #include "ui/button.h"
//
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
// template <typename T>
//[[nodiscard]] static T GetFinalResource(
//	impl::ButtonResourceState current_state, impl::ButtonResourceState default_state,
//	const std::unordered_map<impl::ButtonResourceState, T>& map, const T& fallback = {}
//) {
//	if (auto it{ map.find(current_state) }; it != map.end() && it->second != T{}) {
//		return it->second;
//	}
//	if (auto it{ map.find(default_state) }; it != map.end() && it->second != T{}) {
//		return it->second;
//	}
//	return fallback;
// }
//
// namespace impl {
//
// ButtonResourceState GetButtonResourceState(ButtonState button_state, bool toggled, bool disabled)
// { 	std::uint8_t combined{ 0 };
//
//	// Set the bits for ButtonState
//	combined |= static_cast<std::uint8_t>(button_state);
//	if (disabled) {
//		// ButtonState is irrelevant for disabled buttons.
//		combined  = 0;
//		combined |= static_cast<std::uint8_t>(disabled) << 3;
//	}
//	// Set after disabled bit because ToggledDisabled exists.
//	combined |= static_cast<std::uint8_t>(toggled) << 2;
//
//	return static_cast<ButtonResourceState>(combined);
// }
//
// ButtonInstance::ButtonInstance() {
//	game.event.mouse.Subscribe(this, [this](MouseEvent t, const Event& e) { OnMouseEvent(t, e); });
//	render_target_ = game.renderer.GetRenderTarget();
// }
//
// ButtonInstance::~ButtonInstance() {
//	game.event.mouse.Unsubscribe(this);
// }
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
// void ButtonInstance::OnMouseMove([[maybe_unused]] const MouseMoveEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (button_state_ == impl::InternalButtonState::IdleUp) {
//		button_state_ = impl::InternalButtonState::Hover;
//		if (on_hover_start_ != nullptr) {
//			StartHover();
//		}
//	}
// }
//
// void ButtonInstance::OnMouseMoveOutside([[maybe_unused]] const MouseMoveEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (button_state_ == impl::InternalButtonState::Hover) {
//		button_state_ = impl::InternalButtonState::IdleUp;
//		if (on_hover_stop_ != nullptr) {
//			StopHover();
//		}
//	}
// }
//
// void ButtonInstance::OnMouseEnter([[maybe_unused]] const MouseMoveEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (button_state_ == impl::InternalButtonState::IdleUp) {
//		button_state_ = impl::InternalButtonState::Hover;
//	} else if (button_state_ == impl::InternalButtonState::IdleDown) {
//		button_state_ = impl::InternalButtonState::HoverPressed;
//	} else if (button_state_ == impl::InternalButtonState::HeldOutside) {
//		button_state_ = impl::InternalButtonState::Pressed;
//	}
//	if (on_hover_start_ != nullptr) {
//		StartHover();
//	}
// }
//
// void ButtonInstance::OnMouseLeave([[maybe_unused]] const MouseMoveEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (button_state_ == impl::InternalButtonState::Hover) {
//		button_state_ = impl::InternalButtonState::IdleUp;
//	} else if (button_state_ == impl::InternalButtonState::Pressed) {
//		button_state_ = impl::InternalButtonState::HeldOutside;
//	} else if (button_state_ == impl::InternalButtonState::HoverPressed) {
//		button_state_ = impl::InternalButtonState::IdleDown;
//	}
//	if (on_hover_stop_ != nullptr) {
//		StopHover();
//	}
// }
//
// void ButtonInstance::OnMouseDown(const MouseDownEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (e.mouse == Mouse::Left && button_state_ == impl::InternalButtonState::Hover) {
//		button_state_ = impl::InternalButtonState::Pressed;
//	}
// }
//
// void ButtonInstance::OnMouseDownOutside(const MouseDownEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (e.mouse == Mouse::Left && button_state_ == impl::InternalButtonState::IdleUp) {
//		button_state_ = impl::InternalButtonState::IdleDown;
//	}
// }
//
// void ButtonInstance::OnMouseUp(const MouseUpEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (e.mouse == Mouse::Left) {
//		if (button_state_ == impl::InternalButtonState::Pressed) {
//			button_state_ = impl::InternalButtonState::Hover;
//			if (toggleable_) {
//				Toggle();
//			} else {
//				Activate();
//			}
//		} else if (button_state_ == impl::InternalButtonState::HoverPressed) {
//			button_state_ = impl::InternalButtonState::Hover;
//		}
//	}
//	RecheckState();
// }
//
// void ButtonInstance::OnMouseUpOutside(const MouseUpEvent& e) {
//	if (!enabled_) {
//		return;
//	}
//	if (e.mouse == Mouse::Left) {
//		if (button_state_ == impl::InternalButtonState::IdleDown) {
//			button_state_ = impl::InternalButtonState::IdleUp;
//		} else if (button_state_ == impl::InternalButtonState::HeldOutside) {
//			button_state_ = impl::InternalButtonState::IdleUp;
//		}
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
//	} else if (i.button_state_ == impl::InternalButtonState::Pressed || i.button_state_ ==
//impl::InternalButtonState::HeldOutside) { 		return ButtonState::Pressed; 	} else { 		return
//ButtonState::Default;
//	}
// }
//
// void ToggleButtonGroup::Draw() {
//	ForEachValue([](Button& button) { button.Draw(); });
// }
//
// } // namespace ptgn
