#pragma once

#include <array>
#include <tuple>
#include <variant>

#include "event.h"
#include "events.h"
#include "polygon.h"
#include "texture.h"
#include "resources.h"
#include "type_traits.h"

namespace ptgn {

using TextureOrKey = std::variant<Texture, TextureKey>;

enum class ButtonState : std::size_t {
    DEFAULT = 0,
    HOVER   = 1,
    PRESSED = 2
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
    virtual bool GetInteractable() const final;
    virtual void SetInteractable(bool interactable);

    // These allow for manually triggering button callback events.
    virtual void Activate();
    virtual void StartHover();
    virtual void StopHover();

    virtual void SetOnActivate(std::function<void()> function);
    virtual void SetOnHover(
        std::function<void()> start_hover_function = nullptr,
        std::function<void()> stop_hover_function = nullptr
    );

    virtual bool IsSubscribedToMouseEvents() const final;
    virtual void SubscribeToMouseEvents() final;
    virtual void UnsubscribeFromMouseEvents() final;

    virtual void OnMouseEvent(const Event<MouseEvent>& event);
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

    Rectangle<float> rect_;
    std::function<void()> on_activate_;
    std::function<void()> on_hover_start_;
    std::function<void()> on_hover_stop_;
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
        std::function<void()> on_activate_function = nullptr
    );

    virtual void Draw() const;
    
    virtual const Color& GetCurrentColor() const;
protected:
    const Color& GetCurrentColorImpl(ButtonState state, std::size_t color_array_index = 0) const;
    
    void DrawImpl(std::size_t color_array_index = 0) const;
protected:
    ColorArray<2> colors_{};
};

class ToggleButton : public virtual Button {
public:
    using Button::Button;

    ToggleButton(
        const Rectangle<float>& rect,
 		std::function<void()> on_activate_function = nullptr,
 		bool initially_toggled = false
    );
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
    TexturedButton(
        const Rectangle<float>& rect,
        const TextureOrKey& default,
        const TextureOrKey& hover,
        const TextureOrKey& pressed,
        std::function<void()> on_activate_function = nullptr
    );

    virtual bool GetVisibility() const final;
    virtual void SetVisibility(bool visibility);

    virtual void Draw() const;

    virtual Texture GetCurrentTexture();
    
    void ForEachTexture(std::function<void(Texture)> func);
protected:
    // This function does not check for the validity of the returned texture.
    Texture GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index = 0) const;

    void DrawImpl(std::size_t texture_array_index = 0) const;

protected:
    // Must be initialized explicitly by a constructor.
    // Can technically exist uninitialized if button is default constructed (temporary object).
    // TODO: Figure out a way to store 1 here and 2 in the toggle button class
    TextureArray<2> textures_;
    bool hidden_{ false };
};

class TexturedToggleButton : public ToggleButton, public TexturedButton {
public:
    TexturedToggleButton() = default;
    TexturedToggleButton(
        const Rectangle<float>& rect,
        std::initializer_list<TextureOrKey> default,
        std::initializer_list<TextureOrKey> hover,
        std::initializer_list<TextureOrKey> pressed,
        std::function<void()> on_activate_function = nullptr
    );

    virtual Texture GetCurrentTexture() override;
    virtual void Draw() const override;
};

} // namespace ptgn