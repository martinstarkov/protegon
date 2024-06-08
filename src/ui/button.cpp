#include "protegon/button.h"

#include <functional>
#include <utility>

#include "protegon/collision.h"
#include "protegon/input.h"
#include "core/game.h"
#include "utility/debug.h"

namespace ptgn {

Button::Button(const Rectangle<float>& rect,
			   std::function<void()> on_activate_function) :
	on_activate_{ on_activate_function } {
    SetRectangle(rect);
	SubscribeToMouseEvents();
}

Button::~Button() {
 	UnsubscribeFromMouseEvents();
}

bool Button::GetInteractable() const {
    return enabled_;
}

void Button::SetInteractable(bool interactable) {
    bool subscribed = IsSubscribedToMouseEvents();
    if (interactable && !subscribed) {
        // This needs to happen before RecheckState as it will not trigger if enabled_ == false.
        enabled_ = interactable;
        SubscribeToMouseEvents();
        RecheckState();
    } else if (!interactable && subscribed) {
        UnsubscribeFromMouseEvents();
        button_state_ = InternalButtonState::IDLE_UP;
        if (on_hover_start_ != nullptr) {
            StopHover();
        }
        // This needs to happen after StopHover as it will not trigger if enabled_ == true.
        enabled_ = interactable;
    }
}

void Button::Activate() {
    if (!enabled_) return;
    PTGN_CHECK(on_activate_ != nullptr, "Cannot activate button which has no activate function set");
 	on_activate_();
    RecheckState();
}

void Button::StartHover() {
    if (!enabled_) return;
    PTGN_CHECK(on_hover_start_ != nullptr, "Cannot start hover for button which has no hover start function set");
    on_hover_start_();
}

void Button::StopHover() {
    if (!enabled_) return;
    PTGN_CHECK(on_hover_stop_ != nullptr, "Cannot stop hover for button which has no hover stop function set");
    on_hover_stop_();
}

void Button::SetOnActivate(std::function<void()> function) {
 	on_activate_ = function;
}

void Button::SetOnHover(std::function<void()> start_hover_function, std::function<void()> stop_hover_function) {
    on_hover_start_ = start_hover_function;
    on_hover_stop_ = stop_hover_function;
}

bool Button::InsideRectangle(const V2_int& position) const {
    return overlap::PointRectangle(position, rect_);
}

void Button::OnMouseEvent(const Event<MouseEvent>& event) {
    if (!enabled_) return;
    switch (event.Type()) {
 	    case MouseEvent::MOVE:
 	    {
 		    const MouseMoveEvent& e = static_cast<const MouseMoveEvent&>(event);
 		    bool is_in{ InsideRectangle(e.current) };
 		    if (is_in)
 			    OnMouseMove(e);
 		    else
 			    OnMouseMoveOutside(e);
 		    bool was_in{ InsideRectangle(e.previous) };
 		    if (!was_in && is_in)
 			    OnMouseEnter(e);
 		    else if (was_in && !is_in)
 			    OnMouseLeave(e);
 		    break;
 	    }
 	    case MouseEvent::DOWN:
 	    {
 		    const MouseDownEvent& e = static_cast<const MouseDownEvent&>(event);
 		    if (InsideRectangle(e.current))
 			    OnMouseDown(e);
 		    else
 			    OnMouseDownOutside(e);
 		    break;
 	    }
 	    case MouseEvent::UP:
 	    {
 		    const MouseUpEvent& e = static_cast<const MouseUpEvent&>(event);
 		    if (InsideRectangle(e.current))
 			    OnMouseUp(e);
 		    else
 			    OnMouseUpOutside(e);
 		    break;
 	    }
 	    default:
 		    break;
    }
}

bool Button::IsSubscribedToMouseEvents() const {
    return global::GetGame().event.mouse_event.IsSubscribed((void*)this);
}

void Button::SubscribeToMouseEvents() {
    global::GetGame().event.mouse_event.Subscribe((void*)this, std::bind(&Button::OnMouseEvent, this, std::placeholders::_1));
}

void Button::UnsubscribeFromMouseEvents() {
    global::GetGame().event.mouse_event.Unsubscribe((void*)this);
}

void Button::OnMouseMove(const MouseMoveEvent& e) {
    if (!enabled_) return;
    if (button_state_ == InternalButtonState::IDLE_UP) {
        button_state_ = InternalButtonState::HOVER;
        if (on_hover_start_ != nullptr) {
            StartHover();
        }
    }
}

void Button::OnMouseMoveOutside(const MouseMoveEvent& e) {
    if (!enabled_) return;
    if (button_state_ == InternalButtonState::HOVER) {
        button_state_ = InternalButtonState::IDLE_UP;
        if (on_hover_stop_ != nullptr) {
            StopHover();
        }
    }
}

void Button::OnMouseEnter(const MouseMoveEvent& e) {
    if (!enabled_) return;
    if (button_state_ == InternalButtonState::IDLE_UP)
 	    button_state_ = InternalButtonState::HOVER;
    else if (button_state_ == InternalButtonState::IDLE_DOWN)
 	    button_state_ = InternalButtonState::HOVER_PRESSED;
    else if (button_state_ == InternalButtonState::HELD_OUTSIDE)
 	    button_state_ = InternalButtonState::PRESSED;
    if (on_hover_start_ != nullptr) {
        StartHover();
    }
}

void Button::OnMouseLeave(const MouseMoveEvent& e) {
    if (!enabled_) return;
    if (button_state_ == InternalButtonState::HOVER)
 	    button_state_ = InternalButtonState::IDLE_UP;
    else if (button_state_ == InternalButtonState::PRESSED)
 	    button_state_ = InternalButtonState::HELD_OUTSIDE;
    else if (button_state_ == InternalButtonState::HOVER_PRESSED)
 	    button_state_ = InternalButtonState::IDLE_DOWN;
    if (on_hover_stop_ != nullptr) {
        StopHover();
    }
}

void Button::OnMouseDown(const MouseDownEvent& e) {
    if (!enabled_) return;
    if (e.mouse == Mouse::LEFT && button_state_ == InternalButtonState::HOVER)
        button_state_ = InternalButtonState::PRESSED;
}

void Button::OnMouseDownOutside(const MouseDownEvent& e) {
    if (!enabled_) return;
    if (e.mouse == Mouse::LEFT && button_state_ == InternalButtonState::IDLE_UP)
 	    button_state_ = InternalButtonState::IDLE_DOWN;
}

void Button::OnMouseUp(const MouseUpEvent& e) {
    if (!enabled_) return;
    if (e.mouse == Mouse::LEFT) {
        if (button_state_ == InternalButtonState::PRESSED) {
            button_state_ = InternalButtonState::HOVER;
            if (on_activate_ != nullptr) {
                Activate();
            } else if (button_state_ == InternalButtonState::HOVER_PRESSED) {
                button_state_ = InternalButtonState::HOVER;
            }
        }
    }
    RecheckState();
}

void Button::OnMouseUpOutside(const MouseUpEvent& e) {
    if (!enabled_) return;
    if (e.mouse == Mouse::LEFT) {
 	    if (button_state_ == InternalButtonState::IDLE_DOWN)
 		    button_state_ = InternalButtonState::IDLE_UP;
 	    else if (button_state_ == InternalButtonState::HELD_OUTSIDE)
 		    button_state_ = InternalButtonState::IDLE_UP;
    }
}

const Rectangle<float>& Button::GetRectangle() const {
    return rect_;
}

void Button::RecheckState() {
    OnMouseEvent(MouseMoveEvent{ V2_int{ std::numeric_limits<int>().max(), std::numeric_limits<int>().max() }, global::GetGame().input.GetMousePosition() });
}

void Button::SetRectangle(const Rectangle<float>& new_rectangle) {
 	rect_ = new_rectangle;
    RecheckState();
}

ButtonState Button::GetState() const {
    if (button_state_ == InternalButtonState::HOVER)
 	    return ButtonState::HOVER;
    else if (button_state_ == InternalButtonState::PRESSED)
 	    return ButtonState::PRESSED;
    else
 	    return ButtonState::DEFAULT;
}

ToggleButton::ToggleButton(
    const Rectangle<float>& rect,
    std::function<void()> on_activate_function,
 	bool initially_toggled) : Button{ rect, on_activate_function } {
 	toggled_ = initially_toggled;
}

ToggleButton::~ToggleButton() {
 	UnsubscribeFromMouseEvents();
}

void ToggleButton::OnMouseUp(const MouseUpEvent& e) {
 	if (e.mouse == Mouse::LEFT) {
 		if (button_state_ == InternalButtonState::PRESSED) {
 			button_state_ = InternalButtonState::HOVER;
            toggled_ = !toggled_;
 			if (on_activate_ != nullptr)
 				Activate();
 		} else if (button_state_ == InternalButtonState::HOVER_PRESSED) {
 			button_state_ = InternalButtonState::HOVER;
 		}
 	}
}

bool ToggleButton::IsToggled() const {
 	return toggled_;
}

void ToggleButton::SetToggleState(bool toggled) {
    toggled_ = toggled;
    //RecheckState();
 }

void ToggleButton::Toggle() {
 	Activate();
 	toggled_ = !toggled_;
}

SolidButton::SolidButton(
    const Rectangle<float>& rect,
    Color default,
    Color hover,
    Color pressed,
    std::function<void()> on_activate_function) :
    Button{ rect, on_activate_function } {
    colors_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default;
    colors_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover;
    colors_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed;
}

void SolidButton::DrawImpl(std::size_t color_array_index) const {
    const Color& color = GetCurrentColorImpl(GetState(), color_array_index);
    rect_.DrawSolid(color);
}

void SolidButton::Draw() const {
    DrawImpl(0);
}

const Color& SolidButton::GetCurrentColorImpl(ButtonState state, std::size_t color_array_index) const {
    auto& color_array = colors_.data.at(static_cast<std::size_t>(state));
    const Color& color = color_array.at(color_array_index);
    return color;
}

const Color& SolidButton::GetCurrentColor() const {
    return GetCurrentColorImpl(GetState(), 0);
}

TexturedButton::TexturedButton(
    const Rectangle<float>& rect,
    const TextureOrKey& default,
    const TextureOrKey& hover,
    const TextureOrKey& pressed,
    std::function<void()> on_activate_function) :
    Button{ rect, on_activate_function } {
    textures_.data.at(static_cast<std::size_t>(ButtonState::DEFAULT)).at(0) = default;
    textures_.data.at(static_cast<std::size_t>(ButtonState::HOVER)).at(0) = hover;
    textures_.data.at(static_cast<std::size_t>(ButtonState::PRESSED)).at(0) = pressed;
}

bool TexturedButton::GetVisibility() const {
    return !hidden_;
}

void TexturedButton::ForEachTexture(std::function<void(Texture)> func) {
    for (std::size_t state = 0; state < 3; state++) {
        for (std::size_t texture_array_index = 0; texture_array_index < 2; texture_array_index++) {
            Texture texture = GetCurrentTextureImpl(static_cast<ButtonState>(state), texture_array_index);
            if (!texture.IsValid()) {
                texture = GetCurrentTextureImpl(ButtonState::DEFAULT, texture_array_index);
                if (!texture.IsValid()) {
                    texture = GetCurrentTextureImpl(ButtonState::DEFAULT, 0);
                }
            }
            if (texture.IsValid())
                func(texture);
        }
    }
}

void TexturedButton::SetVisibility(bool visibility) {
    hidden_ = !visibility;
}

void TexturedButton::DrawImpl(std::size_t texture_array_index) const {
    if (hidden_) return;
    Texture texture = GetCurrentTextureImpl(GetState(), texture_array_index);
    if (!texture.IsValid()) {
        texture = GetCurrentTextureImpl(ButtonState::DEFAULT, 0);
    }
    PTGN_ASSERT(texture.IsValid(), "Button state texture (or default texture) must be valid");
    texture.Draw(rect_);
}

void TexturedButton::Draw() const {
    DrawImpl(0);
}

Texture TexturedButton::GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index) const {
    auto& texture_array = textures_.data.at(static_cast<std::size_t>(state));

    const TextureOrKey& texture_state = texture_array.at(texture_array_index);

    Texture texture;

    if (std::holds_alternative<TextureKey>(texture_state)) {
        const TextureKey key{ std::get<TextureKey>(texture_state) };
        PTGN_ASSERT(texture::Has(key), "Cannot get button texture which has not been loaded");
        texture = texture::Get(key);
    } else if (std::holds_alternative<Texture>(texture_state)) {
        texture = std::get<Texture>(texture_state);
    }

    return texture;
}

Texture TexturedButton::GetCurrentTexture() {
    return GetCurrentTextureImpl(GetState(), 0);
}

TexturedToggleButton::TexturedToggleButton(
    const Rectangle<float>& rect,
    std::initializer_list<TextureOrKey> default,
    std::initializer_list<TextureOrKey> hover,
    std::initializer_list<TextureOrKey> pressed,
    std::function<void()> on_activate_function) {
    rect_ = rect;
    on_activate_ = on_activate_function;
    SubscribeToMouseEvents();

    // TODO: Perhaps allow for more than two entries later
    PTGN_CHECK(default.size() <= 2);
    PTGN_CHECK(hover.size() <= 2);
    PTGN_CHECK(pressed.size() <= 2);

    auto set_textures = [&](const auto& list, const ButtonState state) -> void {
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

void TexturedToggleButton::Draw() const {
    DrawImpl(static_cast<std::size_t>(toggled_));
}

Texture TexturedToggleButton::GetCurrentTexture() {
    return GetCurrentTextureImpl(GetState(), static_cast<std::size_t>(toggled_));
}

} // namespace ptgn
