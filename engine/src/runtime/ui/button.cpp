#include "runtime/ui/button.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/input/mouse.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/toggle_button.h"

namespace ptgn {

namespace {

std::optional<Entity> FindButtonPart(Button button, ButtonPartRole role, ButtonVisualState state) {
	if (!HasChildren(button)) {
		return std::nullopt;
	}

	for (Entity child : GetChildren(button)) {
		auto part{ child.TryGet<impl::ButtonPart>() };
		if (!part) {
			continue;
		}

		if (part->role == role && part->state == state) {
			return child;
		}
	}

	return std::nullopt;
}

std::vector<Entity> FindButtonParts(Button button, std::optional<ButtonPartRole> role = {}) {
	std::vector<Entity> parts;

	if (!HasChildren(button)) {
		return parts;
	}

	for (Entity child : GetChildren(button)) {
		auto part{ child.TryGet<impl::ButtonPart>() };
		if (!part) {
			continue;
		}

		if (role.has_value() && part->role != *role) {
			continue;
		}

		parts.emplace_back(child);
	}

	return parts;
}

bool IsPartVisibleForState(const impl::ButtonPart& part, ButtonVisualState active_state) {
	if (part.state == ButtonVisualState::Base) {
		return true;
	}

	return part.state == active_state;
}

ButtonVisualState DisabledStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Disabled;
		case Hover: return ButtonVisualState::DisabledHover;
		case Press: return ButtonVisualState::DisabledPress;
	}

	return ButtonVisualState::Disabled;
}

ButtonVisualState ToggledStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Toggled;
		case Hover: return ButtonVisualState::ToggledHover;
		case Press: return ButtonVisualState::ToggledPress;
	}

	return ButtonVisualState::Toggled;
}

ButtonVisualState NormalStateFrom(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Idle;
		case Hover: return ButtonVisualState::Hover;
		case Press: return ButtonVisualState::Press;
	}

	return ButtonVisualState::Idle;
}

std::optional<V2_float> GetShapeSize(const std::variant<Rect, Circle>& shape) {
	return std::visit(
		[](const auto& value) -> std::optional<V2_float> { return value.GetSize(); }, shape
	);
}

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Entity button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	// Keep this if your AnimationComplete event still needs to restart hover animations.
	// Otherwise this script can be removed entirely in the child-entity model.
}

void ButtonScript::OnEvent(Event event) {
	using namespace ptgn::event;

	event.Dispatch<MouseMoveOver>(&ButtonScript::OnMouseMoveOver, this);
	event.Dispatch<MouseMoveOut>(&ButtonScript::OnMouseMoveOut, this);
	event.Dispatch<MousePressedOver>(&ButtonScript::OnMousePressedOver, this);
	event.Dispatch<MousePressedOut>(&ButtonScript::OnMousePressedOut, this);
	event.Dispatch<MouseReleasedOver>(&ButtonScript::OnMouseReleasedOver, this);
	event.Dispatch<MouseReleasedOut>(&ButtonScript::OnMouseReleasedOut, this);
}

void ButtonScript::OnMouseMoveOver() {
	Button button{ entity };

	if (!button.IsEnabled(true)) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

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

void ButtonScript::OnMouseMoveOut() {
	Button button{ entity };

	if (!button.IsEnabled(true)) {
		return;
	}

	auto state{ button.GetInternalState() };

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

void ButtonScript::OnMousePressedOver(Mouse mouse) {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	if (button.GetInternalState() == InternalButtonState::Hover) {
		button.SetState(InternalButtonState::Pressed);
	}
}

void ButtonScript::OnMousePressedOut(Mouse mouse) {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	if (button.GetInternalState() == InternalButtonState::IdleUp) {
		button.SetState(InternalButtonState::IdleDown);
	}
}

void ButtonScript::OnMouseReleasedOver(Mouse mouse) {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == Pressed) {
		button.SetState(Hover);
		button.Press();
	} else if (state == HoverPressed) {
		button.SetState(Hover);
	}
}

void ButtonScript::OnMouseReleasedOut(Mouse mouse) {
	Button button{ entity };

	if (!button.IsEnabled(false) || mouse != Mouse::Left) {
		return;
	}

	auto state{ button.GetInternalState() };

	using enum InternalButtonState;

	if (state == IdleDown || state == HeldOutside) {
		button.SetState(IdleUp);
	}
}

} // namespace impl

