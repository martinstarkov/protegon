#include "ui/button.h"

#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/function.h"
#include "components/generic.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "debug/log.h"
#include "events/mouse.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/resources/text.h"
#include "rendering/resources/texture.h"

namespace ptgn {

namespace impl {

void ButtonColor::SetToState(ButtonState state) {
	current_ = Get(state);
}

const Color& ButtonColor::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Current: return current_;
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		default:				   PTGN_ERROR("Invalid button state");
	}
}

Color& ButtonColor::Get(ButtonState state) {
	return const_cast<Color&>(std::as_const(*this).Get(state));
}

ButtonText::ButtonText(
	Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontKey& font_key
) {
	Set(parent, manager, ButtonState::Default, text_content, text_color, font_key);
	if (state != ButtonState::Default) {
		Set(parent, manager, state, text_content, text_color, font_key);
	}
}

const Text& ButtonText::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

const Text& ButtonText::GetValid(ButtonState state) const {
	if (state == ButtonState::Current) {
		return Get(ButtonState::Default);
	}
	const Text& text{ Get(state) };
	if (text == Text{}) {
		return Get(ButtonState::Default);
	}
	return text;
}

Text& ButtonText::GetValid(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).GetValid(state));
}

Text& ButtonText::Get(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).Get(state));
}

Color ButtonText::GetTextColor(ButtonState state) const {
	return GetValid(state).GetColor();
}

std::string_view ButtonText::GetTextContent(ButtonState state) const {
	return GetValid(state).GetContent();
}

std::int32_t ButtonText::GetFontSize(ButtonState state) const {
	return GetValid(state).GetFontSize();
}

TextJustify ButtonText::GetTextJustify(ButtonState state) const {
	return GetValid(state).GetTextJustify();
}

void ButtonText::Set(
	Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontKey& font_key
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	auto& text{ Get(state) };
	if (text == Text{}) {
		text = Text{ manager, text_content, text_color, font_key };
		text.SetVisible(false);
		text.SetParent(parent);
	} else {
		text.SetParameter(TextColor{ text_color }, false);
		text.SetParameter(TextContent{ text_content }, false);
		text.SetParameter(FontKey{ font_key }, false);
		text.RecreateTexture();
	}
}

const TextureKey& ButtonTexture::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

TextureKey& ButtonTexture::Get(ButtonState state) {
	return const_cast<TextureKey&>(std::as_const(*this).Get(state));
}

} // namespace impl

Button::Button(Manager& manager, bool) : GameObject{ manager } {}

Button::Button(Manager& manager) : Button{ manager, true } {
	Setup();
	SetupCallbacks(nullptr);
}

void Button::Setup() {
	SetVisible(true);
	SetEnabled(true);

	Add<Interactive>();
	Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);
}

void Button::SetupCallbacks(const std::function<void()>& internal_on_activate) {
	Add<callback::MouseEnter>([e = GetEntity()]([[maybe_unused]] auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::IdleUp) {
			StateChange(e, impl::InternalButtonState::Hover);
			StartHover(e);
		} else if (state == impl::InternalButtonState::IdleDown) {
			StateChange(e, impl::InternalButtonState::HoverPressed);
			StartHover(e);
		} else if (state == impl::InternalButtonState::HeldOutside) {
			StateChange(e, impl::InternalButtonState::Pressed);
		}
	});

	Add<callback::MouseLeave>([e = GetEntity()]([[maybe_unused]] auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::Hover) {
			StateChange(e, impl::InternalButtonState::IdleUp);
			StopHover(e);
		} else if (state == impl::InternalButtonState::Pressed) {
			StateChange(e, impl::InternalButtonState::HeldOutside);
			StopHover(e);
		} else if (state == impl::InternalButtonState::HoverPressed) {
			StateChange(e, impl::InternalButtonState::IdleDown);
			StopHover(e);
		}
	});

	Add<callback::MouseDown>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Hover) {
				StateChange(e, impl::InternalButtonState::Pressed);
			}
		}
	});

	Add<callback::MouseDownOutside>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleUp) {
				StateChange(e, impl::InternalButtonState::IdleDown);
			}
		}
	});

	Add<callback::MouseUp>([internal_on_activate, e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Pressed) {
				StateChange(e, impl::InternalButtonState::Hover);
				Invoke(internal_on_activate);
				Activate(e);
			} else if (state == impl::InternalButtonState::HoverPressed) {
				StateChange(e, impl::InternalButtonState::Hover);
			}
		}
	});

	Add<callback::MouseUpOutside>([e = GetEntity()](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleDown) {
				StateChange(e, impl::InternalButtonState::IdleUp);
			} else if (state == impl::InternalButtonState::HeldOutside) {
				StateChange(e, impl::InternalButtonState::IdleUp);
			}
		}
	});
}

