#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include "components/draw.h"
#include "components/generic.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/text.h"
#include "renderer/texture.h"

namespace ptgn {

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

struct ButtonToggle : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonHoverStart : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonHoverStop : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonActivate : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

enum class InternalButtonState : std::size_t {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

struct TextAlignment : public OriginComponent {
	using OriginComponent::OriginComponent;
};

struct ButtonToggled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct ButtonColor {
	ButtonColor() = default;

	ButtonColor(const Color& color) :
		current_{ color }, default_{ color }, hover_{ color }, pressed_{ color } {}

	void SetToState(ButtonState state);

	Color current_;
	Color default_;
	Color hover_;
	Color pressed_;
};

struct ButtonColorToggled : public ButtonColor {};

} // namespace impl

using ButtonCallback = std::function<void()>;

struct Button : public GameObject {
	Button() = default;
	Button(ecs::Manager& manager);
	virtual ~Button() = default;

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;

	Button& SetBackgroundColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Current) const;

	Button& SetTextColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] ecs::Entity GetTexture(ButtonState state = ButtonState::Current) const;

	Button& SetTexture(std::string_view texture_key, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Current) const;

	Button& SetTint(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string GetTextContent(ButtonState state = ButtonState::Current) const;

	Button& SetTextContent(const std::string& content, ButtonState state = ButtonState::Default);

	[[nodiscard]] ecs::Entity GetText(ButtonState state = ButtonState::Current) const;

	[[nodiscard]] bool IsBordered() const;

	Button& SetBordered(bool bordered);

	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Current) const;

	Button& SetBorderColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextJustify GetTextJustify() const;

	Button& SetTextJustify(const TextJustify& justify);

	[[nodiscard]] V2_float GetTextSize() const;

	// (default: unscaled text size). If either axis of the text size
	// is zero, it is stretched to fit the entire size of the button rectangle (along that axis).
	Button& SetTextSize(const V2_float& size);

	[[nodiscard]] std::int32_t GetFontSize() const;

	Button& SetFontSize(std::int32_t font_size);

	[[nodiscard]] float GetBackgroundLineWidth() const;

	// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
	// width.
	Button& SetBackgroundLineWidth(float line_width);

	[[nodiscard]] float GetBorderWidth() const;

	Button& SetBorderWidth(float line_width);

	// TODO: Add SetRadius() to turn button into rounded rectangle.

	Button& OnHoverStart(const ButtonCallback& callback);
	Button& OnHoverStop(const ButtonCallback& callback);
	Button& OnActivate(const ButtonCallback& callback);

	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

private:
	void StateChange(impl::InternalButtonState new_state);
};

struct ToggleButton : public Button {
	ToggleButton() = default;
	ToggleButton(ecs::Manager& manager, bool toggled = false);

	void Activate() final;

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& Toggle();

	[[nodiscard]] Color GetColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] const impl::Texture& GetTextureToggled(ButtonState state = ButtonState::Default)
		const;

	ToggleButton& SetTextureToggled(
		std::string_view texture_key, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetTintToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTintToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string GetTextContentToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextContentToggled(
		const std::string& content, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBorderColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);

	ToggleButton& OnToggle(const ButtonCallback& callback);
};

/*
class ToggleButtonGroup : public MapManager<Button, std::string_view, std::string, false> {
public:
	ToggleButtonGroup()										   = default;
	virtual ~ToggleButtonGroup() override					   = default;
	ToggleButtonGroup(ToggleButtonGroup&&) noexcept			   = default;
	ToggleButtonGroup& operator=(ToggleButtonGroup&&) noexcept = default;
	ToggleButtonGroup(const ToggleButtonGroup&)				   = delete;
	ToggleButtonGroup& operator=(const ToggleButtonGroup&)	   = delete;

	template <typename TKey, typename... TArgs, tt::constructible<Button, TArgs...> = true>
	Button& Load(const TKey& key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(key) };
		Button& button{ MapManager::Load(key, Button{ std::forward<TArgs>(constructor_args)... }) };
		// Toggle all other buttons when one is pressed.
		button.SetInternalOnActivate([this, &button]() {
			ForEachValue([](Button& b) { b.Set<ButtonProperty::Toggled>(false); });
			button.Set<ButtonProperty::Toggled>(true);
		});
		return button;
	}
};
*/

inline std::ostream& operator<<(std::ostream& os, ButtonState state) {
	switch (state) {
		case ButtonState::Default: os << "Default"; break;
		case ButtonState::Hover:   os << "Hover"; break;
		case ButtonState::Pressed: os << "Pressed"; break;
		default:				   PTGN_ERROR("Invalid button state");
	}
	return os;
}

inline std::ostream& operator<<(std::ostream& os, impl::InternalButtonState state) {
	switch (state) {
		case impl::InternalButtonState::IdleDown:	  os << "Idle Down"; break;
		case impl::InternalButtonState::IdleUp:		  os << "Idle Up"; break;
		case impl::InternalButtonState::Hover:		  os << "Hover"; break;
		case impl::InternalButtonState::HoverPressed: os << "Hover Pressed"; break;
		case impl::InternalButtonState::Pressed:	  os << "Pressed"; break;
		case impl::InternalButtonState::HeldOutside:  os << "Held Outside"; break;
		default:									  PTGN_ERROR("Invalid internal button state");
	}
	return os;
}

} // namespace ptgn