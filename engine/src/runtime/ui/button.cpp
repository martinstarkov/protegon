#include "runtime/ui/button.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/input/interactive.h"
#include "runtime/input/scene_input.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

ButtonDisabledTexture::ButtonDisabledTexture(const Texture& t) : Texture{ t } {}

ButtonDisabledTexture::ButtonDisabledTexture(Texture&& t) : Texture{ std::move(t) } {}

void InternalButtonScript::OnEvent(EventDispatcher d) {
	d.Dispatch<MouseMoveOver>([this](const MouseMoveOver&) { OnMouseMoveOver(); });
	d.Dispatch<MouseMoveOut>([this](const MouseMoveOut&) { OnMouseMoveOut(); });
	d.Dispatch<MousePressedOver>([this](const MousePressedOver& e) { OnMousePressedOver(e.button); }
	);
	d.Dispatch<MousePressedOut>([this](const MousePressedOut& e) { OnMousePressedOut(e.button); });
	d.Dispatch<MouseReleasedOver>([this](const MouseReleasedOver& e) {
		OnMouseReleasedOver(e.button);
	});
	d.Dispatch<MouseReleasedOut>([this](const MouseReleasedOut& e) { OnMouseReleasedOut(e.button); }
	);
}

void InternalButtonScript::OnMouseMoveOver() {
	using enum InternalButtonState;
	auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	if (state == IdleUp) {
		state = Hover;
		button.StartHover();
	} else if (state == IdleDown) {
		state = HoverPressed;
		button.StartHover();
	} else if (state == HeldOutside) {
		state = Pressed;
		return;
	}
	button.ContinueHover();
}

void InternalButtonScript::OnMouseMoveOut() {
	auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	using enum InternalButtonState;
	if (state == Hover) {
		state = IdleUp;
		button.StopHover();
	} else if (state == Pressed) {
		state = HeldOutside;
		button.StopHover();
	} else if (state == HoverPressed) {
		state = IdleDown;
		button.StopHover();
	}
}

void InternalButtonScript::OnMousePressedOver(Mouse mouse) {
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

void InternalButtonScript::OnMousePressedOut(Mouse mouse) {
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

void InternalButtonScript::OnMouseReleasedOver(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum ptgn::impl::InternalButtonState;
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == Pressed) {
			state = Hover;
			button.Activate();
		} else if (state == HoverPressed) {
			state = Hover;
		}
	}
}

void InternalButtonScript::OnMouseReleasedOut(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum ptgn::impl::InternalButtonState;
		auto& state{ entity.Get<InternalButtonState>() };
		if (state == IdleDown) {
			state = IdleUp;
		} else if (state == HeldOutside) {
			state = IdleUp;
		}
	}
}

void ToggleButtonScript::OnEvent(EventDispatcher d) {
	d.Dispatch<ButtonActivate>([this](const ButtonActivate&) { OnButtonActivate(); });
}

void ToggleButtonScript::OnButtonActivate() const {
	ToggleButton self{ entity };
	if (!self.IsEnabled(false)) {
		return;
	}
	self.Toggle();
}

ToggleButtonGroupScript::ToggleButtonGroupScript(const ToggleButtonGroup& group) :
	toggle_button_group_{ group } {}

void ToggleButtonGroupScript::OnEvent(EventDispatcher d) {
	d.Dispatch<ButtonActivate>([this](const ButtonActivate&) { OnButtonActivate(); });
}

void ToggleButtonGroupScript::OnButtonActivate() {
	ToggleButton self{ entity };
	if (!self.IsEnabled(false)) {
		return;
	}

	PTGN_ASSERT(self.Has<ToggleButtonGroupKey>());

	PTGN_ASSERT(toggle_button_group_);
	toggle_button_group_.SetActiveKey(self.Get<ToggleButtonGroupKey>());
}

void ButtonColor::SetToState(ButtonState state) {
	current_ = Get(state);
}

