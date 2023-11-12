#pragma once

#include <array> // std::array
#include <tuple> // std::tuple
#include <variant> // std::variant

#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/rectangle.h"
#include "protegon/texture.h"

namespace ptgn {

enum class ButtonState : std::size_t {
	DEFAULT = 0,
	HOVER   = 1,
	PRESSED = 2,
};

// Interface class for buttons
class IButton {
public:
	virtual ~IButton() = default;
	virtual void Activate() = 0;
	virtual void SetOnActivate(std::function<void()> function) = 0;
	virtual void OnMouseEvent(const Event<MouseEvent>& event) = 0;
	virtual void SubscribeToMouseEvents() = 0;
	virtual void UnsubscribeFromMouseEvents() = 0;
protected:
	enum class InternalButtonState : std::size_t {
		IDLE_UP = 0,
		HOVER = 1,
		PRESSED = 2,
		HELD_OUTSIDE = 3,
		IDLE_DOWN = 4,
		HOVER_PRESSED = 5
	};
};

class Button : public IButton {
public:
	Button() = default;
	Button(const Rectangle<int>& rect,
		   Texture default_texture,
		   Texture hover_texture,
		   Texture pressed_texture,
		   std::function<void()> on_activate_function = nullptr);
	Button(const Rectangle<int>& rect,
		   const std::size_t default_texture_key,
		   const std::size_t hover_texture_key,
		   const std::size_t pressed_texture_key,
		   std::function<void()> on_activate_function = nullptr);
	virtual ~Button();
	virtual void Activate() override final;
	virtual void SetOnActivate(std::function<void()> function) override final;
	virtual void OnMouseEvent(const Event<MouseEvent>& event) override;
	virtual void SubscribeToMouseEvents() override final;
	virtual void UnsubscribeFromMouseEvents() override final;
	virtual void OnMouseMove(const MouseMoveEvent& e);
	virtual void OnMouseMoveOutside(const MouseMoveEvent& e);
	virtual void OnMouseEnter(const MouseMoveEvent& e);
	virtual void OnMouseLeave(const MouseMoveEvent& e);
	virtual void OnMouseDown(const MouseDownEvent& e);
	virtual void OnMouseDownOutside(const MouseDownEvent& e);
	virtual void OnMouseUp(const MouseUpEvent& e);
	virtual void OnMouseUpOutside(const MouseUpEvent& e);
	virtual void Draw();
	const Rectangle<int>& GetRectangle() const;
	void SetRectangle(const Rectangle<int>& new_rectangle);
	ButtonState GetState() const;
protected:
	std::function<void()> on_activate_{ nullptr };
	Rectangle<int> rect_;
	InternalButtonState state_{ InternalButtonState::IDLE_UP };
private:
	// TODO: Add default array here.
	std::variant<std::array<Texture, 3>,
		         std::array<std::size_t, 3>> textures_;
};

class ToggleButton : public Button {
public:
	ToggleButton() = default;
	virtual ~ToggleButton();
	ToggleButton(const Rectangle<int>& rect,
		         std::variant<Texture, 
				              std::pair<Texture, Texture>> default_texture,
				 std::variant<Texture,
							  std::pair<Texture, Texture>> hover_texture,
				 std::variant<Texture,
							  std::pair<Texture, Texture>> pressed_texture,
		         std::function<void()> on_activate_function = nullptr);
	ToggleButton(const Rectangle<int>& rect,
				 std::variant<std::size_t,
							  std::pair<std::size_t, std::size_t>> default_texture_key,
				 std::variant<std::size_t,
							  std::pair<std::size_t, std::size_t>> hover_texture_key,
				 std::variant<std::size_t,
							  std::pair<std::size_t, std::size_t>> pressed_texture_key,
		         std::function<void()> on_activate_function = nullptr);
	virtual void OnMouseUp(const MouseUpEvent& e) override;
	virtual void Draw() override;
	// Returns false if not toggled, true if toggled.
	bool GetToggleStatus() const;
	void SetToggleStatus(bool toggled);
private:
	bool toggled_{ false }; // false = not toggled, true = toggled
	// TODO: Add default array here.
	std::variant<std::array<std::array<Texture, 2>, 3>,
		         std::array<std::array<std::size_t, 2>, 3>> textures_;
};

} // namespace ptgn