void Button::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto state{ Button::GetState(entity) };

	const auto& transform{ entity.GetAbsoluteTransform() };
	auto blend_mode{ entity.GetBlendMode() };
	auto depth{ entity.GetDepth() };
	auto tint{ entity.GetTint().Normalized() };

	if (entity.Has<impl::ButtonColor>()) {
		entity.Get<impl::ButtonColor>().SetToState(state);
	}
	if (entity.Has<impl::ButtonColorToggled>()) {
		entity.Get<impl::ButtonColorToggled>().SetToState(state);
	}
	if (entity.Has<impl::ButtonTint>()) {
		entity.Get<impl::ButtonTint>().SetToState(state);
	}
	if (entity.Has<impl::ButtonTintToggled>()) {
		entity.Get<impl::ButtonTintToggled>().SetToState(state);
	}
	if (entity.Has<impl::ButtonBorderColor>()) {
		entity.Get<impl::ButtonBorderColor>().SetToState(state);
	}
	if (entity.Has<impl::ButtonBorderColorToggled>()) {
		entity.Get<impl::ButtonBorderColorToggled>().SetToState(state);
	}
	if (entity.Has<TextureKey>()) {
		auto& key{ entity.Get<TextureKey>() };
		if (!entity.IsEnabled() && entity.Has<impl::ButtonDisabledTextureKey>()) {
			key = entity.Get<impl::ButtonDisabledTextureKey>();
		} else if (entity.Has<impl::ButtonToggled>() && entity.Has<impl::ButtonTextureToggled>()) {
			key = entity.Get<impl::ButtonTextureToggled>().Get(state);
		} else if (entity.Has<impl::ButtonTexture>()) {
			key = entity.Get<impl::ButtonTexture>().Get(state);
		}
	}

	// TODO: Move this all to a separate functions.
	// TODO: Reduce repeated code.

	Origin origin{ Origin::Center };
	V2_float size;

	if (entity.Has<impl::ButtonSize>()) {
		size = entity.Get<impl::ButtonSize>();
	} else if (entity.Has<impl::ButtonRadius>()) {
		size = V2_float{ entity.Get<impl::ButtonRadius>() * 2.0f };
	}

	if (entity.Has<impl::ButtonOrigin>()) {
		origin = entity.Get<impl::ButtonOrigin>();
	}

	TextureKey button_texture_key;
	if (entity.Has<TextureKey>()) {
		button_texture_key = entity.Get<TextureKey>();
	}

	const impl::Texture* button_texture{ nullptr };

	if (game.texture.Has(button_texture_key)) {
		button_texture = &game.texture.Get(button_texture_key);
	}

	if (button_texture != nullptr && size.IsZero()) {
		size = button_texture->GetSize();
	}

	PTGN_ASSERT(!size.IsZero(), "Invalid size for button");

	if (button_texture != nullptr && *button_texture != impl::Texture{}) {
		Color button_tint{ color::White };
		if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
			entity.Has<impl::ButtonTintToggled>()) {
			button_tint = entity.Get<impl::ButtonTintToggled>().current_;
		} else if (entity.Has<impl::ButtonTint>()) {
			button_tint = entity.Get<impl::ButtonTint>().current_;
		}
		V4_float final_tint_n{ button_tint.Normalized() * tint };

		ctx.AddTexturedQuad(
			impl::GetVertices(transform, size, origin), entity.GetTextureCoordinates(false),
			*button_texture, depth, blend_mode, final_tint_n, false
		);
	} else {
		impl::ButtonBackgroundWidth background_line_width;
		if (entity.Has<impl::ButtonBackgroundWidth>()) {
			background_line_width = entity.Get<impl::ButtonBackgroundWidth>();
		}
		if (background_line_width != 0.0f) {
			Color button_color;
			if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
				entity.Has<impl::ButtonColorToggled>()) {
				button_color = entity.Get<impl::ButtonColorToggled>().current_;
			} else if (entity.Has<impl::ButtonColor>()) {
				button_color = entity.Get<impl::ButtonColor>().current_;
			}
			V4_float background_color_n{ button_color.Normalized() };
			if (background_color_n != V4_float{}) {
				// TODO: Add rounded buttons.
				/*if (radius_ > 0.0f) {
					RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
									i.rect_.rotation };
					r.Draw(bg, i.line_thickness_, i.render_layer_);
				} else {*/
				ctx.AddQuad(
					transform.position, size * transform.scale, origin, background_line_width,
					depth, blend_mode, background_color_n, transform.rotation, false
				);
			}
		}
	}

	const Text* text{ nullptr };
	if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
		entity.Has<impl::ButtonTextToggled>()) {
		const auto& button_text_toggled{ entity.Get<impl::ButtonTextToggled>() };
		text = &button_text_toggled.GetValid(state);
	} else if (entity.Has<impl::ButtonText>()) {
		const auto& button_text{ entity.Get<impl::ButtonText>() };
		text = &button_text.GetValid(state);
	}
	if (text != nullptr && *text != Text{}) {
		/*
		// TODO: Fix ButtonTextFixedSize
		V2_float text_size;
		if (entity.Has<impl::ButtonTextFixedSize>()) {
			text_size = entity.Get<impl::ButtonTextFixedSize>();
		} else {
			text_size = text->GetSize();
		}
		if (NearlyEqual(text_size.x, 0.0f)) {
			text_size.x = size.x;
		}
		if (NearlyEqual(text_size.y, 0.0f)) {
			text_size.y = size.y;
		}
		text_size *= text_transform.scale;
		*/

		// TODO: Fix this centering.
		// Offset by button size so that text is initially centered on button center.
		// text_transform.position += GetOriginOffset(origin, size * text_transform.scale);

		// TODO: Fix text tinting: text->GetTint().Normalized() * tint
		Text::Draw(ctx, *text);
	}

	impl::ButtonBorderWidth border_width;
	if (entity.Has<impl::ButtonBorderWidth>()) {
		border_width = entity.Get<impl::ButtonBorderWidth>();
	}
	if (border_width != 0.0f) {
		Color border_color;
		if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
			entity.Has<impl::ButtonBorderColorToggled>()) {
			border_color = entity.Get<impl::ButtonBorderColorToggled>().current_;
		} else if (entity.Has<impl::ButtonBorderColor>()) {
			border_color = entity.Get<impl::ButtonBorderColor>().current_;
		}
		V4_float border_color_n{ border_color.Normalized() };
		if (border_color_n != V4_float{}) {
			// TODO: Readd rounded buttons.
			/*if (i.radius_ > 0.0f) {
				RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
								i.rect_.rotation };
				r.Draw(border_color, border_width, i.render_layer_ + 2);
			} else {*/
			ctx.AddQuad(
				transform.position, size, origin, border_width, depth, blend_mode, border_color_n,
				transform.rotation, false
			);
		}
	}
}

