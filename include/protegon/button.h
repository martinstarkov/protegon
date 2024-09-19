#pragma once

#include <array>
#include <functional>
#include <variant>
#include <vector>

#include "protegon/color.h"
#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/polygon.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"

namespace ptgn {

enum class ButtonState : std::size_t {
	Default = 0,
	Hover	= 1,
	Pressed = 2
};

template <size_t I>
struct TextureArray {
	std::array<std::array<TextureOrKey, I>, 3> data;
};

template <size_t I>
struct ColorArray {
	std::array<std::array<Color, I>, 3> data;
};

using ButtonActivateFunction = std::function<void()>;
using ButtonHoverFunction	 = std::function<void()>;
using ButtonEnableFunction	 = std::function<void()>;
using ButtonDisableFunction	 = std::function<void()>;

class Button {
public:
	Button() = default;
	Button(
		const Rectangle<float>& rect, const ButtonActivateFunction& on_activate_function = nullptr
	);

	virtual ~Button();

	// Draws a black hollow rectangle around the button rectangle.
	// Use ColorButton for more advanced colored buttons or TexturedButton for textured buttons.
	virtual void Draw() const;

	virtual void DrawHollow(float line_width = 1.0f) const;
	virtual void DrawFilled() const;

	virtual void RecheckState() final;

	// These functions cause button to stop responding to events.
	[[nodiscard]] virtual bool GetInteractable() const final;
	virtual void SetInteractable(bool interactable);

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	// Ensure to subscribe to mouse events for this function to be called.
	virtual void SetOnActivate(const ButtonActivateFunction& function);
	// Ensure to subscribe to mouse events for this function to be called.
	virtual void SetOnHover(
		const ButtonHoverFunction& start_hover_function,
		const ButtonHoverFunction& stop_hover_function = nullptr
	);

	virtual void SetOnEnable(const ButtonEnableFunction& enable_function);
	virtual void SetOnDisable(const ButtonDisableFunction& disable_function);

	[[nodiscard]] virtual bool IsSubscribedToMouseEvents() const final;

	// Copying a button will not preserve this.
	virtual void SubscribeToMouseEvents() final;
	virtual void UnsubscribeFromMouseEvents() final;

	void OnMouseEvent(MouseEvent type, const Event& event);
	virtual void OnMouseMove(const MouseMoveEvent& e);
	virtual void OnMouseMoveOutside(const MouseMoveEvent& e);
	virtual void OnMouseEnter(const MouseMoveEvent& e);
	virtual void OnMouseLeave(const MouseMoveEvent& e);
	virtual void OnMouseDown(const MouseDownEvent& e);
	virtual void OnMouseDownOutside(const MouseDownEvent& e);
	virtual void OnMouseUp(const MouseUpEvent& e);
	virtual void OnMouseUpOutside(const MouseUpEvent& e);

	[[nodiscard]] const Rectangle<float>& GetRectangle() const;
	void SetRectangle(const Rectangle<float>& new_rectangle);

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] bool InsideRectangle(const V2_int& position) const;

protected:
	enum class InternalButtonState : std::size_t {
		IdleUp		 = 0,
		Hover		 = 1,
		Pressed		 = 2,
		HeldOutside	 = 3,
		IdleDown	 = 4,
		HoverPressed = 5
	};

	Rectangle<float> rect_;
	ButtonActivateFunction on_activate_;
	ButtonHoverFunction on_hover_start_;
	ButtonHoverFunction on_hover_stop_;
	ButtonEnableFunction on_enable_;
	ButtonDisableFunction on_disable_;
	InternalButtonState button_state_{ InternalButtonState::IdleUp };
	bool enabled_{ true };
};

class ColorButton : public virtual Button {
public:
	ColorButton() = default;
	ColorButton(
		const Rectangle<float>& rect, const Color& default_color, const Color& hover_color,
		const Color& pressed_color, const ButtonActivateFunction& on_active_function = nullptr
	);