const Color& ButtonColor::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Current: return current_;
		case Default: return default_;
		case Hover:	  return hover_;
		case Pressed: return pressed_;
		default:	  PTGN_ERROR("Invalid button state");
	}
}

Color& ButtonColor::Get(ButtonState state) {
	return const_cast<Color&>(std::as_const(*this).Get(state));
}

ButtonText::ButtonText(
	Entity parent, Scene& scene, ButtonState state, std::string_view text_content, Color text_color,
	std::optional<float> font_size, std::optional<Font> font, const TextProperties& text_properties
) {
	Set(parent, scene, ButtonState::Default, text_content, text_color, font_size, font,
		text_properties);
	if (state != ButtonState::Default) {
		Set(parent, scene, state, text_content, text_color, font_size, font, text_properties);
	}
}

Text ButtonText::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Default: return Text{ default_ };
		case Hover:	  return Text{ hover_ };
		case Pressed: return Text{ pressed_ };
		case Current: [[fallthrough]];
		default:	  PTGN_ERROR("Invalid button state");
	}
}

Text ButtonText::GetValid(ButtonState state) const {
	using enum ptgn::ButtonState;
	if (state == Current) {
		return Get(Default);
	}
	auto text{ Get(state) };
	if (!text) {
		return Get(Default);
	}
	return text;
}

Color ButtonText::GetTextColor(ButtonState state) const {
	return GetValid(state).GetColor();
}

std::string ButtonText::GetTextContent(ButtonState state) const {
	return GetValid(state).GetContent();
}

float ButtonText::GetFontSize(ButtonState state) const {
	return GetValid(state).GetFontSize(false);
}

TextJustify ButtonText::GetTextJustify(ButtonState state) const {
	return GetValid(state).GetJustify();
}

void ButtonText::Set(
	Entity parent, Scene& scene, ButtonState state, std::string_view text_content, Color text_color,
	std::optional<float> font_size, std::optional<Font> font, const TextProperties& text_properties
) {
	PTGN_ASSERT(
		state != ButtonState::Current,
		"Cannot set button's current text as it is a non-owning pointer"
	);
	auto text{ Get(state) };
	if (!text) {
		text = CreateText(scene, text_content, text_color, font_size, font, text_properties);
		Hide(text);
		SetParent(text, parent);
		switch (state) {
			using enum ptgn::ButtonState;
			case Default: {
				PTGN_ASSERT(!default_);
				default_ = GameObject{ std::move(text) };
				break;
			}
			case Hover: {
				PTGN_ASSERT(!hover_);
				hover_ = GameObject{ std::move(text) };
				break;
			}
			case Pressed: {
				PTGN_ASSERT(!pressed_);
				pressed_ = GameObject{ std::move(text) };
				break;
			}
			case Current: [[fallthrough]];
			default:	  PTGN_ERROR("Invalid button state");
		}
	} else {
		Text::SetParameter(text, impl::TextColor{ text_color }, false);
		Text::SetParameter(text, impl::TextContent{ text_content }, false);
		Text::SetParameter(text, font.value_or(Font{}), false);
		Text::SetParameter(text, FontSize{ font_size.value_or(FontSize{}) }, false);
		Text::SetProperties(text, text_properties, true);
	}
}

Texture ButtonTexture::Get(ButtonState state) const {
	switch (state) {
		using enum ptgn::ButtonState;
		case Default: return default_;
		case Hover:	  return hover_;
		case Pressed: return pressed_;
		case Current: [[fallthrough]];
		default:	  PTGN_ERROR("Invalid button state");
	}
}

template <typename TProperty>
static void UpdateStateProperty(Entity entity, const ButtonState& state) {
	if (auto property{ entity.TryGet<TProperty>() }) {
		property->SetToState(state);
	}
}

