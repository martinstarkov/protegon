#include "protegon/button.h"

#include <cassert>    // assert
#include <functional> // std::bind

#include "protegon/collision.h"
#include "core/game.h"
#include "protegon/log.h"

namespace ptgn {

Button::Button(const Rectangle<int>& rect,
			   Texture default_texture,
			   Texture hover_texture,
			   Texture pressed_texture,
			   std::function<void()> on_activate_function) :
	rect_{ rect },
	textures_{ std::array<Texture, 3>{ default_texture, hover_texture, pressed_texture } },
	on_activate_{ on_activate_function } {
	SubscribeToMouseEvents();
}

Button::Button(const Rectangle<int>& rect,
			   const std::size_t default_texture_key,
			   const std::size_t hover_texture_key,
			   const std::size_t pressed_texture_key,
			   std::function<void()> on_activate_function) :
	rect_{ rect },
	textures_{ std::array<std::size_t, 3>{ default_texture_key, hover_texture_key, pressed_texture_key } },
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
	if (state_ == InternalButtonState::IDLE_UP) {
		state_ = InternalButtonState::HOVER;
	}
}

void Button::OnMouseMoveOutside(const MouseMoveEvent& e) {

}

void Button::OnMouseEnter(const MouseMoveEvent& e) {
	if (state_ == InternalButtonState::IDLE_UP) {
		state_ = InternalButtonState::HOVER;
	} else if (state_ == InternalButtonState::IDLE_DOWN) {
		state_ = InternalButtonState::HOVER_PRESSED;
	} else if (state_ == InternalButtonState::HELD_OUTSIDE) {
		state_ = InternalButtonState::PRESSED;
	}
}

void Button::OnMouseLeave(const MouseMoveEvent& e) {
	if (state_ == InternalButtonState::HOVER) {
		state_ = InternalButtonState::IDLE_UP;
	} else if (state_ == InternalButtonState::PRESSED) {
		state_ = InternalButtonState::HELD_OUTSIDE;
	} else if (state_ == InternalButtonState::HOVER_PRESSED) {
		state_ = InternalButtonState::IDLE_DOWN;
	}
}

void Button::OnMouseDown(const MouseDownEvent& e) {
	if (e.mouse == Mouse::LEFT)
		if (state_ == InternalButtonState::HOVER) {
			state_ = InternalButtonState::PRESSED;
		}
}

void Button::OnMouseDownOutside(const MouseDownEvent& e) {
	if (e.mouse == Mouse::LEFT)
		if (state_ == InternalButtonState::IDLE_UP) {
			state_ = InternalButtonState::IDLE_DOWN;
		}
}

void Button::OnMouseUp(const MouseUpEvent& e) {
    if (e.mouse == Mouse::LEFT) {
		if (state_ == InternalButtonState::PRESSED) {
			state_ = InternalButtonState::HOVER;
            if (on_activate_ != nullptr)
				Activate();
        } else if (state_ == InternalButtonState::HOVER_PRESSED) {
			state_ = InternalButtonState::HOVER;
		}
    }
}

void Button::OnMouseUpOutside(const MouseUpEvent& e) {
	if (e.mouse == Mouse::LEFT) {
		if (state_ == InternalButtonState::IDLE_DOWN) {
			state_ = InternalButtonState::IDLE_UP;
		} else if (state_ == InternalButtonState::HELD_OUTSIDE) {
			state_ = InternalButtonState::IDLE_UP;
		}
	}
}

void Button::Draw() {
	if (std::holds_alternative<std::array<Texture, 3>>(textures_)) {
		auto& ts{ std::get<std::array<Texture, 3>>(textures_) };
		auto& state_texture = ts.at(static_cast<std::size_t>(GetState()));
		if (state_texture.IsValid()) {
			state_texture.Draw(rect_);
		} else {
			auto& default_texture = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT));
			assert(default_texture.IsValid() && "Default button state texture must be valid");
			default_texture.Draw(rect_);
		}
	} else if (std::holds_alternative<std::array<std::size_t, 3>>(textures_)) {
		auto& ts{ std::get<std::array<std::size_t, 3>>(textures_) };
		auto& state_texture_key = ts.at(static_cast<std::size_t>(GetState()));
		if (texture::Has(state_texture_key)) {
			texture::Get(state_texture_key)->Draw(rect_);
		} else {
			auto default_texture_key = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT));
			assert(texture::Has(default_texture_key) && "Default button state texture must be valid");
			texture::Get(default_texture_key)->Draw(rect_);
		}
	}
}

const Rectangle<int>& Button::GetRectangle() const {
	return rect_;
}

void Button::SetRectangle(const Rectangle<int>& new_rectangle) {
	rect_ = new_rectangle;
}

ButtonState Button::GetState() const {
	if (state_ == InternalButtonState::HOVER)
		return ButtonState::HOVER;
	else if (state_ == InternalButtonState::PRESSED)
		return ButtonState::PRESSED;
	else
		return ButtonState::DEFAULT;
}