Button& Button::AddInteractableRect(const V2_float& size, Origin origin, const V2_float& offset) {
	if (Has<InteractiveRects>()) {
		auto& interactives{ Get<InteractiveRects>() };
		interactives.rects.push_back({ size, origin, offset });
	} else {
		Add<InteractiveRects>(size, origin, offset);
	}
	return *this;
}

Button& Button::AddInteractableCircle(float radius, const V2_float& offset) {
	if (Has<InteractiveCircles>()) {
		auto& interactives{ Get<InteractiveCircles>() };
		interactives.circles.push_back({ radius, offset });
	} else {
		Add<InteractiveCircles>(radius, offset);
	}
	return *this;
}

Button& Button::SetSize(const V2_float& size) {
	Remove<impl::ButtonRadius>();
	if (Has<impl::ButtonSize>()) {
		Get<impl::ButtonSize>() = size;
	} else {
		Add<impl::ButtonSize>(size);
	}
	return *this;
}

Button& Button::SetOrigin(Origin origin) {
	Remove<impl::ButtonRadius>();
	if (Has<impl::ButtonOrigin>()) {
		Get<impl::ButtonOrigin>() = origin;
	} else {
		Add<impl::ButtonOrigin>(origin);
	}
	return *this;
}

Button& Button::SetRadius(float radius) {
	Remove<impl::ButtonSize>();
	Remove<impl::ButtonOrigin>();
	if (Has<impl::ButtonRadius>()) {
		Get<impl::ButtonRadius>() = radius;
	} else {
		Add<impl::ButtonRadius>(radius);
	}
	return *this;
}

