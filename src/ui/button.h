#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>

#include "components/draw.h"
#include "components/generic.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/texture.h"

namespace ptgn {

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

struct ButtonDisable : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonEnable : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonShow : public CallbackComponent<void> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonHide : public CallbackComponent<void> {
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

	ButtonColor(const Color& color) : default_{ color }, hover_{ color }, pressed_{ color } {}

	Color default_;
	Color pressed_;
	Color hover_;
};

struct ButtonColorToggled : public ButtonColor {};

} // namespace impl

enum class ButtonState : std::uint8_t {
	Default = 0,
	Hover	= 1,
	Pressed = 2
};

using ButtonCallback = std::function<void()>;

using TextAlignment = Origin;

struct Button {
	Button() = default;
	Button(ecs::Manager& manager);
	Button(Button&& other) noexcept;
	Button& operator=(Button&& other) noexcept;
	Button(const Button&)			 = delete;
	Button& operator=(const Button&) = delete;
	virtual ~Button();

	[[nodiscard]] ecs::Entity GetEntity();

	// @return True if the button is responding to events, false otherwise.
	[[nodiscard]] bool IsEnabled() const;

	// Disabling a button causes it to stop responding to events.
	Button& SetEnabled(bool enabled);

	Button& Disable();
	Button& Enable();

	// @return True if the button is visible when drawn, false otherwise.
	[[nodiscard]] bool IsVisible() const;

	// Hiding a button causes it to disappear while still responding to events.
	Button& SetVisible(bool visible);

	Button& Show();
	Button& Hide();

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] Color GetColor(ButtonState state = ButtonState::Default) const;

	Button& SetColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Default) const;

	Button& SetTextColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] const impl::Texture& GetTexture(ButtonState state = ButtonState::Default) const;

	Button& SetTexture(std::string_view texture_key, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Default) const;

	Button& SetTint(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string GetTextContent(ButtonState state = ButtonState::Default) const;

	Button& SetTextContent(const std::string& content, ButtonState state = ButtonState::Default);

	[[nodiscard]] bool IsBordered() const;

	Button& SetBordered(bool bordered);

	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Default) const;

	Button& SetBorderColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextAlignment GetTextAlignment() const;

	// Same as Origin relative to button rect position.
	Button& SetTextAlignment(const TextAlignment& alignment);

	[[nodiscard]] V2_float GetTextSize() const;

	// (default: unscaled text size). If either axis of the text size
	// is zero, it is stretched to fit the entire size of the button rectangle (along that axis).
	Button& SetTextSize(const V2_float& size);

	[[nodiscard]] std::int32_t GetFontSize() const;

	Button& SetFontSize(std::int32_t font_size);

	[[nodiscard]] Depth GetDepth() const;

	Button& SetDepth(const Depth& depth);

	[[nodiscard]] float GetLineWidth() const;

	// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
	// width.
	Button& SetLineWidth(float line_width);

	[[nodiscard]] float GetBorderWidth() const;

	Button& SetBorderWidth(float line_width);

	// TODO: Add SetRadius() to turn button into rounded rectangle.

	Button& OnHoverStart(const ButtonCallback& callback);
	Button& OnHoverStop(const ButtonCallback& callback);
	Button& OnActivate(const ButtonCallback& callback);
	Button& OnDisable(const ButtonCallback& callback);
	Button& OnEnable(const ButtonCallback& callback);
	Button& OnShow(const ButtonCallback& callback);
	Button& OnHide(const ButtonCallback& callback);

	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

protected:
	ecs::Entity entity_;
};

struct ToggleButton : public Button {
	ToggleButton() = default;
	ToggleButton(ecs::Manager& manager, bool toggled = false);
	ToggleButton(ToggleButton&&) noexcept			 = default;
	ToggleButton& operator=(ToggleButton&&) noexcept = default;
	ToggleButton(const ToggleButton&)				 = delete;
	ToggleButton& operator=(const ToggleButton&)	 = delete;
	~ToggleButton() override						 = default;

	void Activate() final;

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& Toggle();

	[[nodiscard]] Color GetColorToggled(ButtonState state = ButtonState::Default) const;

	ToggleButton& SetColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Default) const;

	ToggleButton& SetTextColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] const impl::Texture& GetTextureToggled(ButtonState state = ButtonState::Default)
		const;

	ToggleButton& SetTextureToggled(
		std::string_view texture_key, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetTintToggled(ButtonState state = ButtonState::Default) const;

	ToggleButton& SetTintToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string GetTextContentToggled(ButtonState state = ButtonState::Default) const;

	ToggleButton& SetTextContentToggled(
		const std::string& content, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Default) const;

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