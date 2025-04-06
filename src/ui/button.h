#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string_view>

#include "components/draw.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/resource_manager.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/text.h"
#include "utility/log.h"
#include "utility/type_traits.h"

namespace ptgn {

struct ToggleButton;
class ToggleButtonGroup;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

class RenderData;

struct ButtonTag {};

struct ButtonHoverStart : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonHoverStop : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonActivate : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

struct InternalButtonActivate : public CallbackComponent<> {
	using CallbackComponent::CallbackComponent;
};

struct ButtonToggled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct ButtonDisabledTextureKey : public TextureKey {
	using TextureKey::TextureKey;
};

enum class InternalButtonState : std::size_t {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

struct ButtonTextFixedSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
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

struct ButtonColorToggled : public ButtonColor {
	using ButtonColor::ButtonColor;
};

struct ButtonTint : public ButtonColor {
	using ButtonColor::ButtonColor;

	ButtonTint() : ButtonColor{ color::White } {}
};

struct ButtonTintToggled : public ButtonTint {
	using ButtonTint::ButtonTint;
};

struct ButtonBorderColor : public ButtonColor {
	using ButtonColor::ButtonColor;
};

struct ButtonBorderColorToggled : public ButtonBorderColor {
	using ButtonBorderColor::ButtonBorderColor;
};

struct ButtonTexture {
	ButtonTexture() = default;

	ButtonTexture(const TextureKey& key) : default_{ key }, hover_{ key }, pressed_{ key } {}

	[[nodiscard]] const TextureKey& Get(ButtonState state) const;
	[[nodiscard]] TextureKey& Get(ButtonState state);

	TextureKey default_;
	TextureKey hover_;
	TextureKey pressed_;
};

struct ButtonTextureToggled : public ButtonTexture {
	using ButtonTexture::ButtonTexture;
};

struct ButtonText {
	ButtonText(
		const Entity& parent, Manager& manager, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontKey& font_key
	);

	[[nodiscard]] Color GetTextColor(ButtonState state) const;
	[[nodiscard]] std::string_view GetTextContent(ButtonState state) const;
	[[nodiscard]] std::int32_t GetFontSize(ButtonState state) const;
	[[nodiscard]] TextJustify GetTextJustify(ButtonState state) const;
	[[nodiscard]] const Text& Get(ButtonState state) const;
	[[nodiscard]] const Text& GetValid(ButtonState state) const;
	[[nodiscard]] Text& GetValid(ButtonState state);
	[[nodiscard]] Text& Get(ButtonState state);
	void Set(
		const Entity& parent, Manager& manager, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontKey& font_key
	);

	Text default_;
	Text hover_;
	Text pressed_;
};

struct ButtonTextToggled : public ButtonText {
	using ButtonText::ButtonText;
};

} // namespace impl

using ButtonCallback = std::function<void()>;

struct Button : public GameObject {
	Button() = default;
	explicit Button(Manager& manager);
	Button(const Button&)				 = delete;
	Button& operator=(const Button&)	 = delete;
	Button(Button&&) noexcept			 = default;
	Button& operator=(Button&&) noexcept = default;
	virtual ~Button()					 = default;

	Button& AddInteractableRect(
		const V2_float& size, Origin origin = Origin::Center, const V2_float& offset = {}
	);
	Button& AddInteractableCircle(float radius, const V2_float& offset = {});

	// @param size {} Results in texture sized button.
	Button& SetRect(const V2_float& size = {}, Origin origin = Origin::Center);

	// @param radius 0.0f results in texture sized button.
	Button& SetCircle(float radius = 0.0f);

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;

	Button& SetBackgroundColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] const TextureKey& GetTextureKey(ButtonState state = ButtonState::Current) const;

	Button& SetTextureKey(std::string_view texture_key, ButtonState state = ButtonState::Default);

	Button& SetDisabledTextureKey(std::string_view texture_key);

	[[nodiscard]] const TextureKey& GetDisabledTextureKey() const;

	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Current) const;

	Button& SetTint(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Current) const;

	Button& SetTextColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string_view GetTextContent(ButtonState state = ButtonState::Current) const;

	Button& SetTextContent(std::string_view content, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextJustify GetTextJustify(ButtonState state = ButtonState::Current) const;

	Button& SetTextJustify(const TextJustify& justify, ButtonState state = ButtonState::Default);

	[[nodiscard]] V2_float GetTextFixedSize() const;

	// (default: unscaled text size). If either axis of the text size
	// is zero, it is stretched to fit the entire size of the button rectangle (along that axis).
	Button& SetTextFixedSize(const V2_float& size);

	// Make it so the button text no longer has a fixed size, this will cause the text to stretch
	// based its the font size and wrap settings.
	Button& ClearTextFixedSize();

	[[nodiscard]] std::int32_t GetFontSize(ButtonState state = ButtonState::Current) const;

	Button& SetFontSize(std::int32_t font_size, ButtonState state = ButtonState::Default);

	Button& SetText(
		std::string_view content, const Color& text_color = color::Black,
		std::string_view font_key = "", ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const Text& GetText(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text& GetText(ButtonState state = ButtonState::Current);

	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Current) const;

	Button& SetBorderColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] float GetBackgroundLineWidth() const;

	// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
	// width.
	Button& SetBackgroundLineWidth(float line_width);

	[[nodiscard]] float GetBorderWidth() const;

	Button& SetBorderWidth(float line_width);

	Button& OnHoverStart(const ButtonCallback& callback);
	Button& OnHoverStop(const ButtonCallback& callback);
	Button& OnActivate(const ButtonCallback& callback);

	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

private:
	friend class impl::RenderData;
	friend struct ToggleButton;
	friend class ToggleButtonGroup;

	// Internal constructor so that toggle button can avoid setting up callbacks with nullptr
	// internal_on_activate.
	Button(Manager& manager, bool);

	Button& OnInternalActivate(const ButtonCallback& callback);

	void Setup();
	void SetupCallbacks(const std::function<void()>& internal_on_activate);

	void StateChange(impl::InternalButtonState new_state);

	[[nodiscard]] static ButtonState GetState(const Entity& e);

	static void Activate(const Entity& e);
	static void StartHover(const Entity& e);
	static void StopHover(const Entity& e);
	static void StateChange(const Entity& e, impl::InternalButtonState new_state);
};

struct ToggleButton : public Button {
	ToggleButton() = default;
	ToggleButton(Manager& manager, bool toggled = false);

	void Activate() final;

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& SetToggled(bool toggled);

	ToggleButton& Toggle();

	[[nodiscard]] Color GetBackgroundColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBackgroundColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const TextureKey& GetTextureKeyToggled(ButtonState state = ButtonState::Default)
		const;

	ToggleButton& SetTextureKeyToggled(
		std::string_view texture_key, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetTintToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTintToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextColorToggled(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string_view GetTextContentToggled(ButtonState state = ButtonState::Current)
		const;

	ToggleButton& SetTextContentToggled(
		std::string_view content, ButtonState state = ButtonState::Default
	);

	ToggleButton& SetTextToggled(
		std::string_view content, const Color& text_color = color::Black,
		std::string_view font_key = "", ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const Text& GetTextToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text& GetTextToggled(ButtonState state = ButtonState::Current);

	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBorderColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);

private:
	friend class ToggleButtonGroup;

	static void SetToggled(Entity e, bool toggled);
	static void Toggle(Entity e);
};

class ToggleButtonGroup : public MapManager<ToggleButton, std::string_view, std::string, false> {
public:
	ToggleButtonGroup()										   = default;
	virtual ~ToggleButtonGroup() override					   = default;
	ToggleButtonGroup(ToggleButtonGroup&&) noexcept			   = default;
	ToggleButtonGroup& operator=(ToggleButtonGroup&&) noexcept = default;
	ToggleButtonGroup(const ToggleButtonGroup&)				   = delete;
	ToggleButtonGroup& operator=(const ToggleButtonGroup&)	   = delete;

	template <typename TKey, typename... TArgs, tt::constructible<ToggleButton, TArgs...> = true>
	ToggleButton& Load(const TKey& key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(key) };
		ToggleButton& button{
			MapManager::Load(key, ToggleButton{ std::forward<TArgs>(constructor_args)... })
		};
		// Toggle all other buttons when one is pressed.
		button.OnInternalActivate([this, e = button.GetEntity()]() {
			ForEachValue([](const ToggleButton& b) { ToggleButton::SetToggled(b, false); });
			ToggleButton::SetToggled(e, true);
		});
		return button;
	}
};

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