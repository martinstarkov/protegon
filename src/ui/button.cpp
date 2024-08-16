#include "protegon/button.h"

#include <functional>
#include <utility>

#include "protegon/collision.h"
#include "protegon/game.h"
#include "utility/debug.h"

namespace ptgn {

Button::Button(const Rectangle<float>& rect, std::function<void()> on_activate_function) :
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
		// This needs to happen before RecheckState as it will not trigger if
		// enabled_ == false.
		enabled_ = interactable;
		SubscribeToMouseEvents();
		RecheckState();
	} else if (!interactable && subscribed) {
		UnsubscribeFromMouseEvents();
		button_state_ = InternalButtonState::IdleUp;
		if (on_hover_start_ != nullptr) {
			StopHover();
		}
		// This needs to happen after StopHover as it will not trigger if
		// enabled_ == true.
		enabled_ = interactable;
	}
}

void Button::Activate() {
	if (!enabled_) {
		return;
	}
	PTGN_ASSERT(
		on_activate_ != nullptr, "Cannot activate button which has no activate function set"
	);
	on_activate_();
	RecheckState();
}

void Button::StartHover() {
	if (!enabled_) {
		return;
	}
	PTGN_ASSERT(
		on_hover_start_ != nullptr,
		"Cannot start hover for button which has no hover start function set"
	);
	on_hover_start_();
}

void Button::StopHover() {
	if (!enabled_) {
		return;
	}
	PTGN_ASSERT(
		on_hover_stop_ != nullptr,
		"Cannot stop hover for button which has no hover stop function set"
	);
	on_hover_stop_();
}

void Button::SetOnActivate(std::function<void()> function) {
	on_activate_ = function;
}

void Button::SetOnHover(
	std::function<void()> start_hover_function, std::function<void()> stop_hover_function
) {
	on_hover_start_ = start_hover_function;
	on_hover_stop_	= stop_hover_function;
}

bool Button::InsideRectangle(const V2_int& position) const {
	return overlap::PointRectangle(position, rect_);
}

void Button::OnMouseEvent(MouseEvent type, const Event& event) {
	if (!enabled_) {
		return;
	}
	switch (type) {
		case MouseEvent::Move: {
			const MouseMoveEvent& e = static_cast<const MouseMoveEvent&>(event);
			bool is_in{ InsideRectangle(e.current) };
			if (is_in) {
				OnMouseMove(e);
			} else {
				OnMouseMoveOutside(e);
			}
			bool was_in{ InsideRectangle(e.previous) };
			if (!was_in && is_in) {
				OnMouseEnter(e);
			} else if (was_in && !is_in) {
				OnMouseLeave(e);
			}
			break;
		}
		case MouseEvent::Down: {
			const MouseDownEvent& e = static_cast<const MouseDownEvent&>(event);
			if (InsideRectangle(e.current)) {
				OnMouseDown(e);
			} else {
				OnMouseDownOutside(e);
			}
			break;
		}
		case MouseEvent::Up: {
			const MouseUpEvent& e = static_cast<const MouseUpEvent&>(event);
			if (InsideRectangle(e.current)) {
				OnMouseUp(e);
			} else {
				OnMouseUpOutside(e);
			}
			break;
		}
		default: break;
	}
}

bool Button::IsSubscribedToMouseEvents() const {
	return game.event.mouse.IsSubscribed((void*)this);
}

void Button::SubscribeToMouseEvents() {
	game.event.mouse.Subscribe((void*)this, [&](MouseEvent t, const Event& e) {
		OnMouseEvent(t, e);
	});
}

void Button::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe((void*)this);
}

void Button::OnMouseMove([[maybe_unused]] const MouseMoveEvent& e) {
	if (!enabled_) {
		return;
	}
	if (button_state_ == InternalButtonState::IdleUp) {
		button_state_ = InternalButtonState::Hover;
		if (on_hover_start_ != nullptr) {
			StartHover();
		}
	}
}

void Button::OnMouseMoveOutside([[maybe_unused]] const MouseMoveEvent& e) {
	if (!enabled_) {
		return;
	}
	if (button_state_ == InternalButtonState::Hover) {
		button_state_ = InternalButtonState::IdleUp;
		if (on_hover_stop_ != nullptr) {
			StopHover();
		}
	}
}

void Button::OnMouseEnter([[maybe_unused]] const MouseMoveEvent& e) {
	if (!enabled_) {
		return;
	}
	if (button_state_ == InternalButtonState::IdleUp) {
		button_state_ = InternalButtonState::Hover;
	} else if (button_state_ == InternalButtonState::IdleDown) {
		button_state_ = InternalButtonState::HoverPressed;
	} else if (button_state_ == InternalButtonState::HeldOutside) {
		button_state_ = InternalButtonState::Pressed;
	}
	if (on_hover_start_ != nullptr) {
		StartHover();
	}
}

