// #pragma once

// #include <array> // std::array
// #include <tuple> // std::tuple
// #include <variant> // std::variant

// #include "protegon/event.h"
// #include "protegon/events.h"
// #include "protegon/rectangle.h"
// #include "protegon/texture.h"

// namespace ptgn {

// // First element  = texture for 'not toggled' state of button
// // Second element = texture for 'toggled' state of button.
// using TexturePair = std::pair<Texture, Texture>;
// // First element  = texture key for 'not toggled' state of button
// // Second element = texture key for 'toggled' state of button.
// using TextureKeyPair = std::pair<TextureKey, TextureKey>;

// enum class ButtonState : std::size_t {
// 	DEFAULT = 0,
// 	HOVER   = 1,
// 	PRESSED = 2
// };

// // Interface class for buttons
// class IButton {
// public:
// 	virtual ~IButton() = default;
// 	virtual void Activate() = 0;
// 	virtual void SetOnActivate(std::function<void()> function) = 0;
// 	virtual void OnMouseEvent(const Event<MouseEvent>& event) = 0;
// 	virtual void SubscribeToMouseEvents() = 0;
// 	virtual void UnsubscribeFromMouseEvents() = 0;
// protected:
// 	enum class InternalButtonState : std::size_t {
// 		IDLE_UP = 0,
// 		HOVER = 1,
// 		PRESSED = 2,
// 		HELD_OUTSIDE = 3,
// 		IDLE_DOWN = 4,
// 		HOVER_PRESSED = 5
// 	};
// };

// class Button : public IButton {
// public:
// 	Button() = default;
// 	Button(const Rectangle<int>& rect,
// 		   std::function<void()> on_activate_function = nullptr);
// 	virtual ~Button();
// 	virtual void Activate() override final;
// 	virtual void SetOnActivate(std::function<void()> function) override final;
// 	virtual void OnMouseEvent(const Event<MouseEvent>& event) override;
// 	virtual void SubscribeToMouseEvents() override final;
// 	virtual void UnsubscribeFromMouseEvents() override final;
// 	virtual void OnMouseMove(const MouseMoveEvent& e);
// 	virtual void OnMouseMoveOutside(const MouseMoveEvent& e);
// 	virtual void OnMouseEnter(const MouseMoveEvent& e);
// 	virtual void OnMouseLeave(const MouseMoveEvent& e);
// 	virtual void OnMouseDown(const MouseDownEvent& e);
// 	virtual void OnMouseDownOutside(const MouseDownEvent& e);
// 	virtual void OnMouseUp(const MouseUpEvent& e);
// 	virtual void OnMouseUpOutside(const MouseUpEvent& e);
// 	const Rectangle<int>& GetRectangle() const;
// 	void SetRectangle(const Rectangle<int>& new_rectangle);
// 	ButtonState GetState() const;
// protected:
// 	Rectangle<int> rect_{};
// 	std::function<void()> on_activate_{ nullptr };
// 	InternalButtonState button_state_{ InternalButtonState::IDLE_UP };
// };

// class ToggleButton : public Button {
// public:
// 	using Button::Button;
// 	ToggleButton(const Rectangle<int>& rect,
// 				 std::function<void()> on_activate_function = nullptr,
// 				 bool initially_toggled = false);
// 	// Start in non toggled state.
// 	virtual void OnMouseUp(const MouseUpEvent& e) override;
// 	bool IsToggled() const;
// 	void Toggle();
// private:
// 	bool toggled_{ false };
// };

// class TexturedButton : public Button {
// public:
// 	TexturedButton() = default;
// 	template <typename T, type_traits::type<T, Texture, TextureKey> = true>
// 	TexturedButton(
// 		const Rectangle<int>& rect,
// 		T default,
// 		T hover,
// 		T pressed,
// 		std::function<void()> on_activate_function = nullptr) : Button{ rect, on_activate_function } {
// 		textures_ = std::array<T, 3>{ default, hover, pressed };
// 	}
// 	virtual void Draw();
// private:
// 	// Must be initialized explicitly by a constructor.
// 	// Can technically exist uninitialized if button is default constructed (temporary object).
// 	std::variant<std::array<Texture, 3>,
// 		         std::array<TextureKey, 3>> textures_;
// };

// class TexturedToggleButton : public ToggleButton {
// public:
// 	TexturedToggleButton() = default;
// 	TexturedToggleButton(const Rectangle<int>& rect,
// 						 std::variant<Texture, TexturePair> default,
// 						 std::variant<Texture, TexturePair> hover,
// 						 std::variant<Texture, TexturePair> pressed,
// 						 std::function<void()> on_activate_function = nullptr);
// 	TexturedToggleButton(const Rectangle<int>& rect,
// 						 std::variant<TextureKey, TextureKeyPair> default,
// 						 std::variant<TextureKey, TextureKeyPair> hover,
// 						 std::variant<TextureKey, TextureKeyPair> pressed,
// 						 std::function<void()> on_activate_function = nullptr);
// 	virtual void Draw();
// private:
// 	// Must be initialized explicitly by a constructor.
// 	// Can technically exist uninitialized if button is default constructed (temporary object).
// 	std::variant<std::array<TexturePair, 3>,
// 		         std::array<TextureKeyPair, 3>> textures_;
// };

// } // namespace ptgn