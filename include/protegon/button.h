#pragma once

#include <unordered_map> // std::unordered_map

#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/rectangle.h"
#include "protegon/texture.h"

namespace ptgn {

// TODO: Add disabled state.
enum class ButtonState : std::size_t {
	IDLE_UP       = 0,
	HOVER         = 1,
	PRESSED       = 2,
	HELD_OUTSIDE  = 3,
	IDLE_DOWN     = 4,
	HOVER_PRESSED = 5,
	FOCUSED       = 6
};

// Interface class for buttons
class IButton {
public:
	virtual ~IButton() = default;
	virtual void SetOnActivate(std::function<void()> function) = 0;
	virtual void OnMouseEvent(const Event<MouseEvent>& event) = 0;
	virtual void ResetState() = 0;
	virtual void SubscribeToMouseEvents() = 0;
	virtual void UnsubscribeFromMouseEvents() = 0;
};

class Button : public IButton {
public:
	Button(const Rectangle<int>& rect,
		   const std::unordered_map<ButtonState, Texture>& textures,
		   std::function<void()> on_activate_function = nullptr);
	virtual ~Button();
	virtual void SetOnActivate(std::function<void()> function) override final;
	virtual void OnMouseEvent(const Event<MouseEvent>& event) override;
	virtual void ResetState() override;
	virtual void SubscribeToMouseEvents() override final;
	virtual void UnsubscribeFromMouseEvents() override final;
	void OnMouseMove(const MouseMoveEvent& e);
	void OnMouseMoveOutside(const MouseMoveEvent& e);
	void OnMouseEnter(const MouseMoveEvent& e);
	void OnMouseLeave(const MouseMoveEvent& e);
	void OnMouseDown(const MouseDownEvent& e);
	void OnMouseDownOutside(const MouseDownEvent& e);
	void OnMouseUp(const MouseUpEvent& e);
	void OnMouseUpOutside(const MouseUpEvent& e);
	void Draw();
	const Rectangle<int>& GetRectangle() const;
	void SetRectangle(const Rectangle<int>& new_rectangle);
	ButtonState GetState() const;
	void SetState(ButtonState new_state);
private:
	std::function<void()> on_activate{ nullptr };
	std::unordered_map<ButtonState, Texture> textures;
	Rectangle<int> rect;
	ButtonState state{ ButtonState::IDLE_UP };
};

} // namespace ptgn