void Button::OnMouseLeave([[maybe_unused]] const MouseMoveEvent& e) {
	if (!enabled_) {
		return;
	}
	if (button_state_ == InternalButtonState::Hover) {
		button_state_ = InternalButtonState::IdleUp;
	} else if (button_state_ == InternalButtonState::Pressed) {
		button_state_ = InternalButtonState::HeldOutside;
	} else if (button_state_ == InternalButtonState::HoverPressed) {
		button_state_ = InternalButtonState::IdleDown;
	}
	if (on_hover_stop_ != nullptr) {
		StopHover();
	}
}

void Button::OnMouseDown(const MouseDownEvent& e) {
	if (!enabled_) {
		return;
	}
	if (e.mouse == Mouse::Left && button_state_ == InternalButtonState::Hover) {
		button_state_ = InternalButtonState::Pressed;
	}
}

void Button::OnMouseDownOutside(const MouseDownEvent& e) {
	if (!enabled_) {
		return;
	}
	if (e.mouse == Mouse::Left && button_state_ == InternalButtonState::IdleUp) {
		button_state_ = InternalButtonState::IdleDown;
	}
}

void Button::OnMouseUp(const MouseUpEvent& e) {
	if (!enabled_) {
		return;
	}
	if (e.mouse == Mouse::Left) {
		if (button_state_ == InternalButtonState::Pressed) {
			button_state_ = InternalButtonState::Hover;
			if (on_activate_ != nullptr) {
				Activate();
			} else if (button_state_ == InternalButtonState::HoverPressed) {
				button_state_ = InternalButtonState::Hover;
			}
		}
	}
	RecheckState();
}

void Button::OnMouseUpOutside(const MouseUpEvent& e) {
	if (!enabled_) {
		return;
	}
	if (e.mouse == Mouse::Left) {
		if (button_state_ == InternalButtonState::IdleDown) {
			button_state_ = InternalButtonState::IdleUp;
		} else if (button_state_ == InternalButtonState::HeldOutside) {
			button_state_ = InternalButtonState::IdleUp;
		}
	}
}

const Rectangle<float>& Button::GetRectangle() const {
	return rect_;
}

void Button::RecheckState() {
	OnMouseEvent(
		MouseEvent::Move, MouseMoveEvent{ V2_int{ std::numeric_limits<int>().max(),
												  std::numeric_limits<int>().max() },
										  game.input.GetMousePosition() }
	);
}

void Button::SetRectangle(const Rectangle<float>& new_rectangle) {
	rect_ = new_rectangle;
	RecheckState();
}