void Button::StateChange(impl::InternalButtonState new_state) {
	StateChange(GetEntity(), new_state);
}

void Button::Activate() {
	Activate(GetEntity());
}

void Button::StartHover() {
	StartHover(GetEntity());
}

void Button::StopHover() {
	StopHover(GetEntity());
}

ButtonState Button::GetState() const {
	return GetState(GetEntity());
}

Color Button::GetBackgroundColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonColor>() ? Get<impl::ButtonColor>() : impl::ButtonColor{} };
	return c.Get(state);
}

Button& Button::SetBackgroundColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColor>()) {
		Add<impl::ButtonColor>(color);
	} else {
		auto& c{ Get<impl::ButtonColor>() };
		c.Get(state) = color;
	}
	return *this;
}

Button& Button::SetText(
	std::string_view content, const Color& text_color, std::string_view font_key, ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	}
	return *this;
}

const Text& Button::GetText(ButtonState state) const {
	return Get<impl::ButtonText>().GetValid(state);
}

Text& Button::GetText(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).GetText(state));
}

Color Button::GetTextColor(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextColor(state);
}

Button& Button::SetTextColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{}, TextColor{ color }, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(color);
	}
	return *this;
}

std::string_view Button::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextContent(state);
}

Button& Button::SetTextContent(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{}, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

TextJustify Button::GetTextJustify(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextJustify(state);
}

Button& Button::SetTextJustify(const TextJustify& justify, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		auto& c{ Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{}, TextColor{}, FontKey{}
		) };
		c.Get(state).SetTextJustify(justify);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetTextJustify(justify);
	}
	return *this;
}

/*
V2_float Button::GetTextFixedSize() const {
	return Has<impl::ButtonTextFixedSize>() ? Get<impl::ButtonTextFixedSize>()
											: impl::ButtonTextFixedSize{};
}

Button& Button::SetTextFixedSize(const V2_float& size) {
	// TODO: Fix this by using DisplaySize component.
	PTGN_ASSERT(size.x >= 0.0f && size.y >= 0.0f, "Invalid button text fixed size");
	Add<impl::ButtonTextFixedSize>(size);
	return *this;
}

Button& Button::ClearTextFixedSize() {
	Remove<impl::ButtonTextFixedSize>();
	return *this;
}
*/

std::int32_t Button::GetFontSize(ButtonState state) const {
	return Get<impl::ButtonText>().GetFontSize(state);
}

Button& Button::SetFontSize(std::int32_t font_size, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		auto& c{ Add<impl::ButtonText>(
			GetEntity(), GetManager(), state, TextContent{}, TextColor{}, FontKey{}
		) };
		c.Get(state).SetFontSize(font_size);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetFontSize(font_size);
	}
	return *this;
}

const TextureKey& Button::GetTextureKey(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureKey>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureKey>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return Get<impl::ButtonTexture>().Get(state);
}

