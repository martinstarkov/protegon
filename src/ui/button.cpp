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
#include "scene/camera.h"

namespace ptgn {

Button CreateButton(Manager& manager, const ButtonCallback& on_activate) {
	Button button{ manager.CreateEntity() };

	impl::SetupButton(button);
	impl::SetupButtonCallbacks(button, nullptr);

	if (on_activate) {
		button.OnActivate(on_activate);
	}

	return button;
}

Button CreateTextButton(
	Manager& manager, const TextContent& text_content, const TextColor& text_color,
	const ButtonCallback& on_activate
) {
	Button text_button{ CreateButton(manager, on_activate) };

	text_button.SetText(text_content, text_color);

	return text_button;
}

ToggleButton CreateToggleButton(Manager& manager, bool toggled, const ButtonCallback& on_activate) {
	ToggleButton toggle_button{ manager.CreateEntity() };

	impl::SetupButton(toggle_button);
	impl::SetupButtonCallbacks(toggle_button, [e = toggle_button]() mutable { e.Toggle(); });
	toggle_button.Add<impl::ButtonToggled>(toggled);

	if (on_activate) {
		toggle_button.OnActivate(on_activate);
	}

	return toggle_button;
}

namespace impl {

void SetupButton(Button& button) {
	button.Show();
	button.Enable();
	button.SetDraw<Button>();

	button.Add<Interactive>();
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);
}

void SetupButtonCallbacks(Button& button, const std::function<void()>& internal_on_activate) {
	button.Add<callback::MouseEnter>([e = button]([[maybe_unused]] auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::IdleUp) {
			e.StateChange(impl::InternalButtonState::Hover);
			e.StartHover();
		} else if (state == impl::InternalButtonState::IdleDown) {
			e.StateChange(impl::InternalButtonState::HoverPressed);
			e.StartHover();
		} else if (state == impl::InternalButtonState::HeldOutside) {
			e.StateChange(impl::InternalButtonState::Pressed);
		}
	});

	button.Add<callback::MouseLeave>([e = button]([[maybe_unused]] auto mouse) mutable {
		const auto& state{ e.Get<impl::InternalButtonState>() };
		if (state == impl::InternalButtonState::Hover) {
			e.StateChange(impl::InternalButtonState::IdleUp);
			e.StopHover();
		} else if (state == impl::InternalButtonState::Pressed) {
			e.StateChange(impl::InternalButtonState::HeldOutside);
			e.StopHover();
		} else if (state == impl::InternalButtonState::HoverPressed) {
			e.StateChange(impl::InternalButtonState::IdleDown);
			e.StopHover();
		}
	});

	button.Add<callback::MouseDown>([e = button](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Hover) {
				e.StateChange(impl::InternalButtonState::Pressed);
			}
		}
	});

	button.Add<callback::MouseDownOutside>([e = button](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleUp) {
				e.StateChange(impl::InternalButtonState::IdleDown);
			}
		}
	});

	button.Add<callback::MouseUp>([internal_on_activate, e = button](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::Pressed) {
				e.StateChange(impl::InternalButtonState::Hover);
				Invoke(internal_on_activate);
				e.Activate();
			} else if (state == impl::InternalButtonState::HoverPressed) {
				e.StateChange(impl::InternalButtonState::Hover);
			}
		}
	});

	button.Add<callback::MouseUpOutside>([e = button](auto mouse) mutable {
		if (mouse == Mouse::Left) {
			const auto& state{ e.Get<impl::InternalButtonState>() };
			if (state == impl::InternalButtonState::IdleDown) {
				e.StateChange(impl::InternalButtonState::IdleUp);
			} else if (state == impl::InternalButtonState::HeldOutside) {
				e.StateChange(impl::InternalButtonState::IdleUp);
			}
		}
	});
}

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
		text = CreateText(manager, text_content, text_color, font_key);
		text.Hide();
		text.SetParent(parent);
	} else {
		text.SetParameter(text_color, false);
		text.SetParameter(text_content, false);
		text.SetParameter(font_key, false);
		text.RecreateTexture();
	}
}

const TextureHandle& ButtonTexture::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

TextureHandle& ButtonTexture::Get(ButtonState state) {
	return const_cast<TextureHandle&>(std::as_const(*this).Get(state));
}

} // namespace impl

Button::Button(const Entity& entity) : Entity{ entity } {}

