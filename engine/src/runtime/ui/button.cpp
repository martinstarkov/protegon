#include "runtime/ui/button.h"

#include <ecs/ecs.h>

#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/mouse.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/toggle_button.h"

namespace ptgn {

namespace {

HorizontalAlign GetHorizontalAlignment(Origin origin) {
	switch (origin) {
		using enum Origin;

		case TopLeft:
		case CenterLeft:
		case BottomLeft:   return HorizontalAlign::Left;

		case CenterTop:
		case Center:
		case CenterBottom: return HorizontalAlign::Center;

		case TopRight:
		case CenterRight:
		case BottomRight:  return HorizontalAlign::Right;
	}

	PTGN_ERROR("Unknown Origin: ", std::to_underlying(origin));
}

VerticalAlign GetVerticalAlignment(Origin origin) {
	switch (origin) {
		using enum Origin;

		case TopLeft:
		case CenterTop:
		case TopRight:	   return VerticalAlign::Top;

		case CenterLeft:
		case Center:
		case CenterRight:  return VerticalAlign::Center;

		case BottomLeft:
		case CenterBottom:
		case BottomRight:  return VerticalAlign::Bottom;
	}

	PTGN_ERROR("Unknown Origin: ", std::to_underlying(origin));
}

[[nodiscard]] bool IsPressVisualState(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;

		case Press:
		case ToggledPress:
		case DisabledPress: return true;

		default:			return false;
	}
}

void ResetButtonAnimationPart(Entity part) {
	if (!part.Has<impl::AnimationData>()) {
		return;
	}

	Animation animation{ part };

	auto animation_part{ part.TryGet<impl::ButtonAnimationPart>() };
	auto static_frame{ animation_part ? animation_part->options.static_frame : 0uz };

	animation.Reset();
	animation.SetCurrentFrame(static_frame);
}

std::vector<ButtonVisualState> GetVisualStateFallbacks(ButtonVisualState state) {
	switch (state) {
		using enum ButtonVisualState;

		case Base:			return { Base };

		case Idle:			return { Idle, Base };

		case Hover:			return { Hover, Idle, Base };

		case Press:			return { Press, Hover, Idle, Base };

		case Disabled:		return { Disabled, Idle, Base };

		case DisabledHover: return { DisabledHover, Disabled, Hover, Idle, Base };

		case DisabledPress:
			return { DisabledPress, DisabledHover, Disabled, Press, Hover, Idle, Base };

		case Toggled:	   return { Toggled, Idle, Base };

		case ToggledHover: return { ToggledHover, Toggled, Hover, Idle, Base };

		case ToggledPress: return { ToggledPress, ToggledHover, Toggled, Press, Hover, Idle, Base };
	}

	return { ButtonVisualState::Base };
}

std::optional<Animation> TryAnimationForVisualState(Button button, ButtonVisualState state) {
	for (auto fallback_state : GetVisualStateFallbacks(state)) {
		auto animation{ button.TryAnimation(fallback_state) };
		if (animation.has_value()) {
			return animation;
		}
	}

	return std::nullopt;
}

ButtonVisualState GetPressVisualState(Button button) {
	if (!button.IsEnabled(false)) {
		return ButtonVisualState::DisabledPress;
	}

	if (button.HasToggle() && button.AsToggle().IsToggled()) {
		return ButtonVisualState::ToggledPress;
	}

	return ButtonVisualState::Press;
}

ButtonVisualState ToVisualState(ButtonState state) {
	switch (state) {
		using enum ButtonState;

		case Idle:	return ButtonVisualState::Idle;
		case Hover: return ButtonVisualState::Hover;
		case Press: return ButtonVisualState::Press;
	}

	return ButtonVisualState::Idle;
}

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