Button::Button(Entity entity) : Entity{ entity } {}

void Button::Draw(DrawContext&, Entity entity) {
	Button button{ entity };

	button.UpdateChildLayouts();
	button.RefreshVisualState();
}

bool Button::IsEnabled(bool check_for_hover_enabled) const {
	auto enabled{ TryGet<impl::ButtonEnabled>() };
	if (!enabled) {
		return false;
	}

	if (check_for_hover_enabled) {
		return enabled->hover;
	}

	return enabled->press;
}

ButtonState Button::GetState() const {
	auto state{ GetInternalState() };

	using enum impl::InternalButtonState;

	if (state == Hover || state == HoverPressed) {
		return ButtonState::Hover;
	}

	if (state == Pressed || state == HeldOutside) {
		return ButtonState::Press;
	}

	return ButtonState::Idle;
}

ButtonVisualState Button::GetVisualState() const {
	auto state{ GetState() };

	if (!IsEnabled(false)) {
		return DisabledStateFrom(state);
	}

	if (HasToggle() && AsToggle().IsToggled()) {
		return ToggledStateFrom(state);
	}

	return NormalStateFrom(state);
}

impl::InternalButtonState Button::GetInternalState() const {
	return Get<impl::ButtonData>().state;
}

std::optional<std::variant<Rect, Circle>> Button::GetShape() const {
	if (auto rect{ TryGet<Rect>() }) {
		return *rect;
	}

	if (auto circle{ TryGet<Circle>() }) {
		return *circle;
	}

	return std::nullopt;
}

Button& Button::Enable(bool enable_hover, bool reset_state) {
	return SetEnabled(true, enable_hover, reset_state);
}

Button& Button::Disable(bool disable_hover, bool reset_state) {
	return SetEnabled(false, !disable_hover, reset_state);
}

Button& Button::SetEnabled(bool enable_activation, bool enable_hover, bool reset_state) {
	Add<impl::ButtonEnabled>(enable_activation, enable_hover);

	if (reset_state) {
		SetState(impl::InternalButtonState::IdleUp);
	}

	return *this;
}

Button& Button::Press() {
	if (!IsEnabled(false)) {
		return *this;
	}

	PlaySound(ButtonState::Press);
	PlayAnimation(ButtonState::Press);

	PushEvent<event::ButtonPress>(*this, *this);

	return *this;
}

Button& Button::StartHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PlaySound(ButtonState::Hover);
	PlayAnimation(ButtonState::Hover);

	PushEvent<event::ButtonHoverStart>(*this, *this);

	return *this;
}

Button& Button::ContinueHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PushEvent<event::ButtonHover>(*this, *this);

	return *this;
}

Button& Button::StopHover() {
	if (!IsEnabled(true)) {
		return *this;
	}

	PlaySound(ButtonState::Idle);
	PlayAnimation(ButtonState::Idle);

	PushEvent<event::ButtonHoverStop>(*this, *this);

	return *this;
}

Button& Button::SetShape(const std::optional<std::variant<Rect, Circle>>& shape) {
	Remove<Rect>();
	Remove<Circle>();

	if (!shape.has_value()) {
		return *this;
	}

	std::visit(
		[this](const auto& value) { Add<std::remove_cvref_t<decltype(value)>>(value); }, *shape
	);

	return *this;
}

Button& Button::SetShape(Rect rect) {
	return SetShape(std::variant<Rect, Circle>{ rect });
}

Button& Button::SetShape(Circle circle) {
	return SetShape(std::variant<Rect, Circle>{ circle });
}

Button& Button::SetSize(V2_float size) {
	return SetShape(Rect{ size });
}

Button& Button::RemoveShape() {
	Remove<Rect>();
	Remove<Circle>();
	return *this;
}

Entity Button::Part(ButtonPartRole role, ButtonVisualState state) {
	if (auto existing{ TryPart(role, state) }) {
		return *existing;
	}

	Entity part{ GetScene().CreateEntity() };

	part.Add<impl::ButtonPart>(role, state);
	SetParent(part, *this);

	Show(part, true);

	return part;
}

std::optional<Entity> Button::TryPart(ButtonPartRole role, ButtonVisualState state) const {
	return FindButtonPart(*this, role, state);
}

std::vector<Entity> Button::Parts(ButtonPartRole role) const {
	return FindButtonParts(*this, role);
}

