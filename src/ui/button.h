#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string_view>

#include "common/type_traits.h"
#include "components/draw.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/resource_manager.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/resources/text.h"

// TODO: Add serialization.

namespace ptgn {

class Button;

namespace impl {

class RenderData;

void SetupButton(Button& button);

void SetupButtonCallbacks(Button& button, const std::function<void()>& internal_on_activate);

} // namespace impl

class ToggleButton;
class ToggleButtonGroup;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

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

struct ButtonDisabledTextureKey : public TextureHandle {
	using TextureHandle::TextureHandle;
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

	ButtonTexture(const TextureHandle& key) : default_{ key }, hover_{ key }, pressed_{ key } {}

	[[nodiscard]] const TextureHandle& Get(ButtonState state) const;
	[[nodiscard]] TextureHandle& Get(ButtonState state);

	TextureHandle default_;
	TextureHandle hover_;
	TextureHandle pressed_;
};

struct ButtonTextureToggled : public ButtonTexture {
	using ButtonTexture::ButtonTexture;
};

struct ButtonText {
	ButtonText(
		Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
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
		Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontKey& font_key
	);

	Text default_;
	Text hover_;
	Text pressed_;
};

struct ButtonTextToggled : public ButtonText {
	using ButtonText::ButtonText;
};

struct ButtonSize : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct ButtonRadius : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

} // namespace impl

using ButtonCallback = std::function<void()>;

class Button : public Entity, public Drawable<Button> {
public:
	Button() = default;
	Button(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	Button& AddInteractableRect(
		const V2_float& size, Origin origin = Origin::Center, const V2_float& offset = {}
	);
	Button& AddInteractableCircle(float radius, const V2_float& offset = {});

	// @param size {} results in texture sized button.
	Button& SetSize(const V2_float& size = {});

	// @param radius 0.0f results in texture sized button.
	Button& SetRadius(float radius = 0.0f);

	// These allow for manually triggering button callback events.
	virtual void Activate();
	virtual void StartHover();
	virtual void StopHover();

	[[nodiscard]] ButtonState GetState() const;

	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;

	Button& SetBackgroundColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] const TextureHandle& GetTextureKey(ButtonState state = ButtonState::Current)
		const;

	Button& SetTextureKey(
		const TextureHandle& texture_key, ButtonState state = ButtonState::Default
	);

	Button& SetDisabledTextureKey(const TextureHandle& texture_key);

	[[nodiscard]] const TextureHandle& GetDisabledTextureKey() const;

	[[nodiscard]] Color GetButtonTint(ButtonState state = ButtonState::Current) const;

	Button& SetButtonTint(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Current) const;

	Button& SetTextColor(const TextColor& text_color, ButtonState state = ButtonState::Default);

	[[nodiscard]] std::string_view GetTextContent(ButtonState state = ButtonState::Current) const;

	Button& SetTextContent(const TextContent& content, ButtonState state = ButtonState::Default);

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
		const TextContent& content, const TextColor& text_color = color::Black,
		const FontKey& font_key = {}, ButtonState state = ButtonState::Default
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
	friend class ToggleButton;
	friend class ToggleButtonGroup;
	friend void impl::SetupButtonCallbacks(
		Button& button, const std::function<void()>& internal_on_activate
	);

	Button& OnInternalActivate(const ButtonCallback& callback);

	void StateChange(impl::InternalButtonState new_state);
};

class ToggleButton : public Button {
public:
	ToggleButton() = default;
	using Button::Button;

	void Activate() final;

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& SetToggled(bool toggled);

	ToggleButton& Toggle();

	[[nodiscard]] Color GetBackgroundColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBackgroundColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const TextureHandle& GetTextureKeyToggled(
		ButtonState state = ButtonState::Default
	) const;

	ToggleButton& SetTextureKeyToggled(
		const TextureHandle& texture_key, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetButtonTintToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetButtonTintToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextColorToggled(
		const TextColor& text_color, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] std::string_view GetTextContentToggled(ButtonState state = ButtonState::Current)
		const;

	ToggleButton& SetTextContentToggled(
		const TextContent& content, ButtonState state = ButtonState::Default
	);

	ToggleButton& SetTextToggled(
		const TextContent& content, const TextColor& text_color = color::Black,
		const FontKey& font_key = "", ButtonState state = ButtonState::Default
	);

	[[nodiscard]] const Text& GetTextToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text& GetTextToggled(ButtonState state = ButtonState::Current);

	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBorderColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);
};

class ToggleButtonGroup : public MapManager<ToggleButton, std::string_view, std::string, false> {
public:
	ToggleButtonGroup()										   = default;
	~ToggleButtonGroup() override							   = default;
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
		button.OnInternalActivate([this, e = button]() mutable {
			ForEachValue([](ToggleButton& b) { b.SetToggled(false); });
			e.SetToggled(true);
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

[[nodiscard]] Button CreateButton(Manager& manager, const ButtonCallback& on_activate = {});

[[nodiscard]] Button CreateTextButton(
	Manager& manager, const TextContent& text_content, const TextColor& text_color = color::Black,
	const ButtonCallback& on_activate = {}
);

// @param toggled Whether or not the button start in the toggled state.
[[nodiscard]] ToggleButton CreateToggleButton(
	Manager& manager, bool toggled = false, const ButtonCallback& on_activate = {}
);

} // namespace ptgn