#include "runtime/ui/button.h"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"

#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/interactive.h"

namespace ptgn {

namespace impl {

constexpr std::array<ButtonState, 3> kButtonStates{ ButtonState::Idle, ButtonState::Hover,
													ButtonState::Press };

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Entity button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(EventDispatcher d) {
	d.Dispatch<AnimationComplete>([this](const AnimationComplete&) mutable {
		if (button) {
			Button{ button }.PlayAnimation(ButtonState::Hover);
		}
	});
}

static void AddAnimationCompleteCallback(const Button& button, std::optional<GameObject>& child) {
	if (child.has_value()) {
		PTGN_ASSERT(
			!HasScript<ButtonAnimationCompleteScript>(*child),
			"Button animation cannot have the internal animation complete script more than once"
		);
		AddScript<ButtonAnimationCompleteScript>(*child, button);
	}
}

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
	const auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	if (state == IdleUp) {
		button.SetState(Hover);
		button.StartHover();
	} else if (state == IdleDown) {
		button.SetState(HoverPressed);
		button.StartHover();
	} else if (state == HeldOutside) {
		button.SetState(Pressed);
		return;
	}
	button.ContinueHover();
}

void InternalButtonScript::OnMouseMoveOut() {
	const auto& state{ entity.Get<InternalButtonState>() };
	Button button{ entity };
	if (!button.IsEnabled(true)) {
		return;
	}
	using enum InternalButtonState;
	if (state == Hover) {
		button.SetState(IdleUp);
		button.StopHover();
	} else if (state == Pressed) {
		button.SetState(HeldOutside);
		button.StopHover();
	} else if (state == HoverPressed) {
		button.SetState(IdleDown);
		button.StopHover();
	}
}

void InternalButtonScript::OnMousePressedOver(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		const auto& state{ entity.Get<InternalButtonState>() };
		using enum InternalButtonState;
		if (state == Hover) {
			button.SetState(Pressed);
		}
	}
}

void InternalButtonScript::OnMousePressedOut(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		const auto& state{ entity.Get<InternalButtonState>() };
		using enum InternalButtonState;
		if (state == IdleUp) {
			button.SetState(IdleDown);
		}
	}
}

void InternalButtonScript::OnMouseReleasedOver(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum InternalButtonState;
		const auto& state{ entity.Get<InternalButtonState>() };
		if (state == Pressed) {
			button.SetState(Hover);
			button.Activate();
		} else if (state == HoverPressed) {
			button.SetState(Hover);
		}
	}
}

void InternalButtonScript::OnMouseReleasedOut(Mouse mouse) {
	Button button{ entity };
	if (!button.IsEnabled(false)) {
		return;
	}
	if (mouse == Mouse::Left) {
		using enum InternalButtonState;
		const auto& state{ entity.Get<InternalButtonState>() };
		if (state == IdleDown || state == HeldOutside) {
			button.SetState(IdleUp);
		}
	}
}

void InternalToggleButtonScript::OnEvent(EventDispatcher d) {
	d.Dispatch<ButtonActivate>([this](const ButtonActivate&) { OnButtonActivate(); });
}

