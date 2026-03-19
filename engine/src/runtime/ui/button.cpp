#include "runtime/ui/button.h"

#include <algorithm>
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
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/interactive.h"

namespace ptgn {

namespace impl {

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
	if (Button button{ entity }; !button.IsEnabled(false)) {
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
	if (Button button{ entity }; !button.IsEnabled(false)) {
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
	if (Button button{ entity }; !button.IsEnabled(false)) {
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

static bool IsToggled(Entity button) {
	return button.Has<impl::ButtonToggled>();
}

template <typename Derived>
void ButtonBase<Derived>::Draw(DrawContext& renderer, Entity entity, Camera camera) {
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
	PTGN_ASSERT(button_size.has_value(), "Buttons must have a non-zero size to be drawn");
	auto button_origin{ GetDrawOrigin(button) };
	auto text{ GetButtonText(button, is_toggled, state) };

	if (auto button_texture{ GetButtonTexture(button, is_toggled, state) };
		button_texture.has_value()) {
		auto texture_tint{ GetEffectiveColor<impl::ButtonTint, impl::ButtonTintToggled>(
			button, is_toggled, color::White
		) };

		if (texture_tint.a) {
			auto tex_coords{ GetTextureCoordinates(button, false) };
			Tint final_tint{ texture_tint.Normalized() * tint_n };
			renderer.DrawTexture(
				*button_texture, transform, *button_size, button_origin, final_tint, depth,
				tex_coords, blend_mode
			);
		}
	} else {
		auto line_width{ button.GetOrDefault<impl::ButtonBackgroundWidth>() };

		if (line_width >= kMinLineWidth || line_width == -1.0f) {
			auto color{
				GetEffectiveColor<impl::ButtonColor, impl::ButtonColorToggled>(button, is_toggled)
			};

			if (color.a) {
				Rect rect{ *button_size };
				FillStyle fill_style{ line_width.GetValue() };
				Tint final_tint{ color.Normalized() * tint_n };
				renderer.DrawShape(
					rect, transform, final_tint, fill_style, button_origin, depth, blend_mode
				);
			}
		}
	}

	if (auto line_width{ button.GetOrDefault<impl::ButtonBorderWidth>() };
		line_width >= kMinLineWidth || line_width == -1.0f) {
		auto color{ GetEffectiveColor<impl::ButtonBorderColor, impl::ButtonBorderColorToggled>(
			button, is_toggled
		) };

		if (color.a) {
			Rect rect{ *button_size };
			FillStyle fill_style{ line_width.GetValue() };
			Tint final_tint{ color.Normalized() * tint_n };
			renderer.DrawShape(
				rect, transform, final_tint, fill_style, button_origin, depth, blend_mode
			);
		}
	}

	if (!text) {
		return;
	}

	V2_float text_size;

	if (auto fixed_size{ button.TryGet<ButtonTextFixedSize>() }) {
		text_size = { fixed_size->x.value_or(button_size->x),
					  fixed_size->y.value_or(button_size->y) };
	}

	Text::Draw(renderer, text, text_size, tint, button_origin, *button_size, camera);
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
std::variant<Rect, Circle> ButtonBase<Derived>::GetShape() const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetShape(std::variant<std::monostate, Rect, Circle>) {}

template <typename Derived>
Color ButtonBase<Derived>::GetBackgroundColor(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundColor(Color color, ButtonState state) {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetText(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {}

template <typename Derived>
Entity ButtonBase<Derived>::GetText(ButtonState state) const {}

template <typename Derived>
Color ButtonBase<Derived>::GetTextColor(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextColor(Color text_color, ButtonState state) {}

template <typename Derived>
std::string ButtonBase<Derived>::GetTextContent(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextContent(std::string_view content, ButtonState state) {}

template <typename Derived>
TextJustify ButtonBase<Derived>::GetTextJustify(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTextJustify(TextJustify justify, ButtonState state) {}

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
float ButtonBase<Derived>::GetFontSize(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetFontSize(float font_size, ButtonState state) {}

template <typename Derived>
Texture ButtonBase<Derived>::GetTexture(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTexture(Texture texture, ButtonState state) {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetDisabledTexture(Texture texture) {}

template <typename Derived>
Texture ButtonBase<Derived>::GetDisabledTexture() const {}

template <typename Derived>
Color ButtonBase<Derived>::GetTint(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetTint(Color color, ButtonState state) {}

template <typename Derived>
Color ButtonBase<Derived>::GetBorderColor(ButtonState state) const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderColor(Color color, ButtonState state) {}

template <typename Derived>
FillStyle ButtonBase<Derived>::GetBackgroundFillStyle() const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBackgroundFillStyle(FillStyle fill_style) {}

template <typename Derived>
float ButtonBase<Derived>::GetBorderWidth() const {}

template <typename Derived>
Derived& ButtonBase<Derived>::SetBorderWidth(float line_width) {}

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
		return ButtonState::Idle;
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

Color ToggleButton::GetBackgroundColorToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetBackgroundColorToggled(Color color, ButtonState state) {}

Color ToggleButton::GetTextColorToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetTextColorToggled(Color text_color, ButtonState state) {}

std::string ToggleButton::GetTextContentToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetTextContentToggled(std::string_view content, ButtonState state) {}

ToggleButton& ToggleButton::SetTextToggled(
	std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& text_properties, ButtonState state
) {}

Text ToggleButton::GetTextToggled(ButtonState state) const {}

Color ToggleButton::GetBorderColorToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetBorderColorToggled(Color color, ButtonState state) {}

Texture ToggleButton::GetTextureToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetTextureToggled(Texture texture, ButtonState state) {}

Color ToggleButton::GetTintToggled(ButtonState state) const {}

ToggleButton& ToggleButton::SetTintToggled(Color color, ButtonState state) {}

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

Button CreateButton(
	Scene& scene, std::variant<std::monostate, Rect, Circle> shape, const ButtonConfig& config,
	bool ui_layer
) {
	Button button{ scene.CreateEntity() };

	if (ui_layer) {
		SetUI(button, true);
	}

	Show(button, false);
	SetDraw<Button>(button);

	auto resolved_shape = std::visit(
		[&]<typename T>(const T& arg) -> std::variant<Rect, Circle> {
			if constexpr (std::is_same_v<T, std::monostate>) {
				if (config.enabled.idle.sprite.has_value()) {
					auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.sprite) };
					PTGN_ASSERT(texture_size.has_value(), "No valid texture size for button");
					return Rect{ *texture_size };
				} else if (config.enabled.idle.text.has_value()) {
					auto texture_size{ GetCroppedTextureSize(*config.enabled.idle.text) };
					PTGN_ASSERT(texture_size.has_value(), "No valid text size for button");
					return Rect{ *texture_size };
				} else {
					PTGN_ERROR("Failed to find a valid size for the button");
				}
			} else if (std::is_same_v<T, Rect>) {
				return arg;
			} else if (std::is_same_v<T, Circle>) {
				return arg;
			}
		},
		shape
	);

	std::visit([&]<typename T>(const T& arg) { button.Add<T>(arg); }, resolved_shape);

	SetInteractive(button);

	button.Add<ButtonConfig>(config);

	button.Add<impl::InternalButtonState>(impl::InternalButtonState::IdleUp);

	PTGN_ASSERT(!HasScript<impl::InternalButtonScript>(button));
	AddScript<impl::InternalButtonScript>(button);
	button.Enable();

	return button;
}

ToggleButton CreateToggleButton(
	Scene& scene, std::variant<std::monostate, Rect, Circle> shape,
	const ToggleButtonConfig& config, bool toggled
) {
	ToggleButton toggle_button{ CreateButton(scene, shape, config) };

	toggle_button.Add<impl::ToggleButtonInteractionConfig>(config.toggled);

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