void Button::Draw(impl::RenderData& ctx, const Entity& entity) {
	auto state{ Button{ entity }.GetState() };

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
	if (entity.Has<TextureHandle>()) {
		auto& key{ entity.Get<TextureHandle>() };
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

	Origin origin{ entity.GetOrigin() };
	V2_float size;

	if (entity.Has<impl::ButtonSize>()) {
		size = entity.Get<impl::ButtonSize>();
	} else if (entity.Has<impl::ButtonRadius>()) {
		size = V2_float{ entity.Get<impl::ButtonRadius>() * 2.0f };
	}

	TextureHandle button_texture_key;
	if (entity.Has<TextureHandle>()) {
		button_texture_key = entity.Get<TextureHandle>();
	}

	const impl::Texture* button_texture{ nullptr };

	if (game.texture.Has(button_texture_key)) {
		button_texture = &game.texture.Get(button_texture_key);
	}

	if (button_texture != nullptr && size.IsZero()) {
		size = button_texture->GetSize();
	}

	PTGN_ASSERT(!size.IsZero(), "Invalid size for button");

	auto camera{ entity.GetOrDefault<Camera>() };

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
			impl::GetVertices(transform, size, origin),
			Sprite{ entity }.GetTextureCoordinates(false), *button_texture, depth, camera, blend_mode,
			final_tint_n, false
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
					transform.position, size * Abs(transform.scale), origin, background_line_width,
					depth, camera, blend_mode, background_color_n, transform.rotation, false
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
		V4_float border_color_n{ border_color.Normalized() * tint };
		if (border_color_n != V4_float{}) {
			// TODO: Readd rounded buttons.
			/*if (i.radius_ > 0.0f) {
				RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
								i.rect_.rotation };
				r.Draw(border_color, border_width, i.render_layer_ + 2);
			} else {*/
			ctx.AddQuad(
				transform.position, size, origin, border_width, depth, camera, blend_mode, border_color_n,
				transform.rotation, false
			);
		}
	}

	if (text != nullptr && *text != Text{}) {
		auto text_transform{ text->GetAbsoluteTransform() };

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
		text_size *= Abs(text_transform.scale);

		Sprite text_sprite{ *text };

		// Offset by button size so that text is initially centered on button center.
		text_transform.position += GetOriginOffset(origin, size * Abs(text_transform.scale));

		if (text_sprite.Has<TextColor>() && text_sprite.Get<TextColor>().a == 0) {
			return;
		}

		if (!text_sprite.Has<TextContent>()) {
			return;
		}

		if (std::string_view{ text_sprite.Get<TextContent>() }.empty()) {
			return;
		}

		const auto& text_texture{ text_sprite.GetTexture() };

		if (!text_texture.IsValid()) {
			return;
		}

		auto text_depth{ text->GetDepth() };
		auto text_blend_mode{ text->GetBlendMode() };
		auto text_tint{ text->GetTint().Normalized() };
		auto text_origin{ text->GetOrigin() };
		Camera* text_camera{ nullptr };
		if (text->Has<Camera>()) {
			text_camera = &text->Get<Camera>();
		}
		else {
			text_camera = &camera;
		}

		auto text_display_size{ text_sprite.GetDisplaySize() };
		auto text_coords{ text_sprite.GetTextureCoordinates(false) };
		auto text_vertices{ impl::GetVertices(text_transform, text_display_size, text_origin) };

		ctx.AddTexturedQuad(
			text_vertices, text_coords, text_texture, text_depth, *text_camera, text_blend_mode, text_tint * tint,
			false
		);
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

Button& Button::SetRadius(float radius) {
	Remove<impl::ButtonSize>();
	if (Has<impl::ButtonRadius>()) {
		Get<impl::ButtonRadius>() = radius;
	} else {
		Add<impl::ButtonRadius>(radius);
	}
	return *this;
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
	const TextContent& content, const TextColor& text_color, const FontKey& font_key,
	ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(*this, GetManager(), state, content, text_color, font_key);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(*this, GetManager(), state, content, text_color, font_key);
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

Button& Button::SetTextColor(const TextColor& text_color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(*this, GetManager(), state, TextContent{}, text_color, FontKey{});
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(text_color);
	}
	return *this;
}

std::string_view Button::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextContent(state);
}

Button& Button::SetTextContent(const TextContent& content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(*this, GetManager(), state, content, TextColor{}, FontKey{});
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
		auto& c{
			Add<impl::ButtonText>(*this, GetManager(), state, TextContent{}, TextColor{}, FontKey{})
		};
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
	// TODO: Fix this by using DisplaySize or scale component.
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
		auto& c{
			Add<impl::ButtonText>(*this, GetManager(), state, TextContent{}, TextColor{}, FontKey{})
		};
		c.Get(state).SetFontSize(font_size);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetFontSize(font_size);
	}
	return *this;
}

const TextureHandle& Button::GetTextureKey(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureHandle>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureHandle>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return Get<impl::ButtonTexture>().Get(state);
}