void InternalToggleButtonScript::OnButtonActivate() const {
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

template <typename Derived>
ButtonBase<Derived>::ConstButtonStyles ButtonBase<Derived>::GetStyle(ButtonStyleState state) const {
	PTGN_ASSERT(Has<ButtonConfig>(), "Button must have a valid config");

	ButtonInteractionConfig& backup_config{ Get<ButtonConfig>().enabled };
	const ButtonInteractionConfig* config{ nullptr };

	if (state.toggled) {
		PTGN_ASSERT(
			Has<impl::ToggleButtonInteractionConfig>(),
			"Toggle button must have a toggle interaction config"
		);
		config = &Get<impl::ToggleButtonInteractionConfig>().toggled;
	} else {
		config = state.disabled ? &Get<ButtonConfig>().disabled : &backup_config;
	}

	PTGN_ASSERT(config != nullptr, "Failed to find button style config");

	switch (state.state) {
		using enum ButtonState;
		case Idle: {
			return { backup_config.idle, config->idle, config->idle };
		}
		case Hover: {
			return { backup_config.idle, config->idle, config->hover };
		}
		case Press: {
			return { backup_config.idle, config->idle, config->activate };
		}
		default: {
			ButtonStyleState current_state;
			current_state.state	   = GetState();
			current_state.toggled  = state.toggled;
			current_state.disabled = state.disabled;
			PTGN_ASSERT(
				current_state.state != ButtonState::Current,
				"GetStyle recursive call does not support ButtonState::Current"
			);
			return GetStyle(current_state);
		}
	}
}

template <typename Derived>
ButtonBase<Derived>::ButtonStyles ButtonBase<Derived>::GetStyle(ButtonStyleState state) {
	auto [enabled_idle, idle, desired] = std::as_const(*this).GetStyle(state);
	return { const_cast<ButtonStyle&>(enabled_idle), const_cast<ButtonStyle&>(idle),
			 const_cast<ButtonStyle&>(desired) };
}

template <typename Derived>
void ButtonBase<Derived>::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	Button button{ entity };
	Color entity_tint{ ptgn::GetTint(button) };

	if (entity_tint.a == 0) {
		return;
	}

	auto style_state = button.GetStyleState();

	if (button.Has<InteractionLock>()) {
		style_state.state = ButtonState::Press;
	}

	Tint button_tint{ button.GetTint(style_state) };

	Tint tint{ entity_tint.Normalized() * button_tint.Normalized() };

	if (tint.a == 0) {
		return;
	}

	auto transform{ GetDrawTransform(button) };
	auto depth{ GetDepth(button) };
	auto blend_mode{ GetBlendMode(button) };
	auto button_origin{ GetDrawOrigin(button) };

	std::optional<V2_float> button_size;

	auto sprite_state{ style_state };
	// If we want to prevent the press state animation from playing, we can do this:
	// if (!button.Has<InteractionLock>() && sprite_state.state == ButtonState::Press) {
	//	sprite_state.state = ButtonState::Hover;
	//}
	if (auto sprite{ button.GetSprite(sprite_state) }; sprite.has_value()) {
		button_size = GetDisplaySize(*sprite);
		Sprite::Draw(renderer, *sprite, camera, tint);
	}

	auto background_shape{ button.GetBackgroundShape(style_state) };
	auto bg_fill_style{ button.GetBackgroundFillStyle(style_state) };

	if (auto bg_color{ button.GetBackgroundColor(style_state) }; background_shape.has_value() ||
																 bg_fill_style.has_value() ||
																 bg_color != color::Transparent) {
		FillStyle fill{ bg_fill_style.value_or(FillStyle::Solid()) };
		Tint color{ bg_color.Normalized() * tint.Normalized() };
		if (!background_shape.has_value()) {
			if (button.Has<Rect>()) {
				background_shape = button.Get<Rect>();
			} else if (button.Has<Circle>()) {
				background_shape = button.Get<Circle>();
			}
		}
		if (background_shape.has_value()) {
			std::visit(
				[&]<typename T>(const T& shape) {
					if (!button_size.has_value()) {
						if constexpr (std::is_same_v<T, Rect>) {
							button_size = shape.GetSize(transform);
						} else if constexpr (std::is_same_v<T, Circle>) {
							button_size = shape.GetSize(transform);
						} else {
							static_assert(false, "Unsupported button shape type");
						}
					}
					renderer.DrawShape(
						shape, transform, color, fill, button_origin, depth, blend_mode
					);
				},
				*background_shape
			);
		}
	}

	auto border_shape{ button.GetBackgroundShape(style_state) };
	auto border_width{ button.GetBorderWidth(style_state) };

	if (auto border_color{ button.GetBorderColor(style_state) };
		border_shape.has_value() || border_width.has_value() ||
		border_color != color::Transparent) {
		PTGN_ASSERT(
			!border_width.has_value() || border_width.has_value() && *border_width >= 0.0f,
			"Invalid button border width"
		);
		if (!border_width.has_value()) {
			border_width = kMinLineWidth;
		}
		PTGN_ASSERT(border_width.has_value());
		if (*border_width >= kMinLineWidth) {
			FillStyle fill{ FillStyle::Hollow(*border_width) };
			Tint color{ border_color.Normalized() * tint.Normalized() };
			if (!border_shape.has_value()) {
				if (button.Has<Rect>()) {
					border_shape = button.Get<Rect>();
				} else if (button.Has<Circle>()) {
					border_shape = button.Get<Circle>();
				}
			}
			if (border_shape.has_value()) {
				std::visit(
					[&](const auto& shape) {
						renderer.DrawShape(
							shape, transform, color, fill, button_origin, depth, blend_mode
						);
					},
					*border_shape
				);
			}
		}
	}

	if (!button_size.has_value()) {
		if (auto rect{ button.TryGet<Rect>() }) {
			button_size = rect->GetSize(transform);
		} else if (auto circle{ button.TryGet<Circle>() }) {
			button_size = circle->GetSize(transform);
		}
	}

	if (auto text{ button.GetText(style_state) }; text.has_value()) {
		V2_float text_size;

		if (auto fixed_size{ button.GetTextFixedSize() }; fixed_size.has_value()) {
			if (!fixed_size->x.has_value()) {
				PTGN_ASSERT(button_size.has_value());
				PTGN_ASSERT(button_size->x != 0.0f);
				text_size.x = button_size->x;
			} else {
				text_size.x = *fixed_size->x;
			}

			if (!fixed_size->y.has_value()) {
				PTGN_ASSERT(button_size.has_value());
				PTGN_ASSERT(button_size->y != 0.0f);
				text_size.y = button_size->y;
			} else {
				text_size.y = *fixed_size->y;
			}
		}

		PTGN_ASSERT(button_size.has_value());
		V2_float offset{ *button_size };

		Text::Draw(renderer, *text, text_size, tint, button_origin, offset, camera);
	}
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
		SetState(impl::InternalButtonState::IdleUp);
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
std::optional<std::variant<Rect, Circle>> ButtonBase<Derived>::GetShape() const {
	const auto& config{ Get<ButtonConfig>() };

	if (auto rect{ TryGet<Rect>() }) {
		return *rect;
	} else if (auto circle{ TryGet<Circle>() }) {
		return *circle;
	} else if (config.enabled.idle.sprite.has_value()) {
		auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.sprite) };
		PTGN_ASSERT(texture_size.has_value(), "No valid texture size for button");
		return Rect{ *texture_size };
	} else if (config.enabled.idle.text.has_value()) {
		auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.text) };
		PTGN_ASSERT(texture_size.has_value(), "No valid text size for button");
		return Rect{ *texture_size };
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::RemoveShape() {
	Remove<Circle>();
	Remove<Rect>();
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetShape(const std::optional<std::variant<Rect, Circle>>& shape) {
	std::optional<std::variant<Rect, Circle>> resolved_shape;

	if (!shape.has_value()) {
		const auto& config{ Get<ButtonConfig>() };
		if (config.enabled.idle.sprite.has_value()) {
			auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.sprite) };
			PTGN_ASSERT(texture_size.has_value(), "No valid texture size for button");
			resolved_shape = Rect{ *texture_size };
		} else if (config.enabled.idle.text.has_value()) {
			auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.text) };
			PTGN_ASSERT(texture_size.has_value(), "No valid text size for button");
			resolved_shape = Rect{ *texture_size };
		}
	} else {
		resolved_shape = *shape;
	}
	if (resolved_shape.has_value()) {
		std::visit([&]<typename T>(const T& arg) { Add<T>(arg); }, *resolved_shape);
	}
	return Self();
}