ToggleButton::ToggleButton(const Rectangle<int>& rect,
						   std::variant<Texture,
										std::pair<Texture, Texture>> default_texture,
						   std::variant<Texture,
										std::pair<Texture, Texture>> hover_texture,
						   std::variant<Texture,
										std::pair<Texture, Texture>> pressed_texture,
						   std::function<void()> on_activate_function) {
	rect_ = rect;
	on_activate_ = on_activate_function;
	SubscribeToMouseEvents();
	if (std::holds_alternative<Texture>(default_texture)) {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::DEFAULT)) = { std::get<Texture>(default_texture),
																std::get<Texture>(default_texture) };
	} else {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::DEFAULT)) = { std::get<std::pair<Texture, Texture>>(default_texture).first,
																std::get<std::pair<Texture, Texture>>(default_texture).second };
	}
	if (std::holds_alternative<Texture>(hover_texture)) {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::HOVER)) = { std::get<Texture>(hover_texture),
															  std::get<Texture>(hover_texture) };
	} else {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::HOVER)) = { std::get<std::pair<Texture, Texture>>(hover_texture).first,
															  std::get<std::pair<Texture, Texture>>(hover_texture).second };
	}
	if (std::holds_alternative<Texture>(pressed_texture)) {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::PRESSED)) = { std::get<Texture>(pressed_texture),
																std::get<Texture>(pressed_texture) };
	} else {
		std::get<std::array<std::array<Texture, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::PRESSED)) = { std::get<std::pair<Texture, Texture>>(pressed_texture).first,
																std::get<std::pair<Texture, Texture>>(pressed_texture).second };
	}
}

ToggleButton::ToggleButton(const Rectangle<int>& rect,
						   std::variant<std::size_t,
										std::pair<std::size_t, std::size_t>> default_texture_key,
						   std::variant<std::size_t,
										std::pair<std::size_t, std::size_t>> hover_texture_key,
						   std::variant<std::size_t,
										std::pair<std::size_t, std::size_t>> pressed_texture_key,
						   std::function<void()> on_activate_function) {
	rect_ = rect;
	on_activate_ = on_activate_function;
	SubscribeToMouseEvents();
	if (std::holds_alternative<std::size_t>(default_texture_key)) {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::DEFAULT)) = { std::get<std::size_t>(default_texture_key), 
			                                                    std::get<std::size_t>(default_texture_key) };
	} else {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::DEFAULT)) = { std::get<std::pair<std::size_t, std::size_t>>(default_texture_key).first,
																std::get<std::pair<std::size_t, std::size_t>>(default_texture_key).second };
	}
	if (std::holds_alternative<std::size_t>(hover_texture_key)) {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::HOVER)) = { std::get<std::size_t>(hover_texture_key),
															  std::get<std::size_t>(hover_texture_key) };
	} else {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::HOVER)) = { std::get<std::pair<std::size_t, std::size_t>>(hover_texture_key).first,
															  std::get<std::pair<std::size_t, std::size_t>>(hover_texture_key).second };
	}
	if (std::holds_alternative<std::size_t>(pressed_texture_key)) {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::PRESSED)) = { std::get<std::size_t>(pressed_texture_key),
																std::get<std::size_t>(pressed_texture_key) };
	} else {
		std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_).at(
			static_cast<std::size_t>(ButtonState::PRESSED)) = { std::get<std::pair<std::size_t, std::size_t>>(pressed_texture_key).first,
																std::get<std::pair<std::size_t, std::size_t>>(pressed_texture_key).second };
	}
}

ToggleButton::~ToggleButton() {
	UnsubscribeFromMouseEvents();
}

void ToggleButton::OnMouseUp(const MouseUpEvent& e) {
	if (e.mouse == Mouse::LEFT) {
		if (state_ == InternalButtonState::PRESSED) {
			state_ = InternalButtonState::HOVER;
			toggled_ = !toggled_;
			if (on_activate_ != nullptr)
				Activate();
		} else if (state_ == InternalButtonState::HOVER_PRESSED) {
			state_ = InternalButtonState::HOVER;
		}
	}
}

bool ToggleButton::GetToggleStatus() const {
	return toggled_;
}

void ToggleButton::SetToggleStatus(bool toggled) {
	toggled_ = toggled;
}

void ToggleButton::Draw() {
	if (std::holds_alternative<std::array<std::array<Texture, 2>, 3>>(textures_)) {
		auto& ts{ std::get<std::array<std::array<Texture, 2>, 3>>(textures_) };
		auto& state_texture = ts.at(static_cast<std::size_t>(GetState()))[static_cast<std::size_t>(toggled_)];
		if (state_texture.IsValid()) {
			state_texture.Draw(rect_);
		} else {
			auto& default_texture = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT))[static_cast<std::size_t>(toggled_)];
			assert(default_texture.IsValid() && "Default button state texture must be valid");
			default_texture.Draw(rect_);
		}
	} else if (std::holds_alternative<std::array<std::array<std::size_t, 2>, 3>>(textures_)) {
		auto& ts{ std::get<std::array<std::array<std::size_t, 2>, 3>>(textures_) };
		auto& state_texture_key = ts.at(static_cast<std::size_t>(GetState()))[static_cast<std::size_t>(toggled_)];
		if (texture::Has(state_texture_key)) {
			texture::Get(state_texture_key)->Draw(rect_);
		} else {
			auto default_texture_key = ts.at(static_cast<std::size_t>(ButtonState::DEFAULT))[static_cast<std::size_t>(toggled_)];
			assert(texture::Has(default_texture_key) && "Default button state texture must be valid");
			texture::Get(default_texture_key)->Draw(rect_);
		}
	}
}

} // namespace ptgn
