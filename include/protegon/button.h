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
    std::array<std::array<std::variant<Texture, TextureKey>, I>, 3> data;
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

class ToggleButton : public virtual Button {
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

class TexturedButton : public virtual Button {
public:
    TexturedButton() = default;
    // TODO: Add type check.
    template <typename T>
    TexturedButton(
        const Rectangle<int>& rect,
        T default,
        T hover,
        T pressed,
        std::function<void()> on_activate_function = nullptr) :
        Button{ rect, on_activate_function } {

        std::variant<Texture, TextureKey> default_on = default;
        std::variant<Texture, TextureKey> hover_on = hover;
        std::variant<Texture, TextureKey> pressed_on = pressed;

        for (std::size_t i = 0; i++ i < 2) {
            textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(i) = default_on;
            textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(i) = hover_on;
            textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(i) = pressed_on;
        }
    }
    virtual void Draw();
protected:
    void DrawImpl(std::size_t texture_array_index = 0);
private:
    // Must be initialized explicitly by a constructor.
    // Can technically exist uninitialized if button is default constructed (temporary object).
    // TODO: Figure out a way to store 1 here and 2 in the toggle button class
    TextureArray<2> textures_{};
};

class TexturedToggleButton : public TexturedButton, public ToggleButton {
public:
    TexturedToggleButton() = default;
    // TODO: Add type check.
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

        std::variant<Texture, TextureKey> default_on;
        std::variant<Texture, TextureKey> hover_on;
        std::variant<Texture, TextureKey> pressed_on;
        std::variant<Texture, TextureKey> default_off;
        std::variant<Texture, TextureKey> hover_off;
        std::variant<Texture, TextureKey> pressed_off;

        if constexpr (type_traits::is_safely_castable_v<T, Texture> ||
                      type_traits::is_safely_castable_v<T, TextureKey>) {
            default_on = default;
            hover_on   = hover;
            pressed_on = pressed;
            default_off = default;
            hover_off = hover;
            pressed_off = pressed;
        } else if constexpr (type_traits::is_safely_castable_v<T, TexturePair> ||
                             type_traits::is_safely_castable_v<T, TextureKeyPair>) {
            default_on  = default.first;
            hover_on    = hover.first;
            pressed_on  = pressed.first;
            default_off = default.second;
            hover_off   = hover.second;
            pressed_off = pressed.second;
        }

        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover_on;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed_on;

        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(1) = default_off;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(1) = hover_off;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(1) = pressed_off;

    }
    virtual void Draw() override;
};

} // namespace ptgn