template <typename Derived>
std::optional<Audio> ButtonBase<Derived>::GetSound(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.sound.has_value()) {
		return *desired.sound;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetSound(std::optional<AudioOrKey> sound, ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);

	if (!sound.has_value()) {
		desired.sound = std::nullopt;
		return Self();
	}

	const auto& scene{ GetScene() };

	const auto& assets{ scene.ctx().asset };
	Audio resolved_sound{ sound->Get(assets) };

	desired.sound = resolved_sound;

	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetAnimation(Animation&& animation, ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);
	Hide(animation);
	SetParent(animation, *this);
	desired.sprite = GameObject{ std::move(animation) };
	impl::AddAnimationCompleteCallback(Button{ *this }, desired.sprite);
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::RemoveAnimation(ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);
	if (desired.sprite.has_value()) {
		desired.sprite->Remove<AnimationData>();
	}
	return Self();
}

template <typename Derived>
std::optional<Animation> ButtonBase<Derived>::GetAnimation(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.sprite.has_value() && desired.sprite->Has<AnimationData>()) {
		return Animation{ *desired.sprite };
	}
	return std::nullopt;
}

template <typename Derived>
Color ButtonBase<Derived>::GetBackgroundColor(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.background_color.has_value()) {
		return *desired.background_color;
	} else if (idle.background_color.has_value()) {
		return *idle.background_color;
	} else if (enabled_idle.background_color.has_value()) {
		return *enabled_idle.background_color;
	} else {
		return color::Transparent;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundColor(Color color, ButtonStyleState state) {
	auto [_1, _2, desired]	 = GetStyle(state);
	desired.background_color = color;
	return Self();
}

template <typename Derived>
void ButtonBase<Derived>::SetText(
	GameObject& text, std::string_view text_content, std::optional<Color> text_color,
	FontSize font_size, FontOrKey font, const TextProperties& text_properties
) {
	auto& scene{ GetScene() };

	text = GameObject{ CreateText(
		scene, text_content, text_color.value_or(kDefaultButtonTextColor), font_size, font,
		text_properties
	) };

	Hide(text);
	SetParent(text, *this);
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetText(
	std::string_view text_content, Color text_color, FontSize font_size, FontOrKey font,
	const TextProperties& text_properties, ButtonStyleState state
) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		const auto& scene{ GetScene() };
		const auto& assets{ scene.ctx().asset };

		Font resolved_font{ font.Get(assets) };

		Text::SetParameter(*desired.text, TextColor{ text_color }, false);
		Text::SetParameter(*desired.text, TextContent{ text_content }, false);
		Text::SetParameter(*desired.text, resolved_font, false);
		Text::SetParameter(*desired.text, font_size, false);
		Text::SetProperties(*desired.text, text_properties, true);
	} else {
		desired.text = GameObject{};
		SetText(*desired.text, text_content, text_color, font_size, font, text_properties);
	}
	return Self();
}