ButtonState Button::GetState() const {
	if (button_state_ == InternalButtonState::Hover) {
		return ButtonState::Hover;
	} else if (button_state_ == InternalButtonState::Pressed) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

ToggleButton::ToggleButton(
	const Rectangle<float>& rect, std::function<void()> on_activate_function, bool initially_toggled
) :
	Button{ rect, on_activate_function } {
	toggled_ = initially_toggled;
}

ToggleButton::~ToggleButton() {
	UnsubscribeFromMouseEvents();
}

void ToggleButton::OnMouseUp(const MouseUpEvent& e) {
	if (e.mouse == Mouse::Left) {
		if (button_state_ == InternalButtonState::Pressed) {
			button_state_ = InternalButtonState::Hover;
			toggled_	  = !toggled_;
			if (on_activate_ != nullptr) {
				Activate();
			}
		} else if (button_state_ == InternalButtonState::HoverPressed) {
			button_state_ = InternalButtonState::Hover;
		}
	}
}

bool ToggleButton::IsToggled() const {
	return toggled_;
}

void ToggleButton::SetToggleState(bool toggled) {
	toggled_ = toggled;
	// RecheckState();
}

void ToggleButton::Toggle() {
	Activate();
	toggled_ = !toggled_;
}

SolidButton::SolidButton(
	const Rectangle<float>& rect, Color default_color, Color hover_color, Color pressed_color,
	std::function<void()> on_activate_function
) :
	Button{ rect, on_activate_function } {
	colors_.data.at(static_cast<std::size_t>(ButtonState::Default)).at(0) = default_color;
	colors_.data.at(static_cast<std::size_t>(ButtonState::Hover)).at(0)	  = hover_color;
	colors_.data.at(static_cast<std::size_t>(ButtonState::Pressed)).at(0) = pressed_color;
}

void SolidButton::DrawImpl(std::size_t color_array_index) const {
	const Color& color = GetCurrentColorImpl(GetState(), color_array_index);
	game.renderer.DrawRectangleFilled(rect_.pos, rect_.size, color);
}

void SolidButton::Draw() const {
	DrawImpl(0);
}

const Color& SolidButton::GetCurrentColorImpl(ButtonState state, std::size_t color_array_index)
	const {
	auto& color_array  = colors_.data.at(static_cast<std::size_t>(state));
	const Color& color = color_array.at(color_array_index);
	return color;
}

const Color& SolidButton::GetCurrentColor() const {
	return GetCurrentColorImpl(GetState(), 0);
}

TexturedButton::TexturedButton(
	const Rectangle<float>& rect, const TextureOrKey& default_texture,
	const TextureOrKey& hover_texture, const TextureOrKey& pressed_texture,
	std::function<void()> on_activate_function
) :
	Button{ rect, on_activate_function } {
	textures_.data.at(static_cast<std::size_t>(ButtonState::Default)).at(0) = default_texture;
	textures_.data.at(static_cast<std::size_t>(ButtonState::Hover)).at(0)	= hover_texture;
	textures_.data.at(static_cast<std::size_t>(ButtonState::Pressed)).at(0) = pressed_texture;
}

bool TexturedButton::GetVisibility() const {
	return !hidden_;
}

void TexturedButton::ForEachTexture(std::function<void(Texture)> func) {
	for (std::size_t state = 0; state < 3; state++) {
		for (std::size_t texture_array_index = 0; texture_array_index < 2; texture_array_index++) {
			Texture texture =
				GetCurrentTextureImpl(static_cast<ButtonState>(state), texture_array_index);
			if (!texture.IsValid()) {
				texture = GetCurrentTextureImpl(ButtonState::Default, texture_array_index);
				if (!texture.IsValid()) {
					texture = GetCurrentTextureImpl(ButtonState::Default, 0);
				}
			}
			if (texture.IsValid()) {
				func(texture);
			}
		}
	}
}

void TexturedButton::SetVisibility(bool visibility) {
	hidden_ = !visibility;
}

void TexturedButton::DrawImpl(std::size_t texture_array_index) const {
	if (hidden_) {
		return;
	}
	Texture texture = GetCurrentTextureImpl(GetState(), texture_array_index);
	if (!texture.IsValid()) {
		texture = GetCurrentTextureImpl(ButtonState::Default, 0);
	}
	PTGN_ASSERT(texture.IsValid(), "Button state texture (or default texture) must be valid");
	// TODO: Fix
	// texture.Draw(rect_);
}

void TexturedButton::Draw() const {
	DrawImpl(0);
}

Texture TexturedButton::GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index)
	const {
	auto& texture_array = textures_.data.at(static_cast<std::size_t>(state));

	const TextureOrKey& texture_state = texture_array.at(texture_array_index);

	Texture texture;

	if (std::holds_alternative<TextureKey>(texture_state)) {
		const TextureKey key{ std::get<TextureKey>(texture_state) };
		PTGN_ASSERT(game.texture.Has(key), "Cannot get button texture which has not been loaded");
		texture = game.texture.Get(key);
	} else if (std::holds_alternative<Texture>(texture_state)) {
		texture = std::get<Texture>(texture_state);
	}

	return texture;
}

Texture TexturedButton::GetCurrentTexture() {
	return GetCurrentTextureImpl(GetState(), 0);
}

TexturedToggleButton::TexturedToggleButton(
	const Rectangle<float>& rect, std::initializer_list<TextureOrKey> default_textures,
	std::initializer_list<TextureOrKey> hover_textures,
	std::initializer_list<TextureOrKey> pressed_textures, std::function<void()> on_activate_function
) {
	rect_		 = rect;
	on_activate_ = on_activate_function;
	SubscribeToMouseEvents();

	// TODO: Perhaps allow for more than two entries later
	PTGN_ASSERT(default_textures.size() <= 2);
	PTGN_ASSERT(hover_textures.size() <= 2);
	PTGN_ASSERT(pressed_textures.size() <= 2);

	auto set_textures = [&](const auto& list, const ButtonState state) -> void {
		std::size_t i = 0;
		for (auto it = list.begin(); it != list.end(); ++it) {
			textures_.data.at(static_cast<std::size_t>(state)).at(i) = *it;
			++i;
		}
	};

	set_textures(default_textures, ButtonState::Default);
	set_textures(hover_textures, ButtonState::Hover);
	set_textures(pressed_textures, ButtonState::Pressed);
}

void TexturedToggleButton::Draw() const {
	DrawImpl(static_cast<std::size_t>(toggled_));
}

Texture TexturedToggleButton::GetCurrentTexture() {
	return GetCurrentTextureImpl(GetState(), static_cast<std::size_t>(toggled_));
}

void TexturedToggleButton::OnMouseUp(const MouseUpEvent& e) {
	ToggleButton::OnMouseUp(e);
}

} // namespace ptgn
