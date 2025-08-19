#include "ui/button.h"

#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/generic.h"
#include "components/interactive.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/script.h"
#include "debug/log.h"
#include "input/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/hash.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "resources/resource_manager.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

Button CreateButton(Scene& scene) {
	Button button{ scene.CreateEntity() };

	Show(button);
	SetDraw<Button>(button);

	SetInteractive(button);
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

Button CreateTextButton(
	Scene& scene, const TextContent& text_content, const TextColor& text_color
) {
	Button text_button{ CreateButton(scene) };

	text_button.SetText(text_content, text_color);

	return text_button;
}

ToggleButton CreateToggleButton(Scene& scene, bool toggled) {
	ToggleButton toggle_button{ CreateButton(scene) };

	AddScript<impl::ToggleButtonScript>(toggle_button);
	toggle_button.Add<impl::ButtonToggled>(toggled);

	return toggle_button;
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup toggle_button_group{ scene.CreateEntity() };

	toggle_button_group.Add<impl::ToggleButtonGroupInfo>();

	return toggle_button_group;
}

namespace impl {

void InternalButtonScript::OnMouseMoveOver() {
	auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	if (state == InternalButtonState::IdleUp) {
		state = InternalButtonState::Hover;
		button.StartHover();
	} else if (state == InternalButtonState::IdleDown) {
		state = InternalButtonState::HoverPressed;
		button.StartHover();
	} else if (state == InternalButtonState::HeldOutside) {
		state = InternalButtonState::Pressed;
	}
}

void InternalButtonScript::OnMouseMoveOut() {
	auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	if (state == InternalButtonState::Hover) {
		state = InternalButtonState::IdleUp;
		button.StopHover();
	} else if (state == InternalButtonState::Pressed) {
		state = InternalButtonState::HeldOutside;
		button.StopHover();
	} else if (state == InternalButtonState::HoverPressed) {
		state = InternalButtonState::IdleDown;
		button.StopHover();
	}
}

void InternalButtonScript::OnMouseDownOver(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::Hover) {
			state = InternalButtonState::Pressed;
		}
	}
}

void InternalButtonScript::OnMouseDownOut(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::IdleUp) {
			state = InternalButtonState::IdleDown;
		}
	}
}

void InternalButtonScript::OnMouseUpOver(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::Pressed) {
			state = InternalButtonState::Hover;
			Button{ entity }.Activate();
		} else if (state == InternalButtonState::HoverPressed) {
			state = InternalButtonState::Hover;
		}
	}
}

void InternalButtonScript::OnMouseUpOut(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == InternalButtonState::IdleDown) {
			state = InternalButtonState::IdleUp;
		} else if (state == InternalButtonState::HeldOutside) {
			state = InternalButtonState::IdleUp;
		}
	}
}

void ToggleButtonScript::OnButtonActivate() {
	ToggleButton self{ entity };
	if (!self.IsEnabled(false)) {
		return;
	}
	self.Toggle();
}

ToggleButtonGroupScript::ToggleButtonGroupScript(const ToggleButtonGroup& group) :
	toggle_button_group{ group } {}

void ToggleButtonGroupScript::OnButtonActivate() {
	ToggleButton self{ entity };
	if (!self.IsEnabled(false)) {
		return;
	}

	PTGN_ASSERT(toggle_button_group);

	PTGN_ASSERT(toggle_button_group.Has<impl::ToggleButtonGroupInfo>());

	auto& info{ toggle_button_group.Get<impl::ToggleButtonGroupInfo>() };

	for (auto& [key, toggle_button] : info.buttons_) {
		toggle_button.SetToggled(false);
	}
	self.SetToggled(true);
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
	Entity parent, Scene& scene, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& text_properties
) {
	Set(parent, scene, ButtonState::Default, text_content, text_color, font_size, font_key,
		text_properties);
	if (state != ButtonState::Default) {
		Set(parent, scene, state, text_content, text_color, font_size, font_key, text_properties);
	}
}

ButtonText::~ButtonText() {
	// TODO: Fix. These are an issue when a scene is deleted which destroys pools which destroys
	// these.
	// if (default_) {
	//	default_.Destroy();
	//}
	// if (hover_) {
	//	hover_.Destroy();
	//}
	// if (pressed_) {
	//	pressed_.Destroy();
	//}
}

Text ButtonText::Get(ButtonState state) const {
	switch (state) {
		case ButtonState::Default: return default_;
		case ButtonState::Hover:   return hover_;
		case ButtonState::Pressed: return pressed_;
		case ButtonState::Current: [[fallthrough]];
		default:				   PTGN_ERROR("Invalid button state");
	}
}