template <typename Derived>
std::optional<Text> ButtonBase<Derived>::GetText(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		return Text{ *desired.text };
	} else if (idle.text.has_value()) {
		return Text{ *idle.text };
	} else if (enabled_idle.text.has_value()) {
		return Text{ *enabled_idle.text };
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
std::optional<Color> ButtonBase<Derived>::GetTextColor(ButtonStyleState state) const {
	if (auto text{ GetText(state) }) {
		return text->GetColor();
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextColor(Color text_color, ButtonStyleState state) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		Text::SetParameter(*desired.text, impl::TextColor{ text_color }, true);
	} else {
		desired.text = GameObject{};
		SetText(*desired.text, {}, text_color);
	}
	return Self();
}

template <typename Derived>
std::optional<std::string> ButtonBase<Derived>::GetTextContent(ButtonStyleState state) const {
	if (auto text{ GetText(state) }) {
		return text->GetContent();
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextContent(
	std::string_view text_content, ButtonStyleState state
) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		Text::SetParameter(*desired.text, TextContent{ text_content }, true);
	} else {
		desired.text = GameObject{};
		SetText(*desired.text, text_content);
	}
	return Self();
}

template <typename Derived>
std::optional<TextJustify> ButtonBase<Derived>::GetTextJustify(ButtonStyleState state) const {
	if (auto text{ GetText(state) }) {
		return text->GetJustify();
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextJustify(TextJustify justify, ButtonStyleState state) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		Text::SetParameter(*desired.text, justify, true);
	} else {
		TextProperties text_properties;
		text_properties.justify = justify;
		desired.text			= GameObject{};
		SetText(*desired.text, {}, {}, {}, {}, text_properties);
	}
	return Self();
}

template <typename Derived>
std::optional<ButtonTextFixedSize> ButtonBase<Derived>::GetTextFixedSize(ButtonStyleState state
) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text_fixed_size.has_value()) {
		return desired.text_fixed_size;
	} else if (idle.text_fixed_size.has_value()) {
		return idle.text_fixed_size;
	} else if (enabled_idle.text_fixed_size.has_value()) {
		return enabled_idle.text_fixed_size;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextFixedSize(
	std::optional<ButtonTextFixedSize> size, ButtonStyleState state
) {
	auto [_1, _2, desired]	= GetStyle(state);
	desired.text_fixed_size = size;
	return Self();
}

template <typename Derived>
std::optional<FontSize> ButtonBase<Derived>::GetFontSize(ButtonStyleState state) const {
	if (auto text{ GetText(state) }) {
		return text->GetFontSize();
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetFontSize(FontSize font_size, ButtonStyleState state) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.text.has_value()) {
		Text::SetParameter(*desired.text, font_size, true);
	} else {
		desired.text = GameObject{};
		SetText(*desired.text, {}, {}, font_size);
	}
	return Self();
}

template <typename Derived>
std::optional<Entity> ButtonBase<Derived>::GetSprite(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.sprite.has_value()) {
		return desired.sprite;
	} else if (idle.sprite.has_value()) {
		return idle.sprite;
	} else if (enabled_idle.sprite.has_value()) {
		return enabled_idle.sprite;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
std::optional<Texture> ButtonBase<Derived>::GetTexture(ButtonStyleState state) const {
	auto sprite{ GetSprite(state) };
	if (sprite.has_value()) {
		PTGN_ASSERT(sprite->Has<Texture>(), "Button sprite must have a texture");
		return sprite->Get<Texture>();
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTexture(TextureOrKey texture, ButtonStyleState state) {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.sprite.has_value()) {
		Sprite{ *desired.sprite }.SetTexture(texture);
	} else {
		auto& scene{ GetScene() };
		desired.sprite = GameObject{ CreateSprite(scene, texture) };
		Hide(*desired.sprite);
		SetParent(*desired.sprite, *this);
	}
	return Self();
}

template <typename Derived>
Color ButtonBase<Derived>::GetTint(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.tint.has_value()) {
		return *desired.tint;
	} else if (idle.tint.has_value()) {
		return *idle.tint;
	} else if (enabled_idle.tint.has_value()) {
		return *enabled_idle.tint;
	} else {
		return Tint{};
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTint(Color color, ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);
	desired.tint		   = color;
	return Self();
}

template <typename Derived>
Color ButtonBase<Derived>::GetBorderColor(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.border_color.has_value()) {
		return *desired.border_color;
	} else if (idle.border_color.has_value()) {
		return *idle.border_color;
	} else if (enabled_idle.border_color.has_value()) {
		return *enabled_idle.border_color;
	} else {
		return color::Transparent;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderColor(Color color, ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);
	desired.border_color   = color;
	return Self();
}

template <typename Derived>
std::optional<FillStyle> ButtonBase<Derived>::GetBackgroundFillStyle(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.background_fill.has_value()) {
		return *desired.background_fill;
	} else if (idle.background_fill.has_value()) {
		return *idle.background_fill;
	} else if (enabled_idle.background_fill.has_value()) {
		return *enabled_idle.background_fill;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundFillStyle(FillStyle fill_style, ButtonStyleState state) {
	auto [_1, _2, desired]	= GetStyle(state);
	desired.background_fill = fill_style;
	return Self();
}

template <typename Derived>
std::optional<float> ButtonBase<Derived>::GetBorderWidth(ButtonStyleState state) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.border_width.has_value()) {
		return *desired.border_width;
	} else if (idle.border_width.has_value()) {
		return *idle.border_width;
	} else if (enabled_idle.border_width.has_value()) {
		return *enabled_idle.border_width;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderWidth(float line_width, ButtonStyleState state) {
	auto [_1, _2, desired] = GetStyle(state);
	desired.border_width   = line_width;
	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetExclusiveAudio(bool enabled) {
	if (enabled) {
		Add<impl::ButtonExclusiveAudio>();
	} else {
		Remove<impl::ButtonExclusiveAudio>();
	}
	return Self();
}

template <typename Derived>
impl::InternalButtonState ButtonBase<Derived>::GetInternalState() const {
	return Get<impl::InternalButtonState>();
}

template <typename Derived>
std::optional<std::variant<Rect, Circle>> ButtonBase<Derived>::GetBackgroundShape(
	ButtonStyleState state
) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.background_shape.has_value()) {
		return *desired.background_shape;
	} else if (idle.background_shape.has_value()) {
		return *idle.background_shape;
	} else if (enabled_idle.background_shape.has_value()) {
		return *enabled_idle.background_shape;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundShape(
	std::optional<std::variant<Rect, Circle>> shape, ButtonStyleState state
) {
	auto [_1, _2, desired]	 = GetStyle(state);
	desired.background_shape = shape;
	return Self();
}

template <typename Derived>
std::optional<std::variant<Rect, Circle>> ButtonBase<Derived>::GetBorderShape(ButtonStyleState state
) const {
	auto [enabled_idle, idle, desired] = GetStyle(state);
	if (desired.border_shape.has_value()) {
		return *desired.border_shape;
	} else if (idle.border_shape.has_value()) {
		return *idle.border_shape;
	} else if (enabled_idle.border_shape.has_value()) {
		return *enabled_idle.border_shape;
	} else {
		return std::nullopt;
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderShape(
	std::optional<std::variant<Rect, Circle>> shape, ButtonStyleState state
) {
	auto [_1, _2, desired] = GetStyle(state);
	desired.border_shape   = shape;
	return Self();
}

template <typename Derived>
ButtonState ButtonBase<Derived>::GetState() const {
	PTGN_ASSERT(Has<impl::InternalButtonState>());
	const auto& state{ Get<impl::InternalButtonState>() };
	using enum impl::InternalButtonState;
	if (state == Hover || state == HoverPressed) {
		return ButtonState::Hover;
	} else if (state == Pressed || state == HeldOutside) {
		return ButtonState::Press;
	} else {
		return ButtonState::Idle;
	}
}

template <typename Derived>
ButtonStyleState ButtonBase<Derived>::GetStyleState() const {
	auto state{ GetState() };
	auto disabled{ !IsEnabled(false) };
	auto toggled{ Has<ButtonToggled>() && Has<ToggleButtonInteractionConfig>() };
	return { state, disabled, toggled };
}

template <typename Derived>
void ButtonBase<Derived>::PlaySound(ButtonState active) {
	auto s{ GetStyleState() };

	auto& scene{ GetScene() };
	AudioSystem& audio_system{ scene.ctx().audio };

	bool exclusive_audio{ Has<ButtonExclusiveAudio>() };

	s.state = active;

	// Only stop other sounds if exclusive audio and there is a sound to play for the active state.
	bool stop_others{ exclusive_audio && GetSound(s).has_value() };

	for (auto state : kButtonStates) {
		s.state = state;

		auto sound{ GetSound(s) };

		if (!sound.has_value()) {
			continue;
		}

		if (state == active) {
			audio_system.Play(*sound);
		} else if (stop_others) {
			audio_system.Stop(*sound);
		}
	}
}

template <typename Derived>
void ButtonBase<Derived>::PlayAnimation(ButtonState active) {
	auto s{ GetStyleState() };

	constexpr bool stop_others{ true };

	for (auto state : kButtonStates) {
		s.state = state;

		auto animation = GetAnimation(s);

		if (!animation.has_value()) {
			continue;
		}
		if (state == active) {
			animation->Start(true);
		} else if (stop_others) {
			animation->Reset();
		}
	}
}

template <typename Derived>
Derived& ButtonBase<Derived>::Activate() {
	if (!IsEnabled(false) || Has<InteractionLock>()) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonActivate event;
		scripts->Emit(event);
	}

	auto state{ GetStyleState() };
	state.state = ButtonState::Press;
	if (auto animation = GetAnimation(state); animation.has_value()) {
		Add<InteractionLock>(InteractionLock{ .remaining_time = animation->GetDuration(),
											  .block_hover	  = false,
											  .block_click	  = true });
		PlayAnimation(ButtonState::Press);
	}

	PlaySound(ButtonState::Press);

	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::StartHover() {
	if (!IsEnabled(true) || Has<InteractionLock>()) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStart event;
		scripts->Emit(event);
	}

	PlaySound(ButtonState::Hover);
	PlayAnimation(ButtonState::Hover);

	return Self();
}

template <typename Derived>
Derived& ButtonBase<Derived>::ContinueHover() {
	if (!IsEnabled(true) || Has<InteractionLock>()) {
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
	if (!IsEnabled(true) || Has<InteractionLock>()) {
		return Self();
	}
	if (auto scripts{ TryGet<impl::Scripts>() }) {
		impl::ButtonHoverStop event;
		scripts->Emit(event);
	}

	PlaySound(ButtonState::Idle);
	PlayAnimation(ButtonState::Idle);

	return Self();
}

template <typename Derived>
void ButtonBase<Derived>::SetState(InternalButtonState new_state) {
	auto& state = Get<InternalButtonState>();

	if (state == new_state) {
		return;
	}

	InternalButtonState old_state = state;
	state						  = new_state;

	OnStateChange(old_state, new_state);
}

template <typename Derived>
void ButtonBase<Derived>::OnStateChange(InternalButtonState, InternalButtonState) {
	/* No-op currently */
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
template class ButtonBase<Dropdown>;

} // namespace impl

ToggleButton::operator Button() const {
	return Button{ *this };
}

bool ToggleButton::IsToggled() const {
	return Has<impl::ButtonToggled>();
}

ToggleButton& ToggleButton::OnToggle(const std::function<void(bool)>& callback) {
	AddScript<impl::ButtonToggleScript>(*this, callback);
	return *this;
}

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

ToggleButtonGroup::ToggleButtonGroup(Entity entity) : Entity{ entity } {}

void ToggleButtonGroup::SetAlwaysOneActive(
	bool always_active, std::optional<std::string_view> button_key
) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());
	auto& info{ Get<impl::ToggleButtonGroupData>() };
	info.always_active = always_active;
	if (info.always_active) {
		// In the past, I had it so that if there is already an active button, then there is no need
		// to set an active button, but I find that more confusing.
		// if (info.active.has_value()) {
		//	return;
		//}

		impl::ToggleButtonGroupKey key{};
		if (button_key.has_value()) {
			PTGN_ASSERT(
				std::ranges::contains(
					info.buttons, impl::ToggleButtonGroupKey{ *button_key },
					&std::pair<impl::ToggleButtonGroupKey, GameObject>::first
				),
				"Cannot set always active button key until it has been added to the toggle button "
				"group"
			);
			key = *button_key;
		} else {
			if (info.buttons.empty()) {
				return;
			}
			key = info.buttons.front().first;
		}
		SetActiveKey(key);
	}
}

ToggleButton ToggleButtonGroup::Add(std::string_view button_key, ToggleButton&& toggle_button) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	impl::ToggleButtonGroupKey key{ button_key };

	RemoveScript<impl::InternalToggleButtonScript>(toggle_button);
	toggle_button.Add<impl::ToggleButtonGroupKey>(key);

	auto it = std::ranges::find(
		info.buttons, key, &std::pair<impl::ToggleButtonGroupKey, GameObject>::first
	);

	ToggleButton btn;

	if (it == info.buttons.end()) {
		info.buttons.emplace_back(key, std::move(toggle_button));
		const auto& obj = info.buttons.back().second;

		btn = ToggleButton{ obj };
		AddToggleScript(btn);
	} else {
		it->second = GameObject{ std::move(toggle_button) };
		AddToggleScript(ToggleButton{ it->second });
		btn = ToggleButton{ it->second };
	}

	// If always active is enabled, there must always be an active button, so if there is still no
	// active button, set the first button to active.
	if (info.always_active && !info.active.has_value() && info.buttons.size() == 1) {
		SetActiveKey(key);
	}

	return btn;
}

void ToggleButtonGroup::Remove(std::string_view button_key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };
	impl::ToggleButtonGroupKey key{ button_key };

	auto it = std::ranges::find(
		info.buttons, key, &std::pair<impl::ToggleButtonGroupKey, GameObject>::first
	);

	if (it != info.buttons.end()) {
		PTGN_ASSERT(
			!HasScript<impl::InternalToggleButtonScript>(it->second),
			"When removing a toggle button from the group, it must not already have the internal "
			"toggle button script as it is part of a group: logic error somewhere"
		);
		AddScript<impl::InternalToggleButtonScript>(it->second);
		info.buttons.erase(it);
	}
}

std::optional<ToggleButton> ToggleButtonGroup::GetActive() const {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	if (!info.active.has_value()) {
		return {};
	}

	auto it = std::ranges::find(
		info.buttons, info.active, &std::pair<impl::ToggleButtonGroupKey, GameObject>::first
	);

	if (it == info.buttons.end()) {
		return {};
	}

	PTGN_ASSERT(
		ToggleButton{ it->second }.IsToggled(),
		"Active toggle button should always be toggled: If not, some function is incorrect "
		"changing button states"
	);

	return ToggleButton{ it->second };
}

void ToggleButtonGroup::SetActive(std::string_view button_key) {
	SetActiveKey(impl::ToggleButtonGroupKey{ button_key });
}

void ToggleButtonGroup::AddToggleScript(ToggleButton toggle_button) const {
	PTGN_ASSERT(
		!HasScript<impl::ToggleButtonGroupScript>(toggle_button),
		"Attempting to add toggle button group script to a button more than once"
	);
	AddScript<impl::ToggleButtonGroupScript>(toggle_button, *this);
}

void ToggleButtonGroup::SetActiveKey(impl::ToggleButtonGroupKey key) {
	PTGN_ASSERT(Has<impl::ToggleButtonGroupData>());

	auto& info{ Get<impl::ToggleButtonGroupData>() };

	bool same_as_current{ info.active == key };

	info.active = key;

	auto it = std::ranges::find(
		info.buttons, info.active, &std::pair<impl::ToggleButtonGroupKey, GameObject>::first
	);

	PTGN_ASSERT(
		it != info.buttons.end(),
		"Cannot set non-existent toggle button key to active: ", *info.active
	);

	const auto& active_button{ it->second };

	for (const auto& [_, button] : info.buttons) {
		if (!info.always_active && same_as_current) {
			ToggleButton{ button }.SetToggled(false);
			info.active.reset();
			continue;
		}
		bool is_active{ button == active_button };
		ToggleButton{ button }.SetToggled(is_active);
	}
}

static void ProcessButtonChild(Button button, std::optional<GameObject>& child) {
	if (child.has_value()) {
		Hide(*child);
		SetParent(*child, button);
	}
}

Button CreateButton(
	Scene& scene, const std::optional<std::variant<Rect, Circle>>& shape, ButtonConfig config,
	bool ui_layer
) {
	Button button{ scene.CreateEntity() };

	ProcessButtonChild(button, config.enabled.idle.sprite);
	ProcessButtonChild(button, config.enabled.idle.text);
	ProcessButtonChild(button, config.enabled.hover.sprite);
	ProcessButtonChild(button, config.enabled.hover.text);
	ProcessButtonChild(button, config.enabled.activate.sprite);
	ProcessButtonChild(button, config.enabled.activate.text);
	ProcessButtonChild(button, config.disabled.idle.sprite);
	ProcessButtonChild(button, config.disabled.idle.text);
	ProcessButtonChild(button, config.disabled.hover.sprite);
	ProcessButtonChild(button, config.disabled.hover.text);
	ProcessButtonChild(button, config.disabled.activate.sprite);
	ProcessButtonChild(button, config.disabled.activate.text);

	impl::AddAnimationCompleteCallback(button, config.enabled.activate.sprite);
	impl::AddAnimationCompleteCallback(button, config.disabled.activate.sprite);

	button.Add<ButtonConfig>(std::move(config));

	if (ui_layer) {
		SetUI(button, true);
	}

	Show(button, false);
	SetDraw<Button>(button);
	button.SetShape(shape);

	SetInteractive(button);

	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	PTGN_ASSERT(!HasScript<impl::InternalButtonScript>(button));
	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

ToggleButton CreateToggleButton(
	Scene& scene, const std::optional<std::variant<Rect, Circle>>& shape, ToggleButtonConfig config,
	bool toggled
) {
	ButtonConfig button_config;
	button_config.enabled  = std::move(config.enabled);
	button_config.disabled = std::move(config.disabled);

	ButtonInteractionConfig toggle_config{ std::move(config.toggled) };

	Button button{ CreateButton(scene, shape, std::move(button_config)) };

	ToggleButton toggle_button{ button };

	ProcessButtonChild(toggle_button, toggle_config.idle.sprite);
	ProcessButtonChild(toggle_button, toggle_config.idle.text);
	ProcessButtonChild(toggle_button, toggle_config.hover.sprite);
	ProcessButtonChild(toggle_button, toggle_config.hover.text);
	ProcessButtonChild(toggle_button, toggle_config.activate.sprite);
	ProcessButtonChild(toggle_button, toggle_config.activate.text);

	impl::AddAnimationCompleteCallback(button, toggle_config.activate.sprite);

	toggle_button.Add<impl::ToggleButtonInteractionConfig>(std::move(toggle_config));

	PTGN_ASSERT(!HasScript<impl::InternalToggleButtonScript>(toggle_button));
	AddScript<impl::InternalToggleButtonScript>(toggle_button);
	toggle_button.SetToggled(toggled);

	return toggle_button;
}

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene) {
	ToggleButtonGroup toggle_button_group{ scene.CreateEntity() };

	toggle_button_group.Entity::Add<impl::ToggleButtonGroupData>();

	return toggle_button_group;
}

} // namespace ptgn