Button& Button::SetTextureKey(std::string_view texture_key, ButtonState state) {
	TextureKey tk{ texture_key };
	if (!Has<TextureKey>()) {
		Add<TextureKey>(tk);
	} else if (state == ButtonState::Current) {
		Add<TextureKey>(tk);
		return *this;
	}
	if (!Has<impl::ButtonTexture>()) {
		Add<impl::ButtonTexture>(tk);
	} else {
		auto& c{ Get<impl::ButtonTexture>() };
		c.Get(state) = tk;
	}
	return *this;
}

Button& Button::SetDisabledTextureKey(std::string_view texture_key) {
	if (texture_key == std::string_view{}) {
		Remove<impl::ButtonDisabledTextureKey>();
	} else {
		Add<impl::ButtonDisabledTextureKey>(texture_key);
	}
	return *this;
}

const TextureKey& Button::GetDisabledTextureKey() const {
	PTGN_ASSERT(
		Has<impl::ButtonDisabledTextureKey>(),
		"Cannot retrieve disabled texture key as it has not been set for the button"
	);
	return Get<impl::ButtonDisabledTextureKey>();
}

Color Button::GetTint(ButtonState state) const {
	const auto c{ Has<impl::ButtonTint>() ? Get<impl::ButtonTint>() : impl::ButtonTint{} };
	return c.Get(state);
}

Button& Button::SetTint(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTint>()) {
		auto& c{ Add<impl::ButtonTint>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTint>() };
		c.Get(state) = color;
	}
	return *this;
}

Color Button::GetBorderColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColor>() ? Get<impl::ButtonBorderColor>()
												 : impl::ButtonBorderColor{} };
	return c.Get(state);
}

Button& Button::SetBorderColor(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonBorderColor>()) {
		Add<impl::ButtonBorderColor>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColor>() };
		c.Get(state) = color;
	}
	return *this;
}

float Button::GetBackgroundLineWidth() const {
	return Has<impl::ButtonBackgroundWidth>() ? Get<impl::ButtonBackgroundWidth>()
											  : impl::ButtonBackgroundWidth{};
}

Button& Button::SetBackgroundLineWidth(float line_width) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Invalid button background line width");
	if (line_width != -1.0f && line_width < 1.0f) {
		Remove<impl::ButtonBackgroundWidth>();
	} else {
		Add<impl::ButtonBackgroundWidth>(line_width);
	}
	return *this;
}

float Button::GetBorderWidth() const {
	return Has<impl::ButtonBorderWidth>() ? Get<impl::ButtonBorderWidth>()
										  : impl::ButtonBorderWidth{};
}

Button& Button::SetBorderWidth(float line_width) {
	PTGN_ASSERT(line_width >= 1.0f || line_width == 0.0f, "Cannot set negative border width");
	if (line_width == 0.0f) {
		Remove<impl::ButtonBorderWidth>();
	} else {
		Add<impl::ButtonBorderWidth>(line_width);
	}
	return *this;
}

Button& Button::OnHoverStart(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonHoverStart>();
	} else {
		Add<impl::ButtonHoverStart>(callback);
	}
	return *this;
}

Button& Button::OnHoverStop(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonHoverStop>();
	} else {
		Add<impl::ButtonHoverStop>(callback);
	}
	return *this;
}

Button& Button::OnActivate(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::ButtonActivate>();
	} else {
		Add<impl::ButtonActivate>(callback);
	}
	return *this;
}

Button& Button::OnInternalActivate(const ButtonCallback& callback) {
	if (callback == nullptr) {
		Remove<impl::InternalButtonActivate>();
	} else {
		Add<impl::InternalButtonActivate>(callback);
	}
	return *this;
}

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

