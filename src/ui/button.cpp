#include "protegon/button.h"

#include <cassert>    // assert
#include <functional> // std::bind

#include "protegon/collision.h"
#include "core/game.h"

namespace ptgn {

Button::Button(const Rectangle<int>& rect, 
			   const std::unordered_map<ButtonState, Texture>& textures, 
			   std::function<void()> on_activate_function) :
	rect{ rect },
	textures{ textures },
	on_activate{ on_activate_function } {
	SubscribeToMouseEvents();
}

Button::~Button() {
	UnsubscribeFromMouseEvents();
}

void Button::SetOnActivate(std::function<void()> function) {
	on_activate = function;
}

void Button::OnMouseEvent(const Event<MouseEvent>& event) {
	switch (event.Type()) {
		case MouseEvent::MOVE:
		{
			const MouseMoveEvent& e = static_cast<const MouseMoveEvent&>(event);
			bool is_in{ overlap::PointRectangle(e.current, rect) };
			if (is_in)
				OnMouseMove(e);
			else
				OnMouseMoveOutside(e);
			bool was_in{ overlap::PointRectangle(e.previous, rect) };
			if (!was_in && is_in)
				OnMouseEnter(e);
			else if (was_in && !is_in)
				OnMouseLeave(e);
			break;
		}
		case MouseEvent::DOWN:
		{
			const MouseDownEvent& e = static_cast<const MouseDownEvent&>(event);
			if (overlap::PointRectangle(e.current, rect))
				OnMouseDown(e);
			else
				OnMouseDownOutside(e);
			break;
		}
		case MouseEvent::UP:
		{
			const MouseUpEvent& e = static_cast<const MouseUpEvent&>(event);
			if (overlap::PointRectangle(e.current, rect))
				OnMouseUp(e);
			else
				OnMouseUpOutside(e);
			break;
		}
		default:
			break;
	}
}

void Button::ResetState() {
	if (state == ButtonState::FOCUSED)
		state = ButtonState::HOVER;
}

void Button::SubscribeToMouseEvents() {
	global::GetGame().event.mouse_event.Subscribe((void*)this, std::bind(&Button::OnMouseEvent, this, std::placeholders::_1));
}

void Button::UnsubscribeFromMouseEvents() {
	global::GetGame().event.mouse_event.Unsubscribe((void*)this);
}

void Button::OnMouseMove(const MouseMoveEvent& e) {
	if (state == ButtonState::IDLE_UP) {
		state = ButtonState::HOVER;
	}
}

void Button::OnMouseMoveOutside(const MouseMoveEvent& e) {

}

void Button::OnMouseEnter(const MouseMoveEvent& e) {
	if (state == ButtonState::IDLE_UP) {
		state = ButtonState::HOVER;
	} else if (state == ButtonState::IDLE_DOWN) {
		state = ButtonState::HOVER_PRESSED;
	} else if (state == ButtonState::HELD_OUTSIDE) {
		state = ButtonState::PRESSED;
	}
}

void Button::OnMouseLeave(const MouseMoveEvent& e) {
	if (state == ButtonState::HOVER) {
		state = ButtonState::IDLE_UP;
	} else if (state == ButtonState::PRESSED) {
		state = ButtonState::HELD_OUTSIDE;
	} else if (state == ButtonState::HOVER_PRESSED) {
		state = ButtonState::IDLE_DOWN;
	}
}

void Button::OnMouseDown(const MouseDownEvent& e) {
	if (e.mouse == Mouse::LEFT)
		if (state == ButtonState::HOVER) {
			state = ButtonState::PRESSED;
		}
}

void Button::OnMouseDownOutside(const MouseDownEvent& e) {
	if (e.mouse == Mouse::LEFT)
		if (state == ButtonState::IDLE_UP) {
			state = ButtonState::IDLE_DOWN;
		}
}

void Button::OnMouseUp(const MouseUpEvent& e) {
    if (e.mouse == Mouse::LEFT) {
		if (state == ButtonState::PRESSED) {
			state = ButtonState::FOCUSED;
            if (on_activate != nullptr)
				on_activate();
			ResetState();
        } else if (state == ButtonState::HOVER_PRESSED) {
			state = ButtonState::HOVER;
		} else if (state == ButtonState::FOCUSED) {
			state = ButtonState::HOVER;
		}
    }
}

void Button::OnMouseUpOutside(const MouseUpEvent& e) {
	if (e.mouse == Mouse::LEFT) {
		if (state == ButtonState::IDLE_DOWN) {
			state = ButtonState::IDLE_UP;
		} else if (state == ButtonState::HELD_OUTSIDE) {
			state = ButtonState::IDLE_UP;
		}
		/*else if (state == State::FOCUSED) { // TODO: Set button to idle if user left clicks outside it.
			state = State::IDLE_UP;
		}*/
	}
}

void Button::Draw() {
	auto it = textures.find(state);
	if (it != textures.end()) {
		auto& texture = it->second;
		if (texture.IsValid())
			texture.Draw(rect);
	} else {
		auto idle_it = textures.find(ButtonState::IDLE_UP);
		assert(idle_it != textures.end() && "Idle up state button texture must be added");
		assert(idle_it->second.IsValid() && "Idle up state button texture must be valid");
		idle_it->second.Draw(rect);
	}
}

const Rectangle<int>& Button::GetRectangle() const {
	return rect;
}

void Button::SetRectangle(const Rectangle<int>& new_rectangle) {
	rect = new_rectangle;
}

ButtonState Button::GetState() const {
	return state;
}

void Button::SetState(ButtonState new_state) {
	state = new_state;
}

} // namespace ptgn