static void SetTextureState(Button button, bool is_toggled, const ButtonState& state) {
	auto key{ button.TryGet<Texture>() };
	if (!key) {
		return;
	}
	if (!button.IsEnabled(false) && button.Has<impl::ButtonDisabledTexture>()) {
		*key = button.Get<impl::ButtonDisabledTexture>();
	} else if (is_toggled && button.Has<impl::ButtonTextureToggled>()) {
		*key = button.Get<impl::ButtonTextureToggled>().Get(state);
	} else if (button.Has<impl::ButtonTexture>()) {
		*key = button.Get<impl::ButtonTexture>().Get(state);
	}
}

static bool IsToggled(Entity button) {
	return button.Has<impl::ButtonToggled>();
}

template <typename DefaultComponent, typename ToggledComponent>
static Color GetEffectiveColor(
	Entity button, bool is_toggled, Color fallback_color = color::Transparent
) {
	if (is_toggled && button.Has<ToggledComponent>()) {
		return button.Get<ToggledComponent>().current_;
	} else if (button.Has<DefaultComponent>()) {
		return button.Get<DefaultComponent>().current_;
	}
	return fallback_color;
}

// @return Button texture, or {} is button has no valid texture.
static std::optional<Texture> GetButtonTexture(
	Button button, bool is_toggled, const ButtonState& state
) {
	SetTextureState(button, is_toggled, state);

	auto texture{ button.TryGet<Texture>() };
	return texture ? std::optional<Texture>{ *texture } : std::nullopt;
}

// @return Button text, or empty text object if button has no text.
static Entity GetButtonText(Entity button, bool is_toggled, const ButtonState& state) {
	Entity text;

	if (is_toggled && button.Has<impl::ButtonTextToggled>()) {
		const auto& button_text_toggled{ button.Get<impl::ButtonTextToggled>() };
		text = button_text_toggled.GetValid(state);
	} else if (button.Has<impl::ButtonText>()) {
		const auto& button_text{ button.Get<impl::ButtonText>() };
		text = button_text.GetValid(state);
	}

	return text;
}

template <typename Derived>
void ButtonBase<Derived>::Draw(Renderer& renderer, Entity entity) {
	Button button{ entity };
	Color tint{ ptgn::GetTint(button) };

	if (tint.a == 0) {
		return;
	}

	auto transform{ GetDrawTransform(button) };
	auto depth{ GetDepth(button) };
	auto blend_mode{ GetBlendMode(button) };

	auto tint_n{ tint.Normalized() };
	const auto state{ button.GetState() };
	auto button_size{ button.GetSize() };
	bool is_toggled{ IsToggled(button) };
	PTGN_ASSERT(!button_size.IsZero(), "Buttons must have a non-zero size");
	auto button_origin{ GetDrawOrigin(button) };
	auto text{ GetButtonText(button, is_toggled, state) };

	UpdateStateProperty<impl::ButtonColor>(button, state);
	UpdateStateProperty<impl::ButtonColorToggled>(button, state);
	UpdateStateProperty<impl::ButtonTint>(button, state);
	UpdateStateProperty<impl::ButtonTintToggled>(button, state);
	UpdateStateProperty<impl::ButtonBorderColor>(button, state);
	UpdateStateProperty<impl::ButtonBorderColorToggled>(button, state);

	auto button_texture{ GetButtonTexture(button, is_toggled, state) };

	if (button_texture.has_value()) {
		auto texture_tint{ GetEffectiveColor<impl::ButtonTint, impl::ButtonTintToggled>(
			button, is_toggled, color::White
		) };

		if (texture_tint.a) {
			impl::DrawQuadTexture(
				renderer, *button_texture, transform, button_size, button_origin,
				impl::Tint{ texture_tint.Normalized() * tint_n }, depth, blend_mode,
				GetTextureCoordinates(button, false)
			);
		}
	} else {
		auto line_width{ button.GetOrDefault<impl::ButtonBackgroundWidth>() };

		if (line_width > 0.0f) {
			auto color{
				GetEffectiveColor<impl::ButtonColor, impl::ButtonColorToggled>(button, is_toggled)
			};

			if (color.a) {
				// TODO: Fix.
				/*renderer.DrawRect(
					transform, button_size, Tint{ color.Normalized() * tint_n },
					line_width.GetValue(), button_origin, depth, blend_mode, camera, post_fx
				);*/
			}
		}
	}

	auto line_width{ button.GetOrDefault<impl::ButtonBorderWidth>() };

	if (line_width > 0.0f) {
		auto color{ GetEffectiveColor<impl::ButtonBorderColor, impl::ButtonBorderColorToggled>(
			button, is_toggled
		) };

		if (color.a) {
			// TODO: Fix.
			/*renderer.DrawRect(
				transform, button_size, Tint{ color.Normalized() * tint_n }, line_width.GetValue(),
				button_origin, depth, blend_mode, camera, post_fx
			);*/
		}
	}

	if (!text) {
		return;
	}

	V2_float text_size;

	if (auto fixed_size{ button.TryGet<ButtonTextFixedSize>() }) {
		text_size = { fixed_size->x.value_or(button_size.x),
					  fixed_size->y.value_or(button_size.y) };
	}

	Text::Draw(renderer, text, text_size, tint, button_origin, button_size);
}