ButtonState Button::GetState(const Entity& e) {
	PTGN_ASSERT(e.Has<impl::InternalButtonState>());
	const auto& state{ e.Get<impl::InternalButtonState>() };
	if (state == impl::InternalButtonState::Hover ||
		state == impl::InternalButtonState::HoverPressed) {
		return ButtonState::Hover;
	} else if (state == impl::InternalButtonState::Pressed || state == impl::InternalButtonState::HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

void Button::StateChange(const Entity& e, impl::InternalButtonState new_state) {
	e.Get<impl::InternalButtonState>() = new_state;
}

void Button::Activate(const Entity& e) {
	// TODO: Replace with Invoke<Component>(). And do the same for the button other callbacks.
	if (e.Has<impl::InternalButtonActivate>()) {
		if (const auto& callback{ e.Get<impl::InternalButtonActivate>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
	if (e.Has<impl::ButtonActivate>()) {
		if (const auto& callback{ e.Get<impl::ButtonActivate>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

void Button::StartHover(const Entity& e) {
	if (e.Has<impl::ButtonHoverStart>()) {
		if (const auto& callback{ e.Get<impl::ButtonHoverStart>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

void Button::StopHover(const Entity& e) {
	if (e.Has<impl::ButtonHoverStop>()) {
		if (const auto& callback{ e.Get<impl::ButtonHoverStop>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

ToggleButton::ToggleButton(Manager& manager, bool toggled) : Button{ manager, true } {
	Button::Setup();
	Add<impl::ButtonToggled>(toggled);
	Button::SetupCallbacks([e = GetEntity()]() { Toggle(e); });
}

void ToggleButton::Activate() {
	Toggle();
	Button::Activate();
}

bool ToggleButton::IsToggled() const {
	return Get<impl::ButtonToggled>();
}

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	SetToggled(GetEntity(), toggled);
	return *this;
}

void ToggleButton::SetToggled(Entity e, bool toggled) {
	auto& t{ e.Get<impl::ButtonToggled>() };
	t = toggled;
}

ToggleButton& ToggleButton::Toggle() {
	Toggle(GetEntity());
	return *this;
}

void ToggleButton::Toggle(Entity e) {
	auto& toggled{ e.Get<impl::ButtonToggled>() };
	toggled = !toggled;
}

Color ToggleButton::GetBackgroundColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonColorToggled>() ? Get<impl::ButtonColorToggled>()
												  : impl::ButtonColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetBackgroundColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonColorToggled>()) {
		Add<impl::ButtonColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonColorToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

Color ToggleButton::GetTextColorToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextColor(state);
}

ToggleButton& ToggleButton::SetTextColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			GetEntity(), GetManager(), state, TextContent{}, TextColor{ color }, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(color);
	}
	return *this;
}

std::string_view ToggleButton::GetTextContentToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextContent(state);
}

ToggleButton& ToggleButton::SetTextContentToggled(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{}, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

ToggleButton& ToggleButton::SetTextToggled(
	std::string_view content, const Color& text_color, std::string_view font_key, ButtonState state
) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(
			GetEntity(), GetManager(), state, TextContent{ content }, TextColor{ text_color },
			FontKey{ font_key }
		);
	}
	return *this;
}

const Text& ToggleButton::GetTextToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetValid(state);
}

Text& ToggleButton::GetTextToggled(ButtonState state) {
	return const_cast<Text&>(std::as_const(*this).GetTextToggled(state));
}

Color ToggleButton::GetBorderColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColorToggled>() ? Get<impl::ButtonBorderColorToggled>()
														: impl::ButtonBorderColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetBorderColorToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonBorderColorToggled>()) {
		Add<impl::ButtonBorderColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColorToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

const TextureKey& ToggleButton::GetTextureKeyToggled(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureKey>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureKey>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return Get<impl::ButtonTextureToggled>().Get(state);
}

ToggleButton& ToggleButton::SetTextureKeyToggled(std::string_view texture_key, ButtonState state) {
	TextureKey tk{ texture_key };
	if (!Has<TextureKey>()) {
		Add<TextureKey>(tk);
	} else if (state == ButtonState::Current && Get<impl::ButtonToggled>()) {
		Add<TextureKey>(tk);
		return *this;
	}
	if (!Has<impl::ButtonTextureToggled>()) {
		Add<impl::ButtonTextureToggled>(tk);
	} else {
		auto& c{ Get<impl::ButtonTextureToggled>() };
		c.Get(state) = tk;
	}
	return *this;
}

Color ToggleButton::GetTintToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonTintToggled>() ? Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetTintToggled(const Color& color, ButtonState state) {
	if (!Has<impl::ButtonTintToggled>()) {
		auto& c{ Add<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

} // namespace ptgn