#include "protegon/button.h"

#include <array>
#include <functional>
#include <initializer_list>
#include <limits>
#include <type_traits>
#include <variant>

#include "core/manager.h"
#include "event/event_handler.h"
#include "event/input_handler.h"
#include "event/mouse.h"
#include "protegon/collision.h"
#include "protegon/color.h"
#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/game.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

Button::Button(const Rectangle<float>& rect, const ButtonActivateFunction& on_activate_function) :
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

void Button::Draw() const {
	DrawHollow();
}

void Button::DrawHollow(float line_width) const {
	game.renderer.DrawRectangleHollow(rect_, color::Black, line_width);
}

void Button::DrawFilled() const {
	game.renderer.DrawRectangleFilled(rect_, color::Black);
}

void Button::SetInteractable(bool interactable) {
	bool subscribed = IsSubscribedToMouseEvents();
	if (interactable && !subscribed) {
		// This needs to happen before RecheckState as it will not trigger if
		// enabled_ == false.
		enabled_ = interactable;
		SubscribeToMouseEvents();
		RecheckState();
		if (on_enable_) {
			std::invoke(on_enable_);
		}
	} else if (!interactable && subscribed) {
		UnsubscribeFromMouseEvents();
		button_state_ = InternalButtonState::IdleUp;
		if (on_hover_stop_ != nullptr) {
			StopHover();
		}
		// This needs to happen after StopHover as it will not trigger if
		// enabled_ == true.
		enabled_ = interactable;
		if (on_disable_) {
			std::invoke(on_disable_);
		}
	}
}

void Button::Activate() {
	if (!enabled_) {
		return;
	}
	PTGN_ASSERT(
		on_activate_ != nullptr, "Cannot activate button which has no activate function set"
	);
	std::invoke(on_activate_);
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
	std::invoke(on_hover_start_);
}

void Button::StopHover() {
	if (!enabled_) {
		return;
	}
	PTGN_ASSERT(
		on_hover_stop_ != nullptr,
		"Cannot stop hover for button which has no hover stop function set"
	);
	std::invoke(on_hover_stop_);
}

void Button::SetOnActivate(const ButtonActivateFunction& function) {
	on_activate_ = function;
}

void Button::SetOnHover(
	const ButtonHoverFunction& start_hover_function, const ButtonHoverFunction& stop_hover_function
) {
	on_hover_start_ = start_hover_function;
	on_hover_stop_	= stop_hover_function;
}

void Button::SetOnEnable(const ButtonEnableFunction& enable_function) {
	on_enable_ = enable_function;
}

void Button::SetOnDisable(const ButtonDisableFunction& disable_function) {
	on_disable_ = disable_function;
}

bool Button::InsideRectangle(const V2_int& position) const {
	return game.collision.overlap.PointRectangle(position, rect_);
}