template <typename Derived>
ButtonBase<Derived>::ButtonBase(Entity entity) : Entity{ entity } {}

template <typename Derived>
Derived& ButtonBase<Derived>::OnActivate(const std::function<void()>& callback) {
	AddScript<impl::ButtonActivateScript>(*this, callback);
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::OnHover(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverScript>(*this, callback);
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::OnHoverStart(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStartScript>(*this, callback);
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::OnHoverStop(const std::function<void()>& callback) {
	AddScript<impl::ButtonHoverStopScript>(*this, callback);
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::Enable(bool enable_hover, bool reset_state) {
	return SetEnabled(true, enable_hover, reset_state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::Disable(bool disable_hover, bool reset_state) {
	return SetEnabled(false, !disable_hover, reset_state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetEnabled(
	bool enable_activation, bool enable_hover, bool reset_state
) {
	Add<impl::ButtonEnabled>(enable_activation, enable_hover);
	if (reset_state) {
		auto& state{ Get<impl::InternalButtonState>() };
		state = impl::InternalButtonState::IdleUp;
	}
	return Self();
}

template <typename Derived>
bool ButtonBase<Derived>::IsEnabled(bool check_for_hover_enabled) const {
	if (!Has<impl::ButtonEnabled>()) {
		return false;
	}
	const auto& enabled{ Get<impl::ButtonEnabled>() };
	if (check_for_hover_enabled) {
		return enabled.hover;
	}
	return enabled.activate;
}

template <typename Derived>
V2_float ButtonBase<Derived>::GetSize() const {
	if (auto texture{ TryGet<Texture>() }) {
		return texture->GetSize();
	}

	if (auto rect{ TryGet<Rect>() }) {
		return rect->GetSize();
	}

	if (auto circle{ TryGet<Circle>() }) {
		return V2_float{ circle->radius * 2.0f };
	}

	PTGN_ERROR("Button has no valid size");
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetSize(V2_float size) {
	Remove<Circle>();
	if (Has<Rect>()) {
		Get<Rect>() = Rect{ size };
	} else {
		Add<Rect>(size);
	}
	if (IsInteractive(*this)) {
		ClearInteractiveShapes(*this);
		auto shape{ GetScene().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Rect>(size);
		AddInteractiveShape(*this, GameObject{ std::move(shape) });
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetRadius(float radius) {
	Remove<Rect>();
	if (Has<Circle>()) {
		Get<Circle>() = Circle{ radius };
	} else {
		Add<Circle>(radius);
	}
	if (IsInteractive(*this)) {
		ClearInteractiveShapes(*this);
		auto shape{ GetScene().CreateEntity() };
		AddChild(*this, shape);
		shape.Add<Circle>(radius);
		AddInteractiveShape(*this, GameObject{ std::move(shape) });
	}
	return Self();
}

template <typename Derived>
Color ButtonBase<Derived>::GetBackgroundColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonColor>() ? Get<impl::ButtonColor>() : impl::ButtonColor{} };
	return c.Get(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundColor(Color color, ButtonState state) {
	if (!Has<impl::ButtonColor>()) {
		Add<impl::ButtonColor>(color);
	} else {
		auto& c{ Get<impl::ButtonColor>() };
		c.Get(state) = color;
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetText(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonText>() };
		c.Set(*this, GetScene(), state, content, text_color, font_size, font, text_properties);
	}
	return Self();
}

template <typename Derived>
Entity ButtonBase<Derived>::GetText(ButtonState state) const {
	return Get<impl::ButtonText>().GetValid(state);
}

template <typename Derived>
Color ButtonBase<Derived>::GetTextColor(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextColor(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextColor(Color text_color, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, std::string_view{}, text_color, std::nullopt, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetColor(text_color);
	}
	return Self();
}

template <typename Derived>
std::string ButtonBase<Derived>::GetTextContent(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextContent(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextContent(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, content, Color{}, std::nullopt, std::nullopt, TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetContent(content);
	}
	return Self();
}

template <typename Derived>
TextJustify ButtonBase<Derived>::GetTextJustify(ButtonState state) const {
	return Get<impl::ButtonText>().GetTextJustify(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextJustify(TextJustify justify, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		TextProperties p{};
		p.justify = justify;
		Add<impl::ButtonText>(
			*this, GetScene(), state, std::string_view{}, Color{}, std::nullopt, std::nullopt, p
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetJustify(justify);
	}
	return Self();
}

template <typename Derived>
ButtonTextFixedSize ButtonBase<Derived>::GetTextFixedSize() const {
	return GetOrDefault<ButtonTextFixedSize>();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextFixedSize(ButtonTextFixedSize size) {
	if (!size.x.has_value() && !size.y.has_value()) {
		Remove<ButtonTextFixedSize>();
		return Self();
	}
	Add<ButtonTextFixedSize>(size);
	return Self();
}

template <typename Derived>
float ButtonBase<Derived>::GetFontSize(ButtonState state) const {
	return Get<impl::ButtonText>().GetFontSize(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetFontSize(float font_size, ButtonState state) {
	if (!Has<impl::ButtonText>()) {
		Add<impl::ButtonText>(
			*this, GetScene(), state, std::string_view{}, Color{}, font_size, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonText>() };
		c.Get(state).SetFontSize(font_size);
	}
	return Self();
}

template <typename Derived>
Texture ButtonBase<Derived>::GetTexture(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<Texture>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTexture>(),
		"Cannot retrieve texture key as no texture has been added to the button"
	);
	return Get<impl::ButtonTexture>().Get(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTexture(Texture texture, ButtonState state) {
	if (IsInteractive(*this) && GetInteractiveShapes(*this).empty()) {
		auto shape{ GetScene().CreateEntity() };
		AddChild(*this, shape);
		auto size{ texture.GetSize() };
		shape.Add<Rect>(size);
		AddInteractiveShape(*this, GameObject{ std::move(shape) });
	}
	if (!Has<Texture>()) {
		Add<Texture>(texture);
	} else if (state == ButtonState::Current) {
		Add<Texture>(texture);
		return Self();
	}
	if (!Has<impl::ButtonTexture>()) {
		Add<impl::ButtonTexture>(texture);
	} else {
		const auto& c{ Get<impl::ButtonTexture>() };
		c.Get(state) = texture;
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetDisabledTexture(Texture texture) {
	if (!texture) {
		Remove<impl::ButtonDisabledTexture>();
	} else {
		Add<impl::ButtonDisabledTexture>(texture);
	}
	return Self();
}

template <typename Derived>
Texture ButtonBase<Derived>::GetDisabledTexture() const {
	PTGN_ASSERT(
		Has<impl::ButtonDisabledTexture>(),
		"Cannot retrieve disabled texture key as it has not been set for the button"
	);
	return Get<impl::ButtonDisabledTexture>();
}

template <typename Derived>
Color ButtonBase<Derived>::GetTint(ButtonState state) const {
	const auto c{ Has<impl::ButtonTint>() ? Get<impl::ButtonTint>() : impl::ButtonTint{} };
	return c.Get(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTint(Color color, ButtonState state) {
	if (!Has<impl::ButtonTint>()) {
		auto& c{ Add<impl::ButtonTint>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTint>() };
		c.Get(state) = color;
	}
	return Self();
}

template <typename Derived>
Color ButtonBase<Derived>::GetBorderColor(ButtonState state) const {
	const auto c{ Has<impl::ButtonBorderColor>() ? Get<impl::ButtonBorderColor>()
												 : impl::ButtonBorderColor{} };
	return c.Get(state);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderColor(Color color, ButtonState state) {
	if (!Has<impl::ButtonBorderColor>()) {
		Add<impl::ButtonBorderColor>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColor>() };
		c.Get(state) = color;
	}
	return Self();
}

template <typename Derived>
float ButtonBase<Derived>::GetBackgroundLineWidth() const {
	return Has<impl::ButtonBackgroundWidth>() ? Get<impl::ButtonBackgroundWidth>()
											  : impl::ButtonBackgroundWidth{};
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundLineWidth(float line_width) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Invalid button background line width");
	if (line_width != -1.0f && line_width < 1.0f) {
		Remove<impl::ButtonBackgroundWidth>();
	} else {
		Add<impl::ButtonBackgroundWidth>(line_width);
	}
	return Self();
}

template <typename Derived>
float ButtonBase<Derived>::GetBorderWidth() const {
	return Has<impl::ButtonBorderWidth>() ? Get<impl::ButtonBorderWidth>()
										  : impl::ButtonBorderWidth{};
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderWidth(float line_width) {
	PTGN_ASSERT(line_width >= 1.0f || line_width == 0.0f, "Cannot set negative border width");
	if (line_width == 0.0f) {
		Remove<impl::ButtonBorderWidth>();
	} else {
		Add<impl::ButtonBorderWidth>(line_width);
	}
	return Self();
}

template <typename Derived>
impl::InternalButtonState ButtonBase<Derived>::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

template <typename Derived>
ButtonState ButtonBase<Derived>::GetState() const {
	PTGN_ASSERT(Has<impl::InternalButtonState>());
	const auto& state{ Get<impl::InternalButtonState>() };
	using enum impl::InternalButtonState;
	if (state == Hover || state == HoverPressed) {
		return ButtonState::Hover;
	} else if (state == Pressed || state == HeldOutside) {
		return ButtonState::Pressed;
	} else {
		return ButtonState::Default;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::Activate() {
	if (!IsEnabled(false)) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonActivate event;
		scripts->Emit(event);
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::StartHover() {
	if (!IsEnabled(true)) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStart event;
		scripts->Emit(event);
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::ContinueHover() {
	if (!IsEnabled(true)) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonHover event;
		scripts->Emit(event);
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::StopHover() {
	if (!IsEnabled(true)) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStop event;
		scripts->Emit(event);
	}
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::Self() {
	return static_cast<Derived&>(*this);
}

template <typename Derived>
const Derived& ButtonBase<Derived>::Self() const {
	return static_cast<const Derived&>(*this);
}

template class ButtonBase<Button>;
template class ButtonBase<ToggleButton>;

} // namespace impl

bool ToggleButton::IsToggled() const {
	return Has<impl::ButtonToggled>();
}

// ToggleButton& ToggleButton::OnToggle(const std::function<void(bool)>& callback) {
//	AddScript<impl::ButtonToggledScript>(*this, callback);
//	return *this;
// }

ToggleButton& ToggleButton::SetToggled(bool toggled) {
	if (toggled == IsToggled()) {
		return *this;
	}
	if (toggled) {
		Add<impl::ButtonToggled>();
	} else {
		Remove<impl::ButtonToggled>();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonToggleEvent event;
		event.toggled = toggled;
		scripts->Emit(event);
	}
	return *this;
}

ToggleButton& ToggleButton::Toggle() {
	return SetToggled(!IsToggled());
}

Color ToggleButton::GetBackgroundColorToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonColorToggled>() ? Get<impl::ButtonColorToggled>()
												  : impl::ButtonColorToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetBackgroundColorToggled(Color color, ButtonState state) {
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

ToggleButton& ToggleButton::SetTextColorToggled(Color text_color, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, std::string_view{}, text_color, std::nullopt, std::nullopt,
			TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetColor(text_color);
	}
	return *this;
}

std::string ToggleButton::GetTextContentToggled(ButtonState state) const {
	return Get<impl::ButtonTextToggled>().GetTextContent(state);
}

ToggleButton& ToggleButton::SetTextContentToggled(std::string_view content, ButtonState state) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, content, Color{}, std::nullopt, std::nullopt, TextProperties{}
		);
	} else {
		const auto& c{ Get<impl::ButtonTextToggled>() };
		c.Get(state).SetContent(content);
	}
	return *this;
}

ToggleButton& ToggleButton::SetTextToggled(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {
	if (!Has<impl::ButtonTextToggled>()) {
		Add<impl::ButtonTextToggled>(
			*this, GetScene(), state, content, text_color, font_size, font, text_properties
		);
	} else {
		auto& c{ Get<impl::ButtonTextToggled>() };
		c.Set(*this, GetScene(), state, content, text_color, font_size, font, text_properties);
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

ToggleButton& ToggleButton::SetBorderColorToggled(Color color, ButtonState state) {
	if (!Has<impl::ButtonBorderColorToggled>()) {
		Add<impl::ButtonBorderColorToggled>(color);
	} else {
		auto& c{ Get<impl::ButtonBorderColorToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

Texture ToggleButton::GetTextureToggled(ButtonState state) const {
	if (state == ButtonState::Current) {
		PTGN_ASSERT(
			Has<Texture>(),
			"Cannot retrieve current texture key as no texture has been added to the button"
		);
		return Get<Texture>();
	}
	PTGN_ASSERT(
		Has<impl::ButtonTextureToggled>(),
		"Cannot retrieve toggled texture key as no toggled texture has been added to the button"
	);
	return Get<impl::ButtonTextureToggled>().Get(state);
}

ToggleButton& ToggleButton::SetTextureToggled(Texture texture, ButtonState state) {
	if (!Has<Texture>()) {
		Add<Texture>(texture);
	} else if (state == ButtonState::Current && Has<impl::ButtonToggled>()) {
		Add<Texture>(texture);
		return *this;
	}
	if (!Has<impl::ButtonTextureToggled>()) {
		Add<impl::ButtonTextureToggled>(texture);
	} else {
		auto& c{ Get<impl::ButtonTextureToggled>() };
		c.Get(state) = texture;
	}
	return *this;
}

Color ToggleButton::GetTintToggled(ButtonState state) const {
	const auto c{ Has<impl::ButtonTintToggled>() ? Get<impl::ButtonTintToggled>()
												 : impl::ButtonTintToggled{} };
	return c.Get(state);
}

ToggleButton& ToggleButton::SetTintToggled(Color color, ButtonState state) {
	if (!Has<impl::ButtonTintToggled>()) {
		auto& c{ Add<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	} else {
		auto& c{ Get<impl::ButtonTintToggled>() };
		c.Get(state) = color;
	}
	return *this;
}

ToggleButtonGroup::ToggleButtonGroup(Entity entity) : Entity{ entity } {}

ToggleButton ToggleButtonGroup::Add(std::string_view button_key, ToggleButton toggle_button) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	toggle_button.Add<impl::ToggleButtonGroupKey>(button_key);

	if (auto it{ info.buttons.find(button_key) }; it == info.buttons.end()) {
		auto [new_it, inserted] = info.buttons.try_emplace(button_key, std::move(toggle_button));
		PTGN_ASSERT(inserted, "Failed to insert toggle button");
		ToggleButton btn{ new_it->second };
		AddToggleScript(btn);
		return btn;
	} else {
		it->second = GameObject{ std::move(toggle_button) };
		return ToggleButton{ it->second };
	}
}

void ToggleButtonGroup::Remove(std::string_view button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	auto it{ info.buttons.find(button_key) };

	if (it == info.buttons.end()) {
		return;
	}

	info.buttons.erase(it);
}

ToggleButton ToggleButtonGroup::GetActive() const {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	auto it{ info.buttons.find(info.active) };

	if (it == info.buttons.end() || !ToggleButton{ it->second }.IsToggled()) {
		return {};
	}

	return ToggleButton{ it->second };
}

void ToggleButtonGroup::SetActive(std::string_view button_key) {
	SetActiveKey(impl::ToggleButtonGroupKey{ button_key });
}

void ToggleButtonGroup::AddToggleScript(ToggleButton toggle_button) const {
	AddScript<impl::ToggleButtonGroupScript>(toggle_button, *this);
}

void ToggleButtonGroup::SetActiveKey(impl::ToggleButtonGroupKey key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	auto it{ info.buttons.find(key) };

	PTGN_ASSERT(
		it != info.buttons.end(), "Cannot set non-existent toggle button key to active: ", key
	);

	for (const auto& [_, toggle_button] : info.buttons) {
		ToggleButton{ toggle_button }.SetToggled(false);
	}

	info.active = key;

	ToggleButton{ it->second }.SetToggled(true);
}

Button CreateButton(Scene& scene) {
	Button button{ scene.CreateEntity() };

	Show(button, false);
	SetDraw<Button>(button);

	SetInteractive(button);
	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

Button CreateTextButton(Scene& scene, std::string_view text_content, Color text_color) {
	Button button{ CreateButton(scene) };

	button.SetText(text_content, text_color);

	return button;
}

ToggleButton CreateToggleButton(Scene& scene, bool toggled) {
	ToggleButton toggle_button{ CreateButton(scene) };

	AddScript<impl::ToggleButtonScript>(toggle_button);
	toggle_button.SetToggled(toggled);

	return toggle_button;
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup toggle_button_group{ scene.CreateEntity() };

	toggle_button_group.Entity::Add<impl::ToggleButtonGroupData>();

	return toggle_button_group;
}

// TODO: Fix.
// Entity CreateAnimatedButton(
//	Scene& scene, const V2_float& button_size, const Animation& activate_animation,
//	const Animation& hover_animation, bool force_start_on_activate, bool force_start_on_hover_start,
//	bool stop_on_hover_stop
//) {
//	auto button{ CreateButton(scene) };
//
//	if (activate_animation) {
//		// TODO: Change this once AddChild takes an Entity and not Entity&.
//		Entity activate{ activate_animation };
//		AddChild(button, activate, "activate_animation");
//	}
//	if (hover_animation) {
//		// TODO: Change this once AddChild takes an Entity and not Entity&.
//		Entity hover{ hover_animation };
//		AddChild(button, hover, "hover_animation");
//	}
//
//	SetSize(button, button_size);
//
//	AddScript<impl::AnimatedButtonScript>(
//		button, activate_animation, hover_animation, force_start_on_activate,
//		force_start_on_hover_start, stop_on_hover_stop
//	);
//
//	return button;
//}

} // namespace ptgn