Button& Button::SetTextureKey(const TextureHandle& texture_key, ButtonState state) {
	if (!Has<TextureHandle>()) {
		Add<TextureHandle>(texture_key);
	} else if (state == ButtonState::Current) {
		Add<TextureHandle>(texture_key);
		return *this;
	}
	if (!Has<impl::ButtonTexture>()) {
		Add<impl::ButtonTexture>(texture_key);
	} else {
		auto& c{ Get<impl::ButtonTexture>() };
		c.Get(state) = texture_key;
	}
	return *this;
}

Button& Button::SetDisabledTextureKey(const TextureHandle& texture_key) {
	if (!texture_key) {
		Remove<impl::ButtonDisabledTextureKey>();
	} else {
		Add<impl::ButtonDisabledTextureKey>(texture_key);
	}
	return *this;
}

const TextureHandle& Button::GetDisabledTextureKey() const {
	PTGN_ASSERT(
		Has<impl::ButtonDisabledTextureKey>(),
		"Cannot retrieve disabled texture key as it has not been set for the button"
	);
	return Get<impl::ButtonDisabledTextureKey>();
}

Color Button::GetButtonTint(ButtonState state) const {
	const auto c{ Has<impl::ButtonTint>() ? Get<impl::ButtonTint>() : impl::ButtonTint{} };
	return c.Get(state);
}

Button& Button::SetButtonTint(const Color& color, ButtonState state) {
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

ButtonState Button::GetState() const {
	PTGN_ASSERT(Has<impl::InternalButtonState>());
	const auto& state{ Get<impl::InternalButtonState>() };
	if (state == impl::InternalButtonState::Hover ||
		state == impl::InternalButtonState::HoverPressed) {
		return ButtonState::Hover;
	} else if (state == impl::InternalButtonState::Pressed || state == impl::InternalButtonState::HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

void Button::StateChange(impl::InternalButtonState new_state) {
	Get<impl::InternalButtonState>() = new_state;
}

void Button::Activate() {
	// TODO: Replace with Invoke<Component>(). And do the same for the button other callbacks.
	if (Has<impl::InternalButtonActivate>()) {
		if (const auto& callback{ Get<impl::InternalButtonActivate>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
	if (Has<impl::ButtonActivate>()) {
		if (const auto& callback{ Get<impl::ButtonActivate>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

void Button::StartHover() {
	if (Has<impl::ButtonHoverStart>()) {
		if (const auto& callback{ Get<impl::ButtonHoverStart>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

void Button::StopHover() {
	if (Has<impl::ButtonHoverStop>()) {
		if (const auto& callback{ Get<impl::ButtonHoverStop>() }; callback != nullptr) {
			std::invoke(callback);
		}
	}
}

void ToggleButton::Activate() {
	Toggle();
	Button::Activate();
}

bool ToggleButton::IsToggled() const {
	return Get<impl::ButtonToggled>();
}

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	auto& t{ Get<impl::ButtonToggled>() };
	t = toggled;
	return *this;
}

ToggleButton& ToggleButton::Toggle() {
	auto& toggled{ Get<impl::ButtonToggled>() };
	toggled = !toggled;
	return *this;
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

ToggleButton& ToggleButton::SetTextColorToggled(const TextColor& text_color, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetManager(), state, TextContent{}, text_color, FontKey{}
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(text_color);
	}
	return *this;
}

std::string_view ToggleButton::GetTextContentToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextContent(state);
}

ToggleButton& ToggleButton::SetTextContentToggled(const TextContent& content, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(*this, GetManager(), state, content, TextColor{}, FontKey{});
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

ToggleButton& ToggleButton::SetTextToggled(
	const TextContent& content, const TextColor& text_color, const FontKey& font_key,
	ButtonState state
) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(*this, GetManager(), state, content, text_color, font_key);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(*this, GetManager(), state, content, text_color, font_key);
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

const TextureHandle& ToggleButton::GetTextureKeyToggled(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<TextureHandle>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<TextureHandle>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return Get<impl::ButtonTextureToggled>().Get(state);
}

ToggleButton& ToggleButton::SetTextureKeyToggled(
	const TextureHandle& texture_key, ButtonState state
) {
	if (!Has<TextureHandle>()) {
		Add<TextureHandle>(texture_key);
	} else if (state == ButtonState::Current && Get<impl::ButtonToggled>()) {
		Add<TextureHandle>(texture_key);
		return *this;
	}
	if (!Has<impl::ButtonTextureToggled>()) {
		Add<impl::ButtonTextureToggled>(texture_key);
	} else {
		auto& c{ Get<impl::ButtonTextureToggled>() };
		c.Get(state) = texture_key;
	}
	return *this;
}

Color ToggleButton::GetButtonTintToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonTintToggled>() ? Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetButtonTintToggled(const Color& color, ButtonState state) {
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