Text ButtonText::GetValid(ButtonState state) const {
	if (state == ButtonState::Current) {
		return Get(ButtonState::Default);
	}
	Text text{ Get(state) };
	if (text == Text{}) {
		return Get(ButtonState::Default);
	}
	return text;
}

TextColor ButtonText::GetTextColor(ButtonState state) const {
	return GetValid(state).GetColor();
}

TextContent ButtonText::GetTextContent(ButtonState state) const {
	return GetValid(state).GetContent();
}

FontSize ButtonText::GetFontSize(ButtonState state) const {
	return GetValid(state).GetFontSize();
}

TextJustify ButtonText::GetTextJustify(ButtonState state) const {
	return GetValid(state).GetTextJustify();
}

void ButtonText::Set(
	Entity parent, Scene& scene, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& text_properties
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	Text text{ Get(state) };
	if (text == Text{}) {
		text = CreateText(scene, text_content, text_color, font_size, font_key, text_properties);
		Hide(text);
		SetParent(text, parent);
		switch (state) {
			case ButtonState::Default:
				PTGN_ASSERT(!default_);
				default_ = text;
				break;
			case ButtonState::Hover:
				PTGN_ASSERT(!hover_);
				hover_ = text;
				break;
			case ButtonState::Pressed:
				PTGN_ASSERT(!pressed_);
				pressed_ = text;
				break;
			case ButtonState::Current: [[fallthrough]];
			default:				   PTGN_ERROR("Invalid button state");
		}
	} else {
		text.SetParameter(text_color, false);
		text.SetParameter(text_content, false);
		text.SetParameter(font_key, false);
		text.SetParameter(font_size, false);
		text.SetProperties(text_properties, false);
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
	Button button{ entity };
	auto state{ button.GetState() };

	const auto& transform{ GetDrawTransform(entity) };
	auto blend_mode{ GetBlendMode(entity) };
	auto depth{ GetDepth(entity) };
	auto tint{ GetTint(entity).Normalized() };

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
		if (!button.IsEnabled(false) && entity.Has<impl::ButtonDisabledTextureKey>()) {
			key = entity.Get<impl::ButtonDisabledTextureKey>();
		} else if (entity.Has<impl::ButtonToggled>() && entity.Has<impl::ButtonTextureToggled>()) {
			key = entity.Get<impl::ButtonTextureToggled>().Get(state);
		} else if (entity.Has<impl::ButtonTexture>()) {
			key = entity.Get<impl::ButtonTexture>().Get(state);
		}
	}

	// TODO: Move this all to a separate functions.
	// TODO: Reduce repeated code.

	Origin origin{ GetDrawOrigin(entity) };

	TextureHandle button_texture_key;
	if (entity.Has<TextureHandle>()) {
		button_texture_key = entity.Get<TextureHandle>();
	}

	const impl::Texture* button_texture{ nullptr };

	if (game.texture.Has(button_texture_key)) {
		button_texture = &button_texture_key.GetTexture();
	}

	auto size{ button.GetSize() };

	auto camera{ entity.GetOrParentOrDefault<Camera>() };

	impl::RenderState render_state;
	render_state.blend_mode	 = blend_mode;
	render_state.camera		 = camera;
	render_state.post_fx	 = entity.GetOrDefault<PostFX>();
	render_state.shader_pass = game.shader.Get<ShapeShader::Quad>();

	if (button_texture != nullptr && *button_texture != impl::Texture{}) {
		Color button_tint{ color::White };
		if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
			entity.Has<impl::ButtonTintToggled>()) {
			button_tint = entity.Get<impl::ButtonTintToggled>().current_;
		} else if (entity.Has<impl::ButtonTint>()) {
			button_tint = entity.Get<impl::ButtonTint>().current_;
		}
		V4_float final_tint_n{ button_tint.Normalized() * tint };
		PTGN_ASSERT(!size.IsZero(), "Unable to deduce size of textured button");
		ctx.AddTexturedQuad(
			*button_texture, transform, size, origin, Color{ final_tint_n }, depth,
			Sprite{ entity }.GetTextureCoordinates(false), render_state
		);
	} else {
		PTGN_ASSERT(!size.IsZero(), "Buttons must have a non-zero size");
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
			auto background_color_n{ button_color.Normalized() };
			if (background_color_n != V4_float{}) {
				// TODO: Add rounded buttons.
				/*if (radius_ > 0.0f) {
					RoundedRect r{ i.rect_.position, i.radius_, i.rect_.size, i.rect_.origin,
									i.rect_.rotation };
					r.Draw(bg, i.line_thickness_, i.render_layer_);
				} else {*/
				ctx.AddQuad(
					transform, size, origin, Color{ background_color_n }, depth,
					background_line_width, render_state
				);
			}
		}
	}

	Text text;
	if (entity.Has<impl::ButtonToggled>() && entity.Get<impl::ButtonToggled>() &&
		entity.Has<impl::ButtonTextToggled>()) {
		const auto& button_text_toggled{ entity.Get<impl::ButtonTextToggled>() };
		text = button_text_toggled.GetValid(state);
	} else if (entity.Has<impl::ButtonText>()) {
		const auto& button_text{ entity.Get<impl::ButtonText>() };
		text = button_text.GetValid(state);
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
				transform, size, origin, Color{ border_color_n }, depth, border_width, render_state
			);
		}
	}

	if (text != Text{}) {
		auto text_transform{ GetDrawTransform(text) };

		V2_float text_size;
		if (entity.Has<impl::ButtonTextFixedSize>()) {
			text_size = entity.Get<impl::ButtonTextFixedSize>();
		} else {
			text_size = text.GetSize();
		}
		if (NearlyEqual(text_size.x, 0.0f)) {
			text_size.x = size.x;
		}
		if (NearlyEqual(text_size.y, 0.0f)) {
			text_size.y = size.y;
		}

		Sprite text_sprite{ text };

		// Offset by button size so that text is initially centered on button center.
		text_transform.Translate(-GetOriginOffset(origin, size * Abs(text_transform.GetScale())));

		if (text_sprite.Has<TextColor>() && text_sprite.Get<TextColor>().a == 0) {
			return;
		}

		if (!text_sprite.Has<TextContent>()) {
			return;
		}

		if (const std::string & content{ text_sprite.Get<TextContent>().GetValue() };
			content.empty()) {
			return;
		}

		const auto& text_texture{ text_sprite.GetTexture() };

		if (!text_texture.IsValid()) {
			return;
		}

		auto text_depth{ GetDepth(text) };
		auto text_blend_mode{ GetBlendMode(text) };
		auto text_tint{ GetTint(text).Normalized() };
		auto text_origin{ GetDrawOrigin(text) };
		Camera text_camera;
		if (text.Has<Camera>()) {
			text_camera = text.Get<Camera>();
		} else {
			text_camera = camera;
		}

		auto text_coords{ text_sprite.GetTextureCoordinates(false) };

		auto text_render_state{ render_state };
		text_render_state.blend_mode = text_blend_mode;
		text_render_state.camera	 = text_camera;

		ctx.AddTexturedQuad(
			text_texture, text_transform, text_size, text_origin, Color{ text_tint * tint },
			text_depth, text_coords, render_state
		);
	}
}