		if (role.has_value() && part->role != role.value()) {
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

[[nodiscard]] std::optional<V2_float> GetButtonShapeSize(Button button) {
	auto shape{ button.GetShape() };

	if (!shape.has_value()) {
		return std::nullopt;
	}

	auto transform{ GetWorldTransform(button) };

	return std::visit(
		[&](const auto& value) -> std::optional<V2_float> { return value.GetSize(transform); },
		shape.value()
	);
}

[[nodiscard]] Rect GetButtonLocalRect(Button button, V2_float size) {
	V2_float center{ GetOffset(GetDrawOrigin(button), size) };
	V2_float half_size{ size * 0.5f };

	return Rect{
		center - half_size,
		center + half_size,
	};
}

[[nodiscard]] Rect ApplyContentPadding(Rect rect, Rect padding) {
	rect.min += padding.min;
	rect.max -= padding.max;
	return rect;
}

} // namespace

namespace impl {

ButtonAnimationCompleteScript::ButtonAnimationCompleteScript(Entity button) : button{ button } {}

void ButtonAnimationCompleteScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::AnimationComplete>([this]() {
		if (!button) {
			return;
		}

		Button btn{ button };

		auto part{ entity.TryGet<ButtonPart>() };
		auto visual_override{ btn.TryGet<ButtonVisualOverride>() };

		if (part && visual_override && visual_override->state == part->state) {
			btn.Remove<ButtonVisualOverride>();
			btn.RefreshVisualState();

			ResetButtonAnimationPart(entity);
		}
	});
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

void UpdateButtons(Scene& scene) {
	for (auto [entity, _data] : scene.EntitiesWith<impl::ButtonData>()) {
		Button button{ entity };

		button.UpdateChildLayouts();
		button.RefreshVisualState();
	}
}

} // namespace impl

Button::Button(Entity entity) : Entity{ entity } {}

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
	if (!IsEnabled(false)) {
		return DisabledStateFrom(GetState());
	}

	if (auto visual_override{ TryGet<impl::ButtonVisualOverride>() }) {
		return visual_override->state;
	}

	if (HasToggle() && AsToggle().IsToggled()) {
		return ToggledStateFrom(GetState());
	}

	return NormalStateFrom(GetState());
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

	if (auto visual_override{ TryGet<impl::ButtonVisualOverride>() };
		visual_override && visual_override->block_press) {
		return *this;
	}

	auto press_visual_state{ GetPressVisualState(*this) };

	if (auto animation{ TryAnimationForVisualState(*this, press_visual_state) }) {
		auto part{ animation->TryGet<impl::ButtonAnimationPart>() };

		if (part && part->options.lock_visual_state) {
			auto& visual_override{ TryAdd<impl::ButtonVisualOverride>() };
			visual_override.state		= press_visual_state;
			visual_override.block_press = part->options.block_press;

			RefreshVisualState();
		}

		animation->Reset();
		animation->SetCurrentFrame(part ? part->options.static_frame : 0uz);
		animation->Start(true);
	} else {
		PlayAnimation(ButtonState::Press);
	}

	PlaySound(ButtonState::Press);

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

	std::visit([this]<typename T>(const T& value) { Add<T>(value); }, shape.value());

	UpdateChildLayouts();

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

	Text text{ CreateText(GetScene(), {}, Origin::TopLeft) };

	text.Add<impl::ButtonPart>(ButtonPartRole::Label, state);
	text.Add<impl::ButtonLabelAutoBox>();

	SetParent(text, *this);
	Show(text);

	UpdateChildLayouts();
	RefreshVisualState();

	return text;
}

Sprite Button::Icon(ButtonVisualState state) {
	if (auto icon{ TryIcon(state) }) {
		return *icon;
	}

	Sprite sprite{ CreateSprite(GetScene(), {}, {}, Origin::Center) };

	sprite.Add<impl::ButtonPart>(ButtonPartRole::Icon, state);

	SetParent(sprite, *this);
	Show(sprite);

	RefreshVisualState();

	return sprite;
}

Button& Button::SetLabelOrigin(Origin origin, ButtonVisualState state) {
	Text label{ Label(state) };

	auto& auto_box{ label.TryAdd<impl::ButtonLabelAutoBox>() };
	auto_box.origin = origin;

	UpdateChildLayouts();

	return *this;
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

	return Text{ part.value() };
}

