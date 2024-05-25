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
    virtual bool GetInteractable() const final;
    virtual void SetInteractable(bool interactable);

    // These allow for manually triggering button callback events.
    virtual void Activate();
    virtual void StartHover();
    virtual void StopHover();

    virtual void SetOnActivate(std::function<void()> function);
    virtual void SetOnHover(std::function<void()> start_hover_function = nullptr, std::function<void()> stop_hover_function = nullptr);
    virtual void OnMouseEvent(const Event<MouseEvent>& event);
    virtual bool IsSubscribedToMouseEvents() const final;
    virtual void SubscribeToMouseEvents() final;
    virtual void UnsubscribeFromMouseEvents() final;
    virtual void OnMouseMove(const MouseMoveEvent& e);
    virtual void OnMouseMoveOutside(const MouseMoveEvent& e);
    virtual void OnMouseEnter(const MouseMoveEvent& e);
    virtual void OnMouseLeave(const MouseMoveEvent& e);
    virtual void OnMouseDown(const MouseDownEvent& e);
    virtual void OnMouseDownOutside(const MouseDownEvent& e);
    virtual void OnMouseUp(const MouseUpEvent& e);
    virtual void OnMouseUpOutside(const MouseUpEvent& e);
    const Rectangle<float>& GetRectangle() const;
    void SetRectangle(const Rectangle<float>& new_rectangle);
    ButtonState GetState() const;
    bool InsideRectangle(const V2_int& position) const;
protected:
    enum class InternalButtonState : std::size_t {
        IDLE_UP = 0,
        HOVER = 1,
        PRESSED = 2,
        HELD_OUTSIDE = 3,
        IDLE_DOWN = 4,
        HOVER_PRESSED = 5
    };
    Rectangle<float> rect_{};
    std::function<void()> on_activate_{ nullptr };
    std::function<void()> on_hover_start_{ nullptr };
    std::function<void()> on_hover_stop_{ nullptr };
    InternalButtonState button_state_{ InternalButtonState::IDLE_UP };
    bool enabled_{ true };
};

class SolidButton : public virtual Button {
public:
    SolidButton() = default;
    SolidButton(
        const Rectangle<float>& rect,
        Color default,
        Color hover,
        Color pressed,
        std::function<void()> on_activate_function = nullptr) :
        Button{ rect, on_activate_function } {
        colors_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default;
        colors_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover;
        colors_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed;
    }
    virtual void Draw() const;
    virtual const Color& GetCurrentColor() const;
protected:
    const Color& GetCurrentColorImpl(ButtonState state, std::size_t color_array_index = 0) const;
    void DrawImpl(std::size_t color_array_index = 0) const;
    ColorArray<2> colors_{};
};

class ToggleButton : public virtual Button {
public:
    using Button::Button;
    ToggleButton(const Rectangle<float>& rect,
 				 std::function<void()> on_activate_function = nullptr,
 				 bool initially_toggled = false);
    ~ToggleButton();
    // Start in non toggled state.
    virtual void OnMouseUp(const MouseUpEvent& e) override;
    bool IsToggled() const;
    void Toggle();
    void SetToggleState(bool toggled);
protected:
    bool toggled_{ false };
};

class TexturedButton : public virtual Button {
public:
    TexturedButton() = default;
    template <typename T, type_traits::is_safely_castable_to_one_of<T, Texture, TextureKey> = true>
    TexturedButton(
        const Rectangle<float>& rect,
        T default,
        T hover,
        T pressed,
        std::function<void()> on_activate_function = nullptr) :
        Button{ rect, on_activate_function } {
        textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default;
        textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover;
        textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed;
    }

    virtual bool GetVisibility() const final;
    virtual void SetVisibility(bool visibility);

    virtual void Draw() const;
    virtual Texture GetCurrentTexture();
    void ForEachTexture(std::function<void(Texture)> func);
protected:
    Texture GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index = 0) const;
    void DrawImpl(std::size_t texture_array_index = 0) const;
    // Must be initialized explicitly by a constructor.
    // Can technically exist uninitialized if button is default constructed (temporary object).
    // TODO: Figure out a way to store 1 here and 2 in the toggle button class
    TextureArray<2> textures_{};
    bool hidden_{ false };
};

class TexturedToggleButton : public ToggleButton, public TexturedButton {
public:
    TexturedToggleButton() = default;
    template <typename T, type_traits::is_safely_castable_to_one_of<T, Texture, TextureKey> = true>
    TexturedToggleButton(
        const Rectangle<float>& rect,
        std::initializer_list<T> default,
        std::initializer_list<T> hover,
        std::initializer_list<T> pressed,
        std::function<void()> on_activate_function = nullptr) {
        rect_ = rect;
        on_activate_ = on_activate_function;
        SubscribeToMouseEvents();

        // TODO: Perhaps allow for more than two entries later
        assert(default.size() <= 2);
        assert(hover.size()   <= 2);
        assert(pressed.size() <= 2);

        auto set_textures = [&](const std::initializer_list<T>& list, const ButtonState state) -> void {
            std::size_t i = 0;
            for (auto it = list.begin(); it != list.end(); ++it) {
                textures_.data.at(static_cast<std::size_t>(state)).at(i) = *it;
                ++i;
            }
        };

        set_textures(default, ButtonState::DEFAULT);
        set_textures(hover, ButtonState::HOVER);
        set_textures(pressed, ButtonState::PRESSED);
    }
    virtual Texture GetCurrentTexture() override;
    virtual void Draw() const override;
};

} // namespace ptgn