	void SetColor(const Color& default_color);
	void SetHoverColor(const Color& hover_color);
	void SetPressedColor(const Color& pressed_color);

	const Color& GetColor() const;
	const Color& GetHoverColor() const;
	const Color& GetPressedColor() const;

	// Draws a filled button.
	void Draw() const override;

	void DrawHollow(float line_width = 1.0f) const override;
	void DrawFilled() const override;

	[[nodiscard]] virtual const Color& GetCurrentColor() const;

protected:
	[[nodiscard]] const Color& GetCurrentColorImpl(
		ButtonState state, std::size_t color_array_index = 0
	) const;

protected:
	ColorArray<2> colors_{};
};

using TextAlignment = Origin;

class TextButton : public virtual ColorButton {
public:
	void SetBorder(bool draw_border);
	[[nodiscard]] bool HasBorder() const;

	// If either axis of the text size is zero, it is stretched to fit the entire size of the button
	// rectangle (along that axis).
	void SetTextSize(const V2_float& text_size = {});
	[[nodiscard]] V2_float GetTextSize() const;

	void SetText(const Text& text);
	[[nodiscard]] const Text& GetText() const;

	void SetTextAlignment(const TextAlignment& text_alignment);
	[[nodiscard]] const TextAlignment& GetTextAlignment() const;

	// Draws a filled button.
	void Draw() const override;

	void DrawHollow(float line_width = 1.0f) const override;
	void DrawFilled() const override;

protected:
	V2_float text_size_;
	bool draw_border_{ true };
	Text text_;
	TextAlignment text_alignment_{ TextAlignment::Center };
};

class ToggleButton : public virtual Button {
public:
	using Button::Button;

	ToggleButton(
		const Rectangle<float>& rect, const ButtonActivateFunction& on_active_function = nullptr,
		bool initially_toggled = false
	);
	~ToggleButton() override;

	// Start in non toggled state.
	void OnMouseUp(const MouseUpEvent& e) override;

	[[nodiscard]] bool IsToggled() const;
	void Toggle();
	void SetToggleState(bool toggled);

protected:
	bool toggled_{ false };
};

class TexturedButton : public virtual Button {
public:
	TexturedButton() = default;
	TexturedButton(
		const Rectangle<float>& rect, const TextureOrKey& default_texture,
		const TextureOrKey& hover_texture, const TextureOrKey& pressed_texture,
		const ButtonActivateFunction& on_active_function = nullptr
	);

	[[nodiscard]] virtual bool GetVisibility() const final;
	virtual void SetVisibility(bool visibility);

	void Draw() const override;

	[[nodiscard]] virtual Texture GetCurrentTexture();

	void ForEachTexture(const std::function<void(Texture)>& func) const;

	void SetTintColor(const Color& color);
	[[nodiscard]] Color GetTintColor() const;

protected:
	// This function does not check for the validity of the returned texture.
	[[nodiscard]] Texture GetCurrentTextureImpl(
		ButtonState state, std::size_t texture_array_index = 0
	) const;

	void DrawImpl(std::size_t texture_array_index = 0) const;

protected:
	// Must be initialized explicitly by a constructor.
	// Can technically exist uninitialized if button is default constructed
	// (temporary object).
	// TODO: Figure out a way to store 1 here and 2 in the toggle button class
	TextureArray<2> textures_;
	Color tint_color_{ color::White };
	bool hidden_{ false };
};

class TexturedToggleButton : public virtual ToggleButton, public virtual TexturedButton {
public:
	TexturedToggleButton() = default;
	TexturedToggleButton(
		const Rectangle<float>& rect, const std::vector<TextureOrKey>& default_textures,
		const std::vector<TextureOrKey>& hover_textures	  = {},
		const std::vector<TextureOrKey>& pressed_textures = {},
		const ButtonActivateFunction& on_active_function  = nullptr
	);

	void OnMouseUp(const MouseUpEvent& e) override;

	[[nodiscard]] Texture GetCurrentTexture() override;
	void Draw() const override;
};

} // namespace ptgn