std::vector<Entity> Button::Parts() const {
	return FindButtonParts(*this);
}

Button& Button::RemovePart(ButtonPartRole role, ButtonVisualState state) {
	if (auto part{ TryPart(role, state) }) {
		Hide(*part);
		part->Remove<impl::ButtonPart>();
	}

	return *this;
}

Entity Button::Background(ButtonVisualState state) {
	return Part(ButtonPartRole::Background, state);
}

Entity Button::Border(ButtonVisualState state) {
	return Part(ButtonPartRole::Border, state);
}

Text Button::Label(ButtonVisualState state) {
	if (auto label{ TryLabel(state) }) {
		return *label;
	}

	Text text{ CreateText(GetScene(), {}, Origin::Center) };
	text.Add<impl::ButtonPart>(ButtonPartRole::Label, state);
	text.Add<impl::ButtonLabelAutoBox>();
	SetParent(text, *this);

	return text;
}

Sprite Button::Icon(ButtonVisualState state) {
	if (auto icon{ TryIcon(state) }) {
		return *icon;
	}

	Entity part{ Part(ButtonPartRole::Icon, state) };
	return Sprite{ part };
}

std::optional<Entity> Button::TryBackground(ButtonVisualState state) const {
	return TryPart(ButtonPartRole::Background, state);
}

std::optional<Entity> Button::TryBorder(ButtonVisualState state) const {
	return TryPart(ButtonPartRole::Border, state);
}

std::optional<Text> Button::TryLabel(ButtonVisualState state) const {
	auto part{ TryPart(ButtonPartRole::Label, state) };
	if (!part.has_value()) {
		return std::nullopt;
	}

	return Text{ *part };
}

std::optional<Sprite> Button::TryIcon(ButtonVisualState state) const {
	auto part{ TryPart(ButtonPartRole::Icon, state) };
	if (!part.has_value()) {
		return std::nullopt;
	}

	return Sprite{ *part };
}

Button& Button::RemoveBackground(ButtonVisualState state) {
	return RemovePart(ButtonPartRole::Background, state);
}

Button& Button::RemoveBorder(ButtonVisualState state) {
	return RemovePart(ButtonPartRole::Border, state);
}

Button& Button::RemoveLabel(ButtonVisualState state) {
	return RemovePart(ButtonPartRole::Label, state);
}

Button& Button::RemoveIcon(ButtonVisualState state) {
	return RemovePart(ButtonPartRole::Icon, state);
}

Button& Button::SetLabel(std::string_view content, ButtonVisualState state) {
	Label(state).Content(content);
	return *this;
}

Button& Button::SetIcon(std::string_view texture_key, ButtonVisualState state) {
	Icon(state).SetTexture(texture_key);
	return *this;
}

Button& Button::SetLabelAutoBox(bool enabled, ButtonVisualState state) {
	Text label{ Label(state) };
	auto& auto_box{ label.TryAdd<impl::ButtonLabelAutoBox>() };
	auto_box.enabled = enabled;
	return *this;
}

Button& Button::SetLabelPadding(Rect padding, ButtonVisualState state) {
	Text label{ Label(state) };
	auto& auto_box{ label.TryAdd<impl::ButtonLabelAutoBox>() };
	auto_box.padding = padding;
	return *this;
}

Button& Button::SetSound(std::optional<std::string_view> sound_key, ButtonState state) {
	auto& sounds{ TryAdd<impl::ButtonSounds>() };

	std::optional<Audio>* slot{ nullptr };

	switch (state) {
		using enum ButtonState;

		case Idle:	slot = &sounds.idle; break;
		case Hover: slot = &sounds.hover; break;
		case Press: slot = &sounds.press; break;
	}

	PTGN_ASSERT(slot);

	if (!sound_key.has_value()) {
		*slot = std::nullopt;
		return *this;
	}

	*slot = GetScene().ctx().asset.Get<Audio>(*sound_key);

	return *this;
}

std::optional<Audio> Button::GetSound(ButtonState state) const {
	auto sounds{ TryGet<impl::ButtonSounds>() };
	if (!sounds) {
		return std::nullopt;
	}

	switch (state) {
		using enum ButtonState;

		case Idle:	return sounds->idle;
		case Hover: return sounds->hover;
		case Press: return sounds->press;
	}

	return std::nullopt;
}

Button& Button::SetExclusiveAudio(bool enabled) {
	if (enabled) {
		Add<impl::ButtonExclusiveAudio>();
	} else {
		Remove<impl::ButtonExclusiveAudio>();
	}

	return *this;
}

void Button::RefreshVisualState() {
	auto active_state{ GetVisualState() };

	for (Entity part : Parts()) {
		auto info{ part.TryGet<impl::ButtonPart>() };
		if (!info) {
			continue;
		}

		if (IsPartVisibleForState(*info, active_state)) {
			Show(part);
		} else {
			Hide(part);
		}
	}
}

void Button::SetState(impl::InternalButtonState state) {
	auto& data{ Get<impl::ButtonData>() };

	if (data.state == state) {
		return;
	}

	data.state = state;
	RefreshVisualState();
}

void Button::PlaySound(ButtonState active) {
	auto sound{ GetSound(active) };

	if (!sound.has_value()) {
		return;
	}

	auto& audio{ GetScene().ctx().audio };
	audio.Play(sound->GetEntity().Get<impl::AssetName>().value);
}

void Button::PlayAnimation(ButtonState) {
	// State-specific animations now belong to child entities.
	// Add your animation start/reset policy here if needed.
}

void Button::UpdateChildLayouts() {
	auto shape{ GetShape() };
	if (!shape.has_value()) {
		return;
	}

	auto size{ GetShapeSize(*shape) };
	if (!size.has_value()) {
		return;
	}

	Rect button_box{
		{ -size->x * 0.5f, -size->y * 0.5f },
		{ size->x * 0.5f, size->y * 0.5f },
	};

	for (Entity part : Parts(ButtonPartRole::Label)) {
		auto auto_box{ part.TryGet<impl::ButtonLabelAutoBox>() };
		if (!auto_box || !auto_box->enabled) {
			continue;
		}

		Rect box{ button_box };
		box.min += auto_box->padding.min;
		box.max -= auto_box->padding.max;

		Text{ part }.Box(box).Align(HorizontalAlign::Center, VerticalAlign::Center);
	}
}

Button CreateButton(Scene& scene, const ButtonDesc& desc) {
	Button button{ scene.CreateEntity() };

	button.Add<impl::ButtonData>();
	button.Add<impl::ButtonEnabled>();

	if (desc.ui_layer) {
		SetUI(button, true);
	}

	Show(button, false);
	SetDraw<Button>(button);
	button.SetShape(desc.shape);

	SetPosition(button, desc.position);
	SetDrawOrigin(button, desc.origin);
	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

	if (!desc.enabled) {
		button.Disable();
	}

	return button;
}

Button CreateButton(
	Scene& scene, V2_float position, const std::optional<std::variant<Rect, Circle>>& shape,
	Origin draw_origin
) {
	return CreateButton(
		scene, ButtonDesc{
				   .position = position,
				   .shape	 = shape,
				   .origin	 = draw_origin,
			   }
	);
}

Button CreateButton(
	Scene& scene, V2_float position, V2_float size, const ButtonConfig& config, Origin draw_origin
) {
	Button button{ CreateButton(scene, position, Rect{ size }, draw_origin) };

	if (config.background_color.has_value()) {
		Entity background{ button.Background() };
		background.Add<Rect>(Rect{ size });
		SetTint(background, *config.background_color);
	}

	if (config.texture.has_value()) {
		button.SetIcon(*config.texture);
	}

	if (config.content.has_value()) {
		Text label{ button.Label() };
		label.Content(*config.content)
			.Color(config.text_color.value_or(impl::kDefaultButtonTextColor))
			.Size(config.font_size)
			.Font(config.font)
			.Align(HorizontalAlign::Center, VerticalAlign::Center);
	}

	button.SetSound(config.sound_hover, ButtonState::Hover);
	button.SetSound(config.sound_press, ButtonState::Press);

	return button;
}

Button CreateAnimatedButton(
	Scene& scene, V2_float position, std::optional<V2_float> size,
	const AnimatedButtonConfig& config, Origin draw_origin
) {
	V2_float resolved_size{ size.value_or(V2_float{ 0.0f, 0.0f }) };

	Button button{ CreateButton(scene, position, Rect{ resolved_size }, draw_origin) };
	button.SetIcon(config.texture);

	button.SetSound(config.sound_hover, ButtonState::Hover);
	button.SetSound(config.sound_press, ButtonState::Press);

	return button;
}

} // namespace ptgn