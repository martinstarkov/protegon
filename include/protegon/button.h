#pragma once

#include <array>
#include <tuple>
#include <variant>

#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/game.h"
#include "utility/type_traits.h"

namespace ptgn {

using TextureOrKey = std::variant<Texture, TextureKey>;

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

class Button {
public:
	Button() = default;
	Button(const Rectangle<float>& rect, std::function<void()> on_activate_function = nullptr);

	virtual void RecheckState() final;

	virtual ~Button();

	// These functions cause button to stop responding to events.
	[[nodiscard]] virtual bool GetInteractable() const final;
	virtual void SetInteractable(bool interactable);

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	virtual void SetOnActivate(std::function<void()> function);
	virtual void SetOnHover(
		std::function<void()> start_hover_function = nullptr,
		std::function<void()> stop_hover_function  = nullptr
	);

	[[nodiscard]] virtual bool IsSubscribedToMouseEvents() const final;
	virtual void SubscribeToMouseEvents() final;
	virtual void UnsubscribeFromMouseEvents() final;

	virtual void OnMouseEvent(MouseEvent type, const Event& event);
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
		HeldOutside  = 3,
		IdleDown	 = 4,
		HoverPressed = 5
	};

	Rectangle<float> rect_;
	std::function<void()> on_activate_;
	std::function<void()> on_hover_start_;
	std::function<void()> on_hover_stop_;
	InternalButtonState button_state_{ InternalButtonState::IdleUp };
	bool enabled_{ true };
};

class SolidButton : public virtual Button {
public:
	SolidButton() = default;
	SolidButton(
		const Rectangle<float>& rect, Color default_color, Color hover_color, Color pressed_color,
		std::function<void()> on_activate_function = nullptr
	);

	virtual void Draw() const;

	[[nodiscard]] virtual const Color& GetCurrentColor() const;

protected:
	[[nodiscard]] const Color& GetCurrentColorImpl(
		ButtonState state, std::size_t color_array_index = 0
	) const;

	void DrawImpl(std::size_t color_array_index = 0) const;

protected:
	ColorArray<2> colors_{};
};

class ToggleButton : public virtual Button {
public:
	using Button::Button;

	ToggleButton(
		const Rectangle<float>& rect, std::function<void()> on_activate_function = nullptr,
		bool initially_toggled = false
	);
	~ToggleButton();

	// Start in non toggled state.
	virtual void OnMouseUp(const MouseUpEvent& e) override;

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
		std::function<void()> on_activate_function = nullptr
	);

	[[nodiscard]] virtual bool GetVisibility() const final;
	virtual void SetVisibility(bool visibility);

	virtual void Draw() const;

	[[nodiscard]] virtual Texture GetCurrentTexture();

	void ForEachTexture(std::function<void(Texture)> func);

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
	bool hidden_{ false };
};

class TexturedToggleButton : public virtual ToggleButton, public virtual TexturedButton {
public:
	TexturedToggleButton() = default;
	TexturedToggleButton(
		const Rectangle<float>& rect, std::initializer_list<TextureOrKey> default_textures,
		std::initializer_list<TextureOrKey> hover_textures,
		std::initializer_list<TextureOrKey> pressed_textures,
		std::function<void()> on_activate_function = nullptr
	);

	virtual void OnMouseUp(const MouseUpEvent& e) override;

	[[nodiscard]] virtual Texture GetCurrentTexture() override;
	virtual void Draw() const override;
};

} // namespace ptgn