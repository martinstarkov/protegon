#include "ui/button.h"

#include <functional>
#include <list>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/generic.h"
#include "components/interactive.h"
#include "components/sprite.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
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
#include "renderer/text.h"
#include "renderer/texture.h"
#include "resources/resource_manager.h"
#include "scene/camera.h"

namespace ptgn {

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
	if (!Button{ entity }.IsEnabled(false)) {
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
	if (!Button{ entity }.IsEnabled(false)) {
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
	if (!Button{ entity }.IsEnabled(false)) {
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
	if (!Button{ entity }.IsEnabled(false)) {
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

	PTGN_ASSERT(self.Has<ToggleButtonGroupKey>());

	PTGN_ASSERT(toggle_button_group);
	toggle_button_group.SetActive(self.Get<ToggleButtonGroupKey>());
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
		default:				   PTGN_ERROR("Invalid button state")
	}
}

Color& ButtonColor::Get(ButtonState state) {
	return const_cast<Color&>(std::as_const(*this).Get(state));
}

ButtonText::ButtonText(
	Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& text_properties
) {
	Set(parent, manager, ButtonState::Default, text_content, text_color, font_size, font_key,
		text_properties);
	if (state != ButtonState::Default) {
		Set(parent, manager, state, text_content, text_color, font_size, font_key, text_properties);
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
		default:				   PTGN_ERROR("Invalid button state")
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
	return GetValid(state).GetFontSize(false, {});
}

TextJustify ButtonText::GetTextJustify(ButtonState state) const {
	return GetValid(state).GetTextJustify();
}

void ButtonText::Set(
	Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
	const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& text_properties
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	Text text{ Get(state) };
	if (text == Text{}) {
		text = CreateText(manager, text_content, text_color, font_size, font_key, text_properties);
		Hide(text);
		SetParent(text, parent);
		switch (state) {
			case ButtonState::Default: {
				PTGN_ASSERT(!default_);
				default_ = text;
				break;
			}
			case ButtonState::Hover: {
				PTGN_ASSERT(!hover_);
				hover_ = text;
				break;
			}
			case ButtonState::Pressed: {
				PTGN_ASSERT(!pressed_);
				pressed_ = text;
				break;
			}
			case ButtonState::Current: [[fallthrough]];
			default:				   PTGN_ERROR("Invalid button state")
		}
	} else {
		text.SetParameter(text_color, false);
		text.SetParameter(text_content, false);
		text.SetParameter(font_key, false);
		text.SetParameter(font_size, false);
		text.SetProperties(text_properties, true, {});
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

template <typename TProperty>
static void UpdateStateProperty(Button& button, const ButtonState& state) {
	if (auto property{ button.TryGet<TProperty>() }) {
		property->SetToState(state);
	}
}

static void SetTextureState(Button& button, bool is_toggled, const ButtonState& state) {
	auto key{ button.TryGet<TextureHandle>() };
	if (!key) {
		return;
	}
	if (!button.IsEnabled(false) && button.Has<impl::ButtonDisabledTextureKey>()) {
		*key = button.Get<impl::ButtonDisabledTextureKey>();
	} else if (is_toggled && button.Has<impl::ButtonTextureToggled>()) {
		*key = button.Get<impl::ButtonTextureToggled>().Get(state);
	} else if (button.Has<impl::ButtonTexture>()) {
		*key = button.Get<impl::ButtonTexture>().Get(state);
	}
}

static bool IsToggled(const Button& button) {
	return button.Has<impl::ButtonToggled>() && button.Get<impl::ButtonToggled>();
}

template <typename DefaultComponent, typename ToggledComponent>
static Color GetEffectiveColor(
	const Button& button, bool is_toggled, const Color& fallback_color = color::Transparent
) {
	if (is_toggled && button.Has<ToggledComponent>()) {
		return button.Get<ToggledComponent>().current_;
	} else if (button.Has<DefaultComponent>()) {
		return button.Get<DefaultComponent>().current_;
	}
	return fallback_color;
}

// @return Button texture, or nullptr is button has no valid texture.
static const impl::Texture* GetButtonTexture(
	Button& button, bool is_toggled, const ButtonState& state
) {
	SetTextureState(button, is_toggled, state);

	auto button_texture_key{ button.GetOrDefault<TextureHandle>() };

	if (!game.texture.Has(button_texture_key)) {
		return nullptr;
	}

	return &button_texture_key.GetTexture();
}

// @return Button text, or empty text object if button has no text.
static Text GetButtonText(const Button& button, bool is_toggled, const ButtonState& state) {
	Text text;

	if (is_toggled && button.Has<impl::ButtonTextToggled>()) {
		const auto& button_text_toggled{ button.Get<impl::ButtonTextToggled>() };
		text = button_text_toggled.GetValid(state);
	} else if (button.Has<impl::ButtonText>()) {
		const auto& button_text{ button.Get<impl::ButtonText>() };
		text = button_text.GetValid(state);
	}

	return text;
}

static void DrawTexturedButton(
	impl::RenderData& ctx, const Button& button, bool is_toggled,
	const impl::Texture& button_texture, const impl::ShapeDrawInfo& info, const V2_float& size,
	Origin origin, const V4_float& tint_n
) {
	auto texture_tint{ GetEffectiveColor<impl::ButtonTint, impl::ButtonTintToggled>(
		button, is_toggled, color::White
	) };

	if (!texture_tint.a) {
		return;
	}

	ctx.AddTexturedQuad(
		info.transform, button_texture, Rect{ size }, origin, texture_tint.Normalized() * tint_n,
		info.depth, Sprite{ button }.GetTextureCoordinates(false), info.state
	);
}

template <typename DefaultComponent, typename ToggledComponent, typename LineWidthComponent>
static void DrawButtonQuad(
	impl::RenderData& ctx, const Button& button, bool is_toggled, const impl::ShapeDrawInfo& info,
	const V2_float& size, Origin origin, const V4_float& tint_n
) {
	auto line_width{ button.GetOrDefault<LineWidthComponent>() };

	if (line_width == 0.0f) {
		return;
	}

	auto color{ GetEffectiveColor<DefaultComponent, ToggledComponent>(button, is_toggled) };

	if (!color.a) {
		return;
	}

	// TODO: Fix rounded buttons.
	ctx.AddQuad(
		info.transform, Rect{ size }, origin, color.Normalized() * tint_n, info.depth, line_width,
		info.state
	);
}

static void DrawColoredButton(
	impl::RenderData& ctx, const Button& button, bool is_toggled, const impl::ShapeDrawInfo& info,
	const V2_float& size, Origin origin, const V4_float& tint_n
) {
	DrawButtonQuad<impl::ButtonColor, impl::ButtonColorToggled, impl::ButtonBackgroundWidth>(
		ctx, button, is_toggled, info, size, origin, tint_n
	);
}

static void DrawButtonBorder(
	impl::RenderData& ctx, const Button& button, bool is_toggled, const impl::ShapeDrawInfo& info,
	const V2_float& size, Origin origin, const V4_float& tint_n
) {
	DrawButtonQuad<
		impl::ButtonBorderColor, impl::ButtonBorderColorToggled, impl::ButtonBorderWidth>(
		ctx, button, is_toggled, info, size, origin, tint_n
	);
}

static void DrawButtonText(
	impl::RenderData& ctx, const Button& button, const Text& text, const V2_float& button_size,
	Origin button_origin, const impl::ShapeDrawInfo& info
) {
	if (!text) {
		return;
	}

	V2_float text_size;

	if (button.Has<impl::ButtonTextFixedSize>()) {
		text_size = button.Get<impl::ButtonTextFixedSize>();
		if (NearlyEqual(text_size.x, 0.0f)) {
			text_size.x = button_size.x;
		}
		if (NearlyEqual(text_size.y, 0.0f)) {
			text_size.y = button_size.y;
		}
	}

	auto text_camera{ text.GetOrDefault<Camera>(info.state.camera) };

	impl::DrawText(ctx, text, text_size, text_camera, info.tint, button_origin, button_size);
}

static void DrawButton(
	impl::RenderData& ctx, const Button& button, bool is_toggled,
	const impl::Texture* button_texture, const Text& text, const impl::ShapeDrawInfo& info,
	const V2_float& button_size, Origin button_origin, const V4_float& tint_n
) {
	if (button_texture) {
		DrawTexturedButton(
			ctx, button, is_toggled, *button_texture, info, button_size, button_origin, tint_n
		);
	} else {
		DrawColoredButton(ctx, button, is_toggled, info, button_size, button_origin, tint_n);
	}

	DrawButtonBorder(ctx, button, is_toggled, info, button_size, button_origin, tint_n);
	DrawButtonText(ctx, button, text, button_size, button_origin, info);
}

void Button::Draw(impl::RenderData& ctx, const Entity& entity) {
	Button button{ entity };

	impl::ShapeDrawInfo info{ button };

	if (info.tint.a == 0) {
		return;
	}

	auto tint_n{ info.tint.Normalized() };
	const auto state{ button.GetState() };
	auto button_size{ button.GetSize() };
	bool is_toggled{ IsToggled(button) };
	PTGN_ASSERT(!button_size.IsZero(), "Buttons must have a non-zero size");
	Origin button_origin{ GetDrawOrigin(button) };
	auto text{ GetButtonText(button, is_toggled, state) };

	UpdateStateProperty<impl::ButtonColor>(button, state);
	UpdateStateProperty<impl::ButtonColorToggled>(button, state);
	UpdateStateProperty<impl::ButtonTint>(button, state);
	UpdateStateProperty<impl::ButtonTintToggled>(button, state);
	UpdateStateProperty<impl::ButtonBorderColor>(button, state);
	UpdateStateProperty<impl::ButtonBorderColorToggled>(button, state);

	auto button_texture{ GetButtonTexture(button, is_toggled, state) };

	DrawButton(
		ctx, button, is_toggled, button_texture, text, info, button_size, button_origin, tint_n
	);
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
		ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Rect>(size);
		AddInteractable(*this, std::move(shape));
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
		ClearInteractables(*this);
		auto shape{ GetManager().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Circle>(radius);
		AddInteractable(*this, std::move(shape));
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
			*this, GetManager(), state, content, text_color, font_size, font_key, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(
			*this, GetManager(), state, content, text_color, font_size, font_key, text_properties
		);
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
			*this, GetManager(), state, TextContent{}, text_color, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
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
			*this, GetManager(), state, content, TextColor{}, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
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
		Add<impl::ButtonText>(
			*this, GetManager(), state, TextContent{}, TextColor{}, FontSize{}, ResourceHandle{}, p
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetTextJustify(justify);
	}
	return *this;
}

V2_float Button::GetTextFixedSize() const {
	return GetOrDefault<impl::ButtonTextFixedSize>();
}

Button& Button::SetTextFixedSize(const V2_float& size) {
	Add<impl::ButtonTextFixedSize>(size);
	return *this;
}

Button& Button::ClearTextFixedSize() {
	Remove<impl::ButtonTextFixedSize>();
	return *this;
}

FontSize Button::GetFontSize(ButtonState state) const {
	return Get<impl::ButtonText>().GetFontSize(state);
}

Button& Button::SetFontSize(const FontSize& font_size, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetManager(), state, TextContent{}, TextColor{}, font_size, ResourceHandle{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
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
		AddInteractable(*this, std::move(shape));
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
			*this, GetManager(), state, TextContent{}, text_color, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
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
			*this, GetManager(), state, content, TextColor{}, FontSize{}, ResourceHandle{},
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
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
			*this, GetManager(), state, content, text_color, font_size, font_key, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(
			*this, GetManager(), state, content, text_color, font_size, font_key, text_properties
		);
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

ToggleButton& ToggleButtonGroup::Load(
	const ToggleButtonGroupKey& button_key, ToggleButton&& toggle_button
) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	toggle_button.Add<ToggleButtonGroupKey>(button_key);

	if (auto it{ info.buttons.find(button_key) }; it == info.buttons.end()) {
		auto [new_it, inserted] = info.buttons.try_emplace(button_key, std::move(toggle_button));
		PTGN_ASSERT(inserted, "Failed to insert toggle button");
		AddToggleScript(new_it->second);
		return new_it->second;
	} else {
		it->second = std::move(toggle_button);
		return it->second;
	}
}

void ToggleButtonGroup::Unload(const ToggleButtonGroupKey& button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(button_key) };

	if (it == info.buttons.end()) {
		return;
	}

	info.buttons.erase(it);
}

ToggleButton ToggleButtonGroup::GetActive() const {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(info.active) };

	if (it == info.buttons.end() || !it->second.IsToggled()) {
		return {};
	}

	return it->second;
}

void ToggleButtonGroup::SetActive(const ToggleButtonGroupKey& button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupInfo>());

	auto& info{ Get<impl::ToggleButtonGroupInfo>() };

	auto it{ info.buttons.find(button_key) };

	PTGN_ASSERT(
		it != info.buttons.end(),
		"Cannot set non-existent toggle button key to active: ", button_key.GetKey()
	);

	for (auto& [key, toggle_button] : info.buttons) {
		toggle_button.SetToggled(false);
	}

	info.active = button_key;

	it->second.SetToggled(true);
}

void ToggleButtonGroup::AddToggleScript(ToggleButton& target) {
	AddScript<impl::ToggleButtonGroupScript>(target, *this);
}

Button CreateButton(Manager& manager) {
	Button button{ manager.CreateEntity() };

	Show(button);
	SetDraw<Button>(button);

	SetInteractive(button);
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

Button CreateTextButton(
	Manager& manager, const TextContent& text_content, const TextColor& text_color
) {
	Button text_button{ CreateButton(manager) };

	text_button.SetText(text_content, text_color);

	return text_button;
}

ToggleButton CreateToggleButton(Manager& manager, bool toggled) {
	ToggleButton toggle_button{ CreateButton(manager) };

	AddScript<impl::ToggleButtonScript>(toggle_button);
	toggle_button.Add<impl::ButtonToggled>(toggled);

	return toggle_button;
}

ToggleButtonGroup CreateToggleButtonGroup(Manager& manager) {
	ToggleButtonGroup toggle_button_group{ manager.CreateEntity() };

	toggle_button_group.Add<impl::ToggleButtonGroupInfo>();

	return toggle_button_group;
}

} // namespace ptgn