void Button::OnMouseEvent(MouseEvent type, const Event& event) {
	if (!enabled_) {
		return;
	}
	switch (type) {
		case MouseEvent::Move: {
			const auto& e = static_cast<const MouseMoveEvent&>(event);
			bool is_in{ InsideRectangle(e.current) };
			if (is_in) {
				OnMouseMove(e);
			} else {
				OnMouseMoveOutside(e);
			}
			if (bool was_in{ InsideRectangle(e.previous) }; !was_in && is_in) {
				OnMouseEnter(e);
			} else if (was_in && !is_in) {
				OnMouseLeave(e);
			}
			break;
		}
		case MouseEvent::Down: {
			if (const auto& e = static_cast<const MouseDownEvent&>(event);
				InsideRectangle(e.current)) {
				OnMouseDown(e);
			} else {
				OnMouseDownOutside(e);
			}
			break;
		}
		case MouseEvent::Up: {
			if (const auto& e = static_cast<const MouseUpEvent&>(event);
				InsideRectangle(e.current)) {
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
	return game.event.mouse.IsSubscribed(this);
}

void Button::SubscribeToMouseEvents() {
	game.event.mouse.Subscribe(this, [this](MouseEvent t, const Event& e) { OnMouseEvent(t, e); });
}

void Button::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe(this);
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
			}
		} else if (button_state_ == InternalButtonState::HoverPressed) {
			button_state_ = InternalButtonState::Hover;
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
		MouseEvent::Move,
		MouseMoveEvent{ V2_int{ std::numeric_limits<int>::max(), std::numeric_limits<int>::max() },
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

void TextButton::SetBorder(bool draw_border) {
	draw_border_ = draw_border;
}

bool TextButton::HasBorder() const {
	return draw_border_;
}

void TextButton::SetTextSize(const V2_float& text_size) {
	text_size_ = text_size;
}

V2_float TextButton::GetTextSize() const {
	return text_size_;
}

void TextButton::SetText(const Text& text) {
	PTGN_ASSERT(text.IsValid(), "Cannot set text button to invalid text");
	text_ = text;
}

const Text& TextButton::GetText() const {
	return text_;
}

void TextButton::SetTextAlignment(const TextAlignment& text_alignment) {
	text_alignment_ = text_alignment;
}

const TextAlignment& TextButton::GetTextAlignment() const {
	return text_alignment_;
}

void TextButton::Draw() const {
	DrawFilled();
}

void TextButton::DrawHollow(float line_width) const {
	if (draw_border_) {
		ColorButton::DrawHollow(line_width);
	}
	V2_float size{ rect_.size };
	if (!NearlyEqual(text_size_.x, 0.0f)) {
		size.x = text_size_.x;
	}
	if (!NearlyEqual(text_size_.y, 0.0f)) {
		size.y = text_size_.y;
	}
	text_.Draw({ rect_.Center(), size, text_alignment_ }, 1.0f);
}

void TextButton::DrawFilled() const {
	if (draw_border_) {
		ColorButton::DrawFilled();
	}
	V2_float size{ rect_.size };
	if (!NearlyEqual(text_size_.x, 0.0f)) {
		size.x = text_size_.x;
	}
	if (!NearlyEqual(text_size_.y, 0.0f)) {
		size.y = text_size_.y;
	}
	text_.Draw({ rect_.Center(), size, text_alignment_ }, 1.0f);
}

ToggleButton::ToggleButton(
	const Rectangle<float>& rect, const ButtonActivateFunction& on_activate_function,
	bool initially_toggled
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
}

void ToggleButton::Toggle() {
	Activate();
	toggled_ = !toggled_;
}

ColorButton::ColorButton(
	const Rectangle<float>& rect, const Color& default_color, const Color& hover_color,
	const Color& pressed_color, const ButtonActivateFunction& on_activate_function
) :
	Button{ rect, on_activate_function } {
	SetColor(default_color);
	SetHoverColor(hover_color);
	SetPressedColor(pressed_color);
}

void ColorButton::SetColor(const Color& default_color) {
	colors_.data.at(static_cast<std::size_t>(ButtonState::Default)).at(0) = default_color;
}

void ColorButton::SetHoverColor(const Color& hover_color) {
	colors_.data.at(static_cast<std::size_t>(ButtonState::Hover)).at(0) = hover_color;
}

void ColorButton::SetPressedColor(const Color& pressed_color) {
	colors_.data.at(static_cast<std::size_t>(ButtonState::Pressed)).at(0) = pressed_color;
}

const Color& ColorButton::GetColor() const {
	return colors_.data.at(static_cast<std::size_t>(ButtonState::Default)).at(0);
}

const Color& ColorButton::GetHoverColor() const {
	return colors_.data.at(static_cast<std::size_t>(ButtonState::Hover)).at(0);
}

const Color& ColorButton::GetPressedColor() const {
	return colors_.data.at(static_cast<std::size_t>(ButtonState::Pressed)).at(0);
}

void ColorButton::Draw() const {
	DrawFilled();
}

void ColorButton::DrawHollow(float line_width) const {
	const Color& color = GetCurrentColorImpl(GetState(), 0);
	game.renderer.DrawRectangleHollow(rect_, color, line_width);
}

void ColorButton::DrawFilled() const {
	const Color& color = GetCurrentColorImpl(GetState(), 0);
	game.renderer.DrawRectangleFilled(rect_, color);
}

const Color& ColorButton::GetCurrentColorImpl(ButtonState state, std::size_t color_array_index)
	const {
	auto& color_array  = colors_.data.at(static_cast<std::size_t>(state));
	const Color& color = color_array.at(color_array_index);
	return color;
}

const Color& ColorButton::GetCurrentColor() const {
	return GetCurrentColorImpl(GetState(), 0);
}

TexturedButton::TexturedButton(
	const Rectangle<float>& rect, const TextureOrKey& default_texture,
	const TextureOrKey& hover_texture, const TextureOrKey& pressed_texture,
	const ButtonActivateFunction& on_activate_function
) :
	Button{ rect, on_activate_function } {
	textures_.data.at(static_cast<std::size_t>(ButtonState::Default)).at(0) = default_texture;
	textures_.data.at(static_cast<std::size_t>(ButtonState::Hover)).at(0)	= hover_texture;
	textures_.data.at(static_cast<std::size_t>(ButtonState::Pressed)).at(0) = pressed_texture;
}

bool TexturedButton::GetVisibility() const {
	return !hidden_;
}

void TexturedButton::ForEachTexture(const std::function<void(Texture)>& func) const {
	for (std::size_t state = 0; state < 3; state++) {
		for (std::size_t texture_array_index = 0; texture_array_index < 2; texture_array_index++) {
			Texture texture =
				GetCurrentTextureImpl(static_cast<ButtonState>(state), texture_array_index);

			if (texture.IsValid()) {
				std::invoke(func, texture);
				continue;
			}

			texture = GetCurrentTextureImpl(ButtonState::Default, texture_array_index);

			if (texture.IsValid()) {
				std::invoke(func, texture);
				continue;
			}

			texture = GetCurrentTextureImpl(ButtonState::Default, 0);

			if (texture.IsValid()) {
				std::invoke(func, texture);
				continue;
			}
		}
	}
}

void TexturedButton::SetTintColor(const Color& color) {
	tint_color_ = color;
}

Color TexturedButton::GetTintColor() const {
	return tint_color_;
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
	game.renderer.DrawTexture(
		texture, rect_.pos, rect_.size, {}, {}, rect_.origin, Flip::None, 0.0f, {}, 0.0f,
		tint_color_
	);
}

void TexturedButton::Draw() const {
	DrawImpl(0);
}

Texture TexturedButton::GetCurrentTextureImpl(ButtonState state, std::size_t texture_array_index)
	const {
	auto& texture_array = textures_.data.at(static_cast<std::size_t>(state));

	const TextureOrKey& texture_state = texture_array.at(texture_array_index);

	Texture texture;

	if (std::holds_alternative<impl::TextureManager::Key>(texture_state)) {
		const auto& key{ std::get<impl::TextureManager::Key>(texture_state) };
		PTGN_ASSERT(game.texture.Has(key), "Cannot get button texture which has not been loaded");
		texture = game.texture.Get(key);
	} else if (std::holds_alternative<impl::TextureManager::InternalKey>(texture_state)) {
		const auto& key{ std::get<impl::TextureManager::InternalKey>(texture_state) };
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
	const Rectangle<float>& rect, const std::vector<TextureOrKey>& default_textures,
	const std::vector<TextureOrKey>& hover_textures,
	const std::vector<TextureOrKey>& pressed_textures,
	const ButtonActivateFunction& on_activate_function
) {
	rect_		 = rect;
	on_activate_ = on_activate_function;
	SubscribeToMouseEvents();

	// TODO: Perhaps allow for more than two entries later
	PTGN_ASSERT(default_textures.size() <= 2);
	PTGN_ASSERT(hover_textures.size() <= 2);
	PTGN_ASSERT(pressed_textures.size() <= 2);

	auto set_textures = [&](const auto& list, const ButtonState state) {
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
