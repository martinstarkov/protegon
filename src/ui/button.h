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
#include "renderer/origin.h"
#include "renderer/text.h"
#include "renderer/texture.h"

namespace ptgn {

struct ToggleButton;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

class RenderData;

struct ButtonTag {};

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

struct ButtonToggled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;
};

enum class InternalButtonState : std::size_t {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

struct ButtonBorderWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	ButtonBorderWidth() : ArithmeticComponent{ 1.0f } {}
};

struct ButtonBackgroundWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	ButtonBackgroundWidth() : ArithmeticComponent{ -1.0f } {}
};

struct ButtonColor {
	ButtonColor() = default;

	ButtonColor(const Color& color) :
		current_{ color }, default_{ color }, hover_{ color }, pressed_{ color } {}

	void SetToState(ButtonState state);

	[[nodiscard]] const Color& Get(ButtonState state) const;
	[[nodiscard]] Color& Get(ButtonState state);

	Color current_;
	Color default_;
	Color hover_;
	Color pressed_;
};

struct ButtonColorToggled : public ButtonColor {};

struct ButtonTint : public ButtonColor {
	ButtonTint() : ButtonColor{ color::White } {}
};

struct ButtonTintToggled : public ButtonTint {};

struct ButtonBorderColor : public ButtonColor {};

struct ButtonBorderColorToggled : public ButtonBorderColor {};

struct ButtonTexture {
	ButtonTexture() = default;

	ButtonTexture(const TextureKey& key) : default_{ key }, hover_{ key }, pressed_{ key } {}

	[[nodiscard]] const TextureKey& Get(ButtonState state) const;
	[[nodiscard]] TextureKey& Get(ButtonState state);

	TextureKey default_;
	TextureKey hover_;
	TextureKey pressed_;
};

struct ButtonTextureToggled : public ButtonTexture {};

struct ButtonText {
	ButtonText(
		const ecs::Entity& parent, ecs::Manager& manager, ButtonState state,
		const TextContent& text_content, const TextColor& text_color, const FontKey& font_key
	);

	[[nodiscard]] Color GetColor(ButtonState state) const;
	[[nodiscard]] std::string_view GetContent(ButtonState state) const;
	[[nodiscard]] const Text& Get(ButtonState state) const;
	[[nodiscard]] const Text& GetValid(ButtonState state) const;
	[[nodiscard]] Text& GetValid(ButtonState state);
	[[nodiscard]] Text& Get(ButtonState state);
	void Set(
		const ecs::Entity& parent, ecs::Manager& manager, ButtonState state,
		const TextContent& text_content, const TextColor& text_color, const FontKey& font_key
	);

	Text default_;
	Text hover_;
	Text pressed_;
};

struct ButtonTextToggled : public ButtonText {};

} // namespace impl

using ButtonCallback = std::function<void()>;

struct Button : public GameObject {
	Button() = default;
	explicit Button(ecs::Manager& manager);
	Button(const Button&)				 = delete;
	Button& operator=(const Button&)	 = delete;
	Button(Button&&) noexcept			 = default;
	Button& operator=(Button&&) noexcept = default;
	virtual ~Button()					 = default;

	Button& AddInteractableRect(
		const V2_float& size, Origin origin = Origin::Center, const V2_float& offset = {}
	);
	Button& AddInteractableCircle(float radius, const V2_float& offset = {});

	Button& SetRect(const V2_float& size, Origin origin = Origin::Center);
	Button& SetCircle(float radius);

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;

	Button& SetBackgroundColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Current) const;

	Button& SetTextColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextureKey GetTextureKey(ButtonState state = ButtonState::Current) const;

	Button& SetTextureKey(std::string_view texture_key, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Current) const;

	Button& SetTint(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string_view GetTextContent(ButtonState state = ButtonState::Current) const;

	Button& SetTextContent(std::string_view content, ButtonState state = ButtonState::Default);

	Button& SetText(
		std::string_view content, const Color& text_color = color::Black,
		std::string_view font_key = "", ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const Text& GetText(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text& GetText(ButtonState state = ButtonState::Current);

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
	friend class impl::RenderData;
	friend struct ToggleButton;

	// Internal constructor so that toggle button can avoid setting up callbacks with nullptr
	// internal_on_activate.
	Button(ecs::Manager& manager, bool);

	void Setup();
	void SetupCallbacks(const std::function<void()>& internal_on_activate);

	void StateChange(impl::InternalButtonState new_state);

	[[nodiscard]] static ButtonState GetState(const ecs::Entity& e);

	static void Activate(const ecs::Entity& e);
	static void StartHover(const ecs::Entity& e);
	static void StopHover(const ecs::Entity& e);
	static void StateChange(const ecs::Entity& e, impl::InternalButtonState new_state);
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

	// TODO: Add SetTextToggled(content, color, font, state);

	ToggleButton& SetTextColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextureKey GetTextureKeyToggled(ButtonState state = ButtonState::Default) const;

	ToggleButton& SetTextureKeyToggled(
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

private:
	static void Toggle(const ecs::Entity& e);
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