Button& Button::OnActivate(const std::function<void()>& on_activate_callback) {
	AddScript<impl::ButtonActivateScript>(*this, on_activate_callback);
	return *this;
}

Button& Button::Enable(bool enable_hover, bool reset_state) {
	return Button::SetEnabled(true, enable_hover, reset_state);
}

Button& Button::Disable(bool disable_hover, bool reset_state) {
	return Button::SetEnabled(false, !disable_hover, reset_state);
}

Button& Button::SetEnabled(bool enable_activation, bool enable_hover, bool reset_state) {
	Add<impl::ButtonEnabled>(enable_activation, enable_hover);
	if (reset_state) {
		auto& state{ Get<impl::InternalButtonState>() };
		state = impl::InternalButtonState::IdleUp;
	}
	return *this;
}

bool Button::IsEnabled(bool check_for_hover_enabled) const {
	if (!Has<impl::ButtonEnabled>()) {
		return false;
	}
	const auto& enabled{ Get<impl::ButtonEnabled>() };
	if (check_for_hover_enabled) {
		return enabled.hover;
	}
	return enabled.activate;
}

V2_float Button::GetSize() const {
	V2_float size;

	const impl::Texture* button_texture{ nullptr };

	TextureHandle button_texture_key;

	if (Has<TextureHandle>()) {
		button_texture_key = Get<TextureHandle>();
	}

	if (game.texture.Has(button_texture_key)) {
		button_texture = &button_texture_key.GetTexture();
	}

	if (button_texture != nullptr && size.IsZero()) {
		size = button_texture->GetSize();
	}

	if (!size.IsZero()) {
		return size;
	}

	if (Has<Rect>()) {
		size = Get<Rect>().GetSize();
	} else if (Has<Circle>()) {
		size = V2_float{ Get<Circle>().radius * 2.0f };
	}

	return size;
}

Button& Button::SetSize(const V2_float& size) {
	Remove<Circle>();
	if (Has<Rect>()) {
		Get<Rect>() = Rect{ size };
	} else {
		Add<Rect>(size);
	}
	if (IsInteractive(*this)) {
		impl::ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Rect>(size);
		AddInteractable(*this, shape);
	}
	return *this;
}

