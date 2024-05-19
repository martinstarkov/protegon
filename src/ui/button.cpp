#include "protegon/button.h"

#include <cassert>    // assert
#include <functional> // std::bind
#include <utility>

#include "protegon/collision.h"
#include "protegon/input.h"
#include "core/game.h"
#include "protegon/log.h"

namespace ptgn {

Button::Button(const Rectangle<float>& rect,
			   std::function<void()> on_activate_function) :
	rect_{ rect },
	on_activate_{ on_activate_function } {
	SubscribeToMouseEvents();
    RecheckState();
}

Button::~Button() {
 	UnsubscribeFromMouseEvents();
}

void Button::Activate() {
 	on_activate_();
}

void Button::StartHover() {
    on_hover_start_();
}

void Button::StopHover() {
    on_hover_stop_();
}

void Button::SetOnActivate(std::function<void()> function) {
 	on_activate_ = function;
}

void Button::SetOnHover(std::function<void()> start_hover_function, std::function<void()> stop_hover_function) {
    on_hover_start_ = start_hover_function;
    on_hover_stop_ = stop_hover_function;
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
    if (on_hover_start_ != nullptr) {
        StartHover();
    }
}

void Button::OnMouseLeave(const MouseMoveEvent& e) {
    if (button_state_ == InternalButtonState::HOVER)
 	    button_state_ = InternalButtonState::IDLE_UP;
    else if (button_state_ == InternalButtonState::PRESSED)
 	    button_state_ = InternalButtonState::HELD_OUTSIDE;
    else if (button_state_ == InternalButtonState::HOVER_PRESSED)
 	    button_state_ = InternalButtonState::IDLE_DOWN;
    if (on_hover_stop_ != nullptr) {
        StopHover();
    }
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

const Rectangle<float>& Button::GetRectangle() const {
    return rect_;
}

void Button::RecheckState() {
    OnMouseEvent(MouseMoveEvent{ V2_int{ std::numeric_limits<int>().min(), std::numeric_limits<int>().min() }, input::GetMousePosition() });
}

void Button::SetRectangle(const Rectangle<float>& new_rectangle) {
    RecheckState();
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
    const Rectangle<float>& rect,
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

void SolidButton::DrawImpl(std::size_t color_array_index) const {
    const Color& color = GetCurrentColorImpl(GetState(), color_array_index);
    rect_.DrawSolid(color);
}

void SolidButton::Draw() const {
    DrawImpl(0);
}

const Color& SolidButton::GetCurrentColorImpl(ButtonState state, std::size_t color_array_index) const {
    auto& color_array = colors_.data.at(static_cast<std::size_t>(state));
    const Color& color = color_array.at(color_array_index);
    return color;
}

const Color& SolidButton::GetCurrentColor() const {
    return GetCurrentColorImpl(GetState(), 0);
}

void TexturedButton::DrawImpl(std::size_t texture_array_index) const {
    Texture texture = GetCurrentTextureImpl(GetState(), texture_array_index);
    if (!texture.IsValid()) {
        texture = GetCurrentTextureImpl(ButtonState::DEFAULT, 0);
    }
    assert(texture.IsValid() && "Button state texture (or default texture) must be valid");
    texture.Draw(rect_);
}

void TexturedButton::Draw() const {
    DrawImpl(0);
}

const Texture& TexturedButton::GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index) const {
    auto& texture_array = textures_.data.at(static_cast<std::size_t>(state));

    const std::variant<Texture, TextureKey>& texture_state = texture_array.at(texture_array_index);

    if (std::holds_alternative<TextureKey>(texture_state)) {
        const TextureKey key = std::get<TextureKey>(texture_state);
        assert(texture::Has(key) && "Cannot get button texture which has not been loaded");
        return *texture::Get(key);
    }
    else {
        return std::get<Texture>(texture_state);
    }
}

const Texture& TexturedButton::GetCurrentTexture() const {
    return GetCurrentTextureImpl(GetState(), 0);
}

void TexturedToggleButton::Draw() const {
    DrawImpl(static_cast<std::size_t>(toggled_));
}

const Texture& TexturedToggleButton::GetCurrentTexture() const {
    return GetCurrentTextureImpl(GetState(), static_cast<std::size_t>(toggled_));
}

} // namespace ptgn