std::optional<Sprite> Button::TryIcon(ButtonVisualState state) const {
	auto part{ TryPart(ButtonPartRole::Icon, state) };
	if (!part.has_value()) {
		return std::nullopt;
	}

	return Sprite{ part.value() };
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

Button& Button::SetTexture(std::string_view texture_key, ButtonVisualState state) {
	Icon(state).SetTexture(texture_key);
	RefreshVisualState();
	return *this;
}

Button& Button::SetAnimation(
	Animation&& animation, ButtonVisualState state, ButtonAnimationOptions options
) {
	RemoveIcon(state);

	Animation anim{ std::move(animation) };

	anim.Add<impl::ButtonPart>(ButtonPartRole::Icon, state);
	anim.Add<impl::ButtonAnimationPart>(options);

	SetParent(anim, *this);
	SetDrawOrigin(anim, Origin::Center);

	anim.Reset();

	if (options.playback == ButtonAnimationPlayback::StaticFrame) {
		anim.SetCurrentFrame(options.static_frame);
	}

	Hide(anim);

	if (!HasScript<impl::ButtonAnimationCompleteScript>(anim)) {
		AddScript<impl::ButtonAnimationCompleteScript>(anim, *this);
	}

	RefreshVisualState();

	return *this;
}

Button& Button::SetAnimation(Animation&& animation, ButtonVisualState state) {
	ButtonAnimationOptions options;

	if (state == ButtonVisualState::Press || state == ButtonVisualState::ToggledPress ||
		state == ButtonVisualState::DisabledPress) {
		options.playback		  = ButtonAnimationPlayback::PlayOnce;
		options.lock_visual_state = true;
		options.block_press		  = false;
	}

	return SetAnimation(std::move(animation), state, options);
}

Button& Button::SetStaticAnimationFrame(
	Animation&& animation, ButtonVisualState state, std::size_t frame
) {
	return SetAnimation(
		std::move(animation), state,
		ButtonAnimationOptions{
			.playback	  = ButtonAnimationPlayback::StaticFrame,
			.static_frame = frame,
		}
	);
}

std::optional<Animation> Button::TryAnimation(ButtonVisualState state) const {
	auto icon{ TryIcon(state) };

	if (!icon.has_value()) {
		return std::nullopt;
	}

	if (!icon.value().Has<impl::AnimationData>()) {
		return std::nullopt;
	}

	return Animation{ icon.value() };
}

Button& Button::RemoveAnimation(ButtonVisualState state) {
	auto animation{ TryAnimation(state) };

	if (!animation.has_value()) {
		return *this;
	}

	animation.value().Stop(true);
	animation.value().Remove<impl::AnimationData>();
	animation.value().Remove<impl::ButtonPart>();
	Hide(animation.value());

	return *this;
}

Button& Button::SetLabelAutoBox(bool enabled, ButtonVisualState state) {
	Text label{ Label(state) };
	auto& auto_box{ label.TryAdd<impl::ButtonLabelAutoBox>() };
	auto_box.enabled = enabled;
	UpdateChildLayouts();
	return *this;
}

Button& Button::SetLabelPadding(Rect padding, ButtonVisualState state) {
	Text label{ Label(state) };
	auto& auto_box{ label.TryAdd<impl::ButtonLabelAutoBox>() };
	auto_box.padding = padding;
	UpdateChildLayouts();
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
		slot->reset();
		return *this;
	}

	auto audio{ impl::AssetAccessor{ GetScene().ctx().asset }.Get<Audio>(sound_key.value()) };

	slot->emplace(std::move(audio));

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
	auto parts{ Parts() };

	if (!IsVisible(*this)) {
		for (Entity part : parts) {
			Hide(part);
		}
		return;
	}

	auto active_state{ GetVisualState() };
	auto fallback_states{ GetVisualStateFallbacks(active_state) };

	for (Entity part : parts) {
		Hide(part);
	}

	auto show_role = [&](ButtonPartRole role) {
		for (auto state : fallback_states) {
			bool found{ false };

			for (Entity part : parts) {
				auto info{ part.TryGet<impl::ButtonPart>() };

				if (!info) {
					continue;
				}

				if (info->role != role || info->state != state) {
					continue;
				}

				Show(part);
				found = true;
			}

			if (found) {
				return;
			}
		}
	};

	show_role(ButtonPartRole::Background);
	show_role(ButtonPartRole::Border);
	show_role(ButtonPartRole::Icon);
	show_role(ButtonPartRole::Label);
}

void Button::SetState(impl::InternalButtonState state) {
	auto& data{ Get<impl::ButtonData>() };

	if (data.state == state) {
		return;
	}

	auto old_visual_state{ GetVisualState() };

	data.state = state;

	auto new_visual_state{ GetVisualState() };

	if (old_visual_state != new_visual_state && IsPressVisualState(new_visual_state)) {
		if (auto animation{ TryAnimationForVisualState(*this, new_visual_state) }) {
			ResetButtonAnimationPart(*animation);
		}
	}

	RefreshVisualState();
}

void Button::PlaySound(ButtonState active) {
	auto sound{ GetSound(active) };

	if (!sound.has_value()) {
		return;
	}

	auto& audio{ GetScene().ctx().audio };
	audio.Play(sound.value().GetEntity().Get<impl::AssetName>().value);
}