Button& Button::SetRadius(float radius) {
	Remove<Rect>();
	if (Has<Circle>()) {
		Get<Circle>() = Circle{ radius };
	} else {
		Add<Circle>(radius);
	}
	if (IsInteractive(*this)) {
		impl::ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Circle>(radius);
		AddInteractable(*this, shape);
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
	const TextContent& content, const TextColor& text_color, const FontSize& font_size,
	const ResourceHandle& font_key, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, content, text_color, font_size, font_key, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(*this, GetScene(), state, content, text_color, font_size, font_key, text_properties);
	}
	return *this;
}

Text Button::GetText(ButtonState state) const {
	return Get<impl::ButtonText>().GetValid(state);
}

TextColor Button::GetTextColor(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextColor(state);
}

Button& Button::SetTextColor(const TextColor& text_color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, TextContent{}, text_color, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(text_color);
	}
	return *this;
}

TextContent Button::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextContent(state);
}

Button& Button::SetTextContent(const TextContent& content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, content, TextColor{}, FontSize{}, ResourceHandle{},
			TextProperties{}
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
		TextProperties p{};
		p.justify = justify;
		auto& c{ Add<impl::ButtonText>(
			*this, GetScene(), state, TextContent{}, TextColor{}, FontSize{}, ResourceHandle{}, p
		) };
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

FontSize Button::GetFontSize(ButtonState state) const {
	return Get<impl::ButtonText>().GetFontSize(state);
}

Button& Button::SetFontSize(const FontSize& font_size, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		auto& c{ Add<impl::ButtonText>(
			*this, GetScene(), state, TextContent{}, TextColor{}, font_size, ResourceHandle{},
			TextProperties{}
		) };
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
	if (IsInteractive(*this) && GetInteractables(*this).empty()) {
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		auto size{ texture_key.GetSize() };
		shape.Add<Rect>(size);
		AddInteractable(*this, shape);
	}
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

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

ButtonState Button::GetState() const {
	PTGN_ASSERT(Has<impl::InternalButtonState>());
	const auto& state{ Get<impl::InternalButtonState>() };
	if (state == impl::InternalButtonState::Hover ||
		state == impl::InternalButtonState::HoverPressed) {
		return ButtonState::Hover;
	} else if (state == impl::InternalButtonState::Pressed ||
			   state == impl::InternalButtonState::HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

void Button::Activate() {
	if (!IsEnabled(false) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonActivate);
}

void Button::StartHover() {
	if (!IsEnabled(true) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonHoverStart);
}

void Button::StopHover() {
	if (!IsEnabled(true) || !Has<Scripts>()) {
		return;
	}
	Get<Scripts>().AddAction(&ButtonScript::OnButtonHoverStop);
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

TextColor ToggleButton::GetTextColorToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextColor(state);
}

ToggleButton& ToggleButton::SetTextColorToggled(const TextColor& text_color, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, TextContent{}, text_color, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(text_color);
	}
	return *this;
}

TextContent ToggleButton::GetTextContentToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextContent(state);
}

ToggleButton& ToggleButton::SetTextContentToggled(const TextContent& content, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, content, TextColor{}, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

ToggleButton& ToggleButton::SetTextToggled(
	const TextContent& content, const TextColor& text_color, const FontSize& font_size,
	const ResourceHandle& font_key, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, content, text_color, font_size, font_key, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(*this, GetScene(), state, content, text_color, font_size, font_key, text_properties);
	}
	return *this;
}

Text ToggleButton::GetTextToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetValid(state);
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

impl::ToggleButtonGroupInfo::~ToggleButtonGroupInfo() {
	for (auto [key, toggle_button] : buttons_) {
		toggle_button.Destroy();
	}
}

ToggleButton& ToggleButtonGroup::Load(std::string_view button_key, ToggleButton&& toggle_button) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto key{ Hash(button_key) };

	if (auto it{ info.buttons_.find(key) }; it == info.buttons_.end()) {
		auto [new_it, inserted] = info.buttons_.try_emplace(key, toggle_button);
		PTGN_ASSERT(inserted, "Failed to insert toggle button");
		AddToggleScript(new_it->second);
		return new_it->second;
	} else {
		it->second.Destroy();
		it->second = toggle_button;
		return it->second;
	}
}

void ToggleButtonGroup::Unload(std::string_view button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto key{ Hash(button_key) };

	auto it{ info.buttons_.find(key) };

	if (it == info.buttons_.end()) {
		return;
	}

	it->second.Destroy();
	info.buttons_.erase(it);
}

void ToggleButtonGroup::AddToggleScript(ToggleButton& target) {
	AddScript<impl::ToggleButtonGroupScript>(target, *this);
}

} // namespace ptgn