#pragma once

#include <array> // std::array
#include <tuple> // std::tuple
#include <variant> // std::variant

#include "event.h"
#include "events.h"
#include "rectangle.h"
#include "texture.h"
#include "resources.h"
#include "type_traits.h"

namespace ptgn {

// First element  = texture for 'not toggled' state of button
// Second element = texture for 'toggled' state of button.
using TexturePair = std::pair<Texture, Texture>;
// First element  = texture key for 'not toggled' state of button
// Second element = texture key for 'toggled' state of button.
using TextureKeyPair = std::pair<TextureKey, TextureKey>;

enum class ButtonState : std::size_t {
    DEFAULT = 0,
    HOVER   = 1,
    PRESSED = 2
};

template <size_t I>
struct TextureArray {
    std::array<std::array<Texture, I>, 3> data;
};

class Button {
public:
    Button() = default;
    Button(const Rectangle<int>& rect,
 		    std::function<void()> on_activate_function = nullptr);
    virtual ~Button();
    virtual void Activate();
    virtual void SetOnActivate(std::function<void()> function);
    virtual void OnMouseEvent(const Event<MouseEvent>& event);
    virtual void SubscribeToMouseEvents();
    virtual void UnsubscribeFromMouseEvents();
    virtual void OnMouseMove(const MouseMoveEvent& e);
    virtual void OnMouseMoveOutside(const MouseMoveEvent& e);
    virtual void OnMouseEnter(const MouseMoveEvent& e);
    virtual void OnMouseLeave(const MouseMoveEvent& e);
    virtual void OnMouseDown(const MouseDownEvent& e);
    virtual void OnMouseDownOutside(const MouseDownEvent& e);
    virtual void OnMouseUp(const MouseUpEvent& e);
    virtual void OnMouseUpOutside(const MouseUpEvent& e);
    const Rectangle<int>& GetRectangle() const;
    void SetRectangle(const Rectangle<int>& new_rectangle);
    ButtonState GetState() const;
protected:
        enum class InternalButtonState : std::size_t {
            IDLE_UP = 0,
            HOVER = 1,
            PRESSED = 2,
            HELD_OUTSIDE = 3,
            IDLE_DOWN = 4,
            HOVER_PRESSED = 5
        };
    Rectangle<int> rect_{};
    std::function<void()> on_activate_{ nullptr };
    InternalButtonState button_state_{ InternalButtonState::IDLE_UP };
};

class ToggleButton : public Button {
public:
    using Button::Button;
    ToggleButton(const Rectangle<int>& rect,
 				 std::function<void()> on_activate_function = nullptr,
 				 bool initially_toggled = false);
    ~ToggleButton();
    // Start in non toggled state.
    virtual void OnMouseUp(const MouseUpEvent& e) override;
    bool IsToggled() const;
    void Toggle();
protected:
    bool toggled_{ false };
};

class TexturedButton : public Button {
public:
    TexturedButton() = default;
    template <typename T>
    TexturedButton(
        const Rectangle<int>& rect,
        T default,
        T hover,
        T pressed,
        std::function<void()> on_activate_function = nullptr) :
        Button{ rect, on_activate_function } {

        Texture default_on;
        Texture hover_on;
        Texture pressed_on;

        if constexpr (std::is_same_v<T, Texture>) {
            default_on = default;
            hover_on   = hover;
            pressed_on = pressed;
        }
        else if constexpr (std::is_same_v<T, TextureKey>) {
            default_on = *texture::Get(default);
            hover_on   = *texture::Get(hover);
            pressed_on = *texture::Get(pressed);
        }

        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed_on;
    }
    virtual void Draw();
private:
    // Must be initialized explicitly by a constructor.
    // Can technically exist uninitialized if button is default constructed (temporary object).
    TextureArray<1> textures_{};
};

class TexturedToggleButton : public ToggleButton {
public:
    TexturedToggleButton() = default;
    template <typename T>
    TexturedToggleButton(
        const Rectangle<int>& rect,
        T default,
        T hover,
        T pressed,
        std::function<void()> on_activate_function = nullptr) {
        rect_ = rect;
        on_activate_ = on_activate_function;
        SubscribeToMouseEvents();

        Texture default_on;
        Texture hover_on;
        Texture pressed_on;
        Texture default_off;
        Texture hover_off;
        Texture pressed_off;

        if constexpr (type_traits::is_safely_castable_v<T, Texture>) {
            default_on = default;
            hover_on   = hover;
            pressed_on = pressed;
        } else if constexpr (type_traits::is_safely_castable_v<T, TextureKey>) {
            default_on = *texture::Get(default);
            hover_on   = *texture::Get(hover);
            pressed_on = *texture::Get(pressed);
        } else if constexpr (type_traits::is_safely_castable_v<T, TexturePair>) {
            default_on  = default.first;
            hover_on    = hover.first;
            pressed_on  = pressed.first;
            default_off = default.second;
            hover_off   = hover.second;
            pressed_off = pressed.second;
        } else if constexpr (type_traits::is_safely_castable_v<T, TextureKeyPair>) {
            default_on  = *texture::Get(default.first);
            hover_on    = *texture::Get(hover.first);
            pressed_on  = *texture::Get(pressed.first);
            default_off = *texture::Get(default.second);
            hover_off   = *texture::Get(hover.second);
            pressed_off = *texture::Get(pressed.second);
        }

        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed_on;

        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(1) = default_off;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(1) = hover_off;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(1) = pressed_off;
    }
    virtual void Draw();
private:
    // Must be initialized explicitly by a constructor.
    // Can technically exist uninitialized if button is default constructed (temporary object).
    TextureArray<2> textures_{};
};

} // namespace ptgn