void Button::PlayAnimation(ButtonState active) const {
	auto active_state{ ToVisualState(active) };

	for (Entity part : Parts(ButtonPartRole::Icon)) {
		auto part_data{ part.TryGet<impl::ButtonPart>() };

		if (!part_data || !part.Has<impl::AnimationData>()) {
			continue;
		}

		Animation animation{ part };

		auto animation_part{ part.TryGet<impl::ButtonAnimationPart>() };
		auto playback{ animation_part ? animation_part->options.playback
									  : ButtonAnimationPlayback::Play };
		auto static_frame{ animation_part ? animation_part->options.static_frame : 0uz };

		if (part_data->state != active_state) {
			animation.Reset();
			animation.SetCurrentFrame(static_frame);
			continue;
		}

		switch (playback) {
			using enum ButtonAnimationPlayback;

			case StaticFrame:
				animation.Reset();
				animation.SetCurrentFrame(static_frame);
				break;

			case Play:
			case PlayOnce: animation.Start(true); break;
		}
	}
}

void Button::UpdateChildLayouts() {
	auto size{ GetButtonShapeSize(*this) };

	if (!size.has_value() || !size->IsPositive()) {
		return;
	}

	Rect button_rect{ GetButtonLocalRect(*this, *size) };

	for (Entity part : Parts(ButtonPartRole::Label)) {
		auto auto_box{ part.TryGet<impl::ButtonLabelAutoBox>() };

		if (!auto_box || !auto_box->enabled) {
			continue;
		}

		Rect content_rect{ ApplyContentPadding(button_rect, auto_box->padding) };

		if (!content_rect.GetSize().IsPositive()) {
			continue;
		}

		Origin origin{ auto_box->origin };
		V2_float content_size{ content_rect.GetSize() };
		V2_float label_position{ content_rect.GetOriginPoint(origin) };

		SetPosition(part, label_position);
		SetDrawOrigin(part, origin);

		Text label{ part };

		if (Rect label_box{ {}, content_size }; label.GetTextBox().rect != label_box) {
			label.Box(label_box);
		}

		label.ApplyFallbackAlignment(GetHorizontalAlignment(origin), GetVerticalAlignment(origin));
	}
}

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc) {
	Button button{ scene.CreateEntity() };

	button.Add<impl::ButtonData>();
	button.Add<impl::ButtonEnabled>();

	if (desc.ui_layer) {
		SetUI(button, true);
	}

	Show(button, false);
	button.SetShape(desc.shape);

	SetTransform(button, transform);
	SetDrawOrigin(button, desc.origin);
	SetInteractive(button);

	AddScript<impl::ButtonScript>(button);

	if (!desc.enabled) {
		button.Disable();
	}

	return button;
}

Button CreateButton(
	Scene& scene, Transform transform, const std::optional<std::variant<Rect, Circle>>& shape,
	Origin draw_origin
) {
	return CreateButton(
		scene, transform,
		ButtonDesc{
			.shape	= shape,
			.origin = draw_origin,
		}
	);
}

Button CreateButton(
	Scene& scene, Transform transform, V2_float size, const ButtonConfig& config, Origin draw_origin
) {
	Button button{ CreateButton(scene, transform, Rect{ size }, draw_origin) };

	if (config.background_color.has_value()) {
		Entity background{ button.Background() };
		background.Add<Rect>(Rect{ size });
		background.Add<Color>(config.background_color.value());
	}

	if (config.texture.has_value()) {
		button.SetIcon(config.texture.value());
	}

	if (config.content.has_value()) {
		Text label{ button.Label() };

		label.Content(config.content.value())
			.Color(config.text_color.value_or(impl::kDefaultButtonTextColor))
			.Size(config.font_size)
			.Font(config.font);

		if (config.horizontal_align.has_value()) {
			label.HorizontalAlign(config.horizontal_align.value());
		}

		if (config.vertical_align.has_value()) {
			label.VerticalAlign(config.vertical_align.value());
		}
	}

	button.SetSound(config.sound_hover, ButtonState::Hover);
	button.SetSound(config.sound_press, ButtonState::Press);

	return button;
}

Button CreateAnimatedButton(
	Scene& scene, Transform transform, std::optional<V2_float> size,
	const AnimatedButtonConfig& config, Origin draw_origin
) {
	V2_float resolved_size{ size.value_or(V2_float{}) };

	Button button{ CreateButton(scene, transform, Rect{ resolved_size }, draw_origin) };
	button.SetIcon(config.texture);

	button.SetSound(config.sound_hover, ButtonState::Hover);
	button.SetSound(config.sound_press, ButtonState::Press);

	return button;
}

} // namespace ptgn