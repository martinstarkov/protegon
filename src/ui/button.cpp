#include "protegon/button.h"

#include <cassert>    // assert
#include <functional> // std::bind
#include <utility>

#include "protegon/collision.h"
#include "core/game.h"
#include "protegon/log.h"

namespace ptgn {

Button::Button(const Rectangle<int>& rect,
			   std::function<void()> on_activate_function) :
	rect_{ rect },
	on_activate_{ on_activate_function } {
	SubscribeToMouseEvents();
}

Button::~Button() {
 	UnsubscribeFromMouseEvents();
}

void Button::Activate() {
 	on_activate_();
}

void Button::SetOnActivate(std::function<void()> function) {
 	on_activate_ = function;
}

void Button::OnMouseEvent(const Event<MouseEvent>& event) {
    switch (event.Type()) {
 	    case MouseEvent::MOVE:
 	    {
 		    const MouseMoveEvent& e = static_cast<const MouseMoveEvent&>(event);
 		    bool is_in{ overlap::PointRectangle(e.current, rect_) };
 		    if (is_in)
 			    OnMouseMove(e);
 		    else
 			    OnMouseMoveOutside(e);
 		    bool was_in{ overlap::PointRectangle(e.previous, rect_) };
 		    if (!was_in && is_in)
 			    OnMouseEnter(e);
 		    else if (was_in && !is_in)
 			    OnMouseLeave(e);
 		    break;
 	    }
 	    case MouseEvent::DOWN:
 	    {
 		    const MouseDownEvent& e = static_cast<const MouseDownEvent&>(event);
 		    if (overlap::PointRectangle(e.current, rect_))
 			    OnMouseDown(e);
 		    else
 			    OnMouseDownOutside(e);
 		    break;
 	    }
 	    case MouseEvent::UP:
 	    {
 		    const MouseUpEvent& e = static_cast<const MouseUpEvent&>(event);
 		    if (overlap::PointRectangle(e.current, rect_))
 			    OnMouseUp(e);
 		    else
 			    OnMouseUpOutside(e);
 		    break;
 	    }
 	    default:
 		    break;
    }
}

void Button::SubscribeToMouseEvents() {
    global::GetGame().event.mouse_event.Subscribe((void*)this, std::bind(&Button::OnMouseEvent, this, std::placeholders::_1));
}

void Button::UnsubscribeFromMouseEvents() {
    global::GetGame().event.mouse_event.Unsubscribe((void*)this);
}

void Button::OnMouseMove(const MouseMoveEvent& e) {
    if (button_state_ == InternalButtonState::IDLE_UP)
 	    button_state_ = InternalButtonState::HOVER;
}

void Button::OnMouseMoveOutside(const MouseMoveEvent& e) {

}

void Button::OnMouseEnter(const MouseMoveEvent& e) {
    if (button_state_ == InternalButtonState::IDLE_UP)
 	    button_state_ = InternalButtonState::HOVER;
    else if (button_state_ == InternalButtonState::IDLE_DOWN)
 	    button_state_ = InternalButtonState::HOVER_PRESSED;
    else if (button_state_ == InternalButtonState::HELD_OUTSIDE)
 	    button_state_ = InternalButtonState::PRESSED;
}

void Button::OnMouseLeave(const MouseMoveEvent& e) {
    if (button_state_ == InternalButtonState::HOVER)
 	    button_state_ = InternalButtonState::IDLE_UP;
    else if (button_state_ == InternalButtonState::PRESSED)
 	    button_state_ = InternalButtonState::HELD_OUTSIDE;
    else if (button_state_ == InternalButtonState::HOVER_PRESSED)
 	    button_state_ = InternalButtonState::IDLE_DOWN;
}

void Button::OnMouseDown(const MouseDownEvent& e) {
    if (e.mouse == Mouse::LEFT && button_state_ == InternalButtonState::HOVER)
        button_state_ = InternalButtonState::PRESSED;
}

void Button::OnMouseDownOutside(const MouseDownEvent& e) {
    if (e.mouse == Mouse::LEFT && button_state_ == InternalButtonState::IDLE_UP)
 	    button_state_ = InternalButtonState::IDLE_DOWN;
}

void Button::OnMouseUp(const MouseUpEvent& e) {
    if (e.mouse == Mouse::LEFT) {
        if (button_state_ == InternalButtonState::PRESSED) {
            button_state_ = InternalButtonState::HOVER;
            if (on_activate_ != nullptr) {
                Activate();
            }
            else if (button_state_ == InternalButtonState::HOVER_PRESSED) {
                button_state_ = InternalButtonState::HOVER;
            }
        }
    }
}

void Button::OnMouseUpOutside(const MouseUpEvent& e) {
    if (e.mouse == Mouse::LEFT) {
 	    if (button_state_ == InternalButtonState::IDLE_DOWN)
 		    button_state_ = InternalButtonState::IDLE_UP;
 	    else if (button_state_ == InternalButtonState::HELD_OUTSIDE)
 		    button_state_ = InternalButtonState::IDLE_UP;
    }
}

const Rectangle<int>& Button::GetRectangle() const {
    return rect_;
}

void Button::SetRectangle(const Rectangle<int>& new_rectangle) {
 	rect_ = new_rectangle;
}

ButtonState Button::GetState() const {
    if (button_state_ == InternalButtonState::HOVER)
 	    return ButtonState::HOVER;
    else if (button_state_ == InternalButtonState::PRESSED)
 	    return ButtonState::PRESSED;
    else
 	    return ButtonState::DEFAULT;
}

ToggleButton::ToggleButton(
    const Rectangle<int>& rect,
    std::function<void()> on_activate_function,
 	bool initially_toggled) : Button{ rect, on_activate_function } {
 	toggled_ = initially_toggled;
}

ToggleButton::~ToggleButton() {
 	UnsubscribeFromMouseEvents();
}

void ToggleButton::OnMouseUp(const MouseUpEvent& e) {
 	if (e.mouse == Mouse::LEFT) {
 		if (button_state_ == InternalButtonState::PRESSED) {
 			button_state_ = InternalButtonState::HOVER;
            toggled_ = !toggled_;
 			if (on_activate_ != nullptr)
 				Activate();
 		} else if (button_state_ == InternalButtonState::HOVER_PRESSED) {
 			button_state_ = InternalButtonState::HOVER;
 		}
 	}
}

bool ToggleButton::IsToggled() const {
 	return toggled_;
}

void ToggleButton::Toggle() {
 	Activate();
 	toggled_ = !toggled_;
}

void TexturedButton::Draw() {
 	auto& ts{ textures_.data };
 	auto& state_texture = ts.at(static_cast<std::size_t>(GetState())).at(0);
 	if (state_texture.IsValid()) {
 		state_texture.Draw(rect_);
 	} else {
 		auto& default_texture = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0);
 		assert(default_texture.IsValid() && "Default button state texture must be valid");
 		default_texture.Draw(rect_);
 	}
}

void TexturedToggleButton::Draw() {
    auto& ts{ textures_.data };
    auto& state_texture = ts.at(static_cast<std::size_t>(GetState())).at(static_cast<std::size_t>(toggled_));
    if (state_texture.IsValid()) {
        state_texture.Draw(rect_);
    } else {
        auto& default_texture = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0);
        assert(default_texture.IsValid() && "Default button state texture must be valid");
        default_texture.Draw(rect_);
    }
}

} // namespace ptgn
