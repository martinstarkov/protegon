#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "core/script.h"
#include "debug/log.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;
class Button;
class ToggleButton;
class ToggleButtonGroup;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

class RenderData;
class ToggleButtonGroupScript;

enum class InternalButtonState {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

class ButtonScript : public ptgn::Script<ButtonScript> {
public:
	void OnMouseEnter(V2_int mouse_position) override;

	void OnMouseLeave(V2_int mouse_position) override;

	void OnMouseDown(V2_int mouse_position, Mouse mouse) override;

	void OnMouseDownOutside(Mouse mouse) override;

	void OnMouseUp(V2_int mouse_position, Mouse mouse) override;

	void OnMouseUpOutside(Mouse mouse) override;
};

class ToggleButtonScript : public ptgn::Script<ToggleButtonScript> {
public:
	void OnButtonActivate() override;
};

class ButtonActivateScript : public ptgn::Script<ButtonActivateScript> {
public:
	ButtonActivateScript() = default;

	explicit ButtonActivateScript(const std::function<void()>& on_activate_callback) :
		on_activate{ on_activate_callback } {}

	void OnButtonActivate() override {
		if (on_activate) {
			std::invoke(on_activate);
		}
	}

	std::function<void()> on_activate;
};

struct ButtonToggled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct ButtonDisabledTextureKey : public TextureHandle {
	using TextureHandle::TextureHandle;
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

	PTGN_SERIALIZER_REGISTER_NAMED(
		ButtonColor, KeyValue("current", current_), KeyValue("default", default_),
		KeyValue("hover", hover_), KeyValue("pressed", pressed_)
	)
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

	PTGN_SERIALIZER_REGISTER_NAMED(
		ButtonTexture, KeyValue("default", default_), KeyValue("hover", hover_),
		KeyValue("pressed", pressed_)
	)
};

struct ButtonTextureToggled : public ButtonTexture {
	using ButtonTexture::ButtonTexture;
};

struct ButtonText {
	ButtonText() = default;

	ButtonText(
		Entity parent, Scene& scene, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& text_properties
	);

	ButtonText& operator=(const ButtonText&)	 = default;
	ButtonText(const ButtonText&)				 = default;
	ButtonText& operator=(ButtonText&&) noexcept = default;
	ButtonText(ButtonText&&) noexcept			 = default;

	~ButtonText();

	[[nodiscard]] TextColor GetTextColor(ButtonState state) const;
	[[nodiscard]] TextContent GetTextContent(ButtonState state) const;
	[[nodiscard]] FontSize GetFontSize(ButtonState state) const;
	[[nodiscard]] TextJustify GetTextJustify(ButtonState state) const;
	[[nodiscard]] Text Get(ButtonState state) const;
	[[nodiscard]] Text GetValid(ButtonState state) const;

	void Set(
		Entity parent, Scene& scene, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& text_properties
	);

	Text default_;
	Text hover_;
	Text pressed_;

	PTGN_SERIALIZER_REGISTER_NAMED(
		ButtonText, KeyValue("default", default_), KeyValue("hover", hover_),
		KeyValue("pressed", pressed_)
	)
};

struct ButtonTextToggled : public ButtonText {
	using ButtonText::ButtonText;
};

struct ButtonEnabled : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	ButtonEnabled() : ArithmeticComponent{ true } {}
};

} // namespace impl

class Button : public Entity, public Drawable<Button> {
public:
	Button() = default;
	Button(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	// Will set the script for when the button is activated.
	Button& OnActivate(const std::function<void()>& on_activate_callback);

	// @param size {} results in texture sized button.
	Button& SetSize(const V2_float& size = {});

	// @return {} if no size is specified via SetSize, SetRadius, or button texture. If radius,
	// returns 2.0f * V2_float{ radius, radius }
	[[nodiscard]] V2_float GetSize() const;

	// @param radius 0.0f results in texture sized button.
	Button& SetRadius(float radius = 0.0f);

	Button& Enable();
	Button& Disable();
	Button& SetEnabled(bool enabled = true);

	// Manual button script triggers.
	void Activate();
	void StartHover();
	void StopHover();

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

	[[nodiscard]] TextColor GetTextColor(ButtonState state = ButtonState::Current) const;

	Button& SetTextColor(const TextColor& text_color, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextContent GetTextContent(ButtonState state = ButtonState::Current) const;

	Button& SetTextContent(const TextContent& content, ButtonState state = ButtonState::Default);

	[[nodiscard]] TextJustify GetTextJustify(ButtonState state = ButtonState::Current) const;

	Button& SetTextJustify(const TextJustify& justify, ButtonState state = ButtonState::Default);

	// TODO: Fix Fixed size functionality.
	//[[nodiscard]] V2_float GetTextFixedSize() const;
	// (default: unscaled text size). If either axis of the text size
	// is zero, it is stretched to fit the entire size of the button rectangle (along that axis).
	// Button& SetTextFixedSize(const V2_float& size);
	// Make it so the button text no longer has a fixed size, this will cause the text to stretch
	// based its the font size and wrap settings.
	// Button& ClearTextFixedSize();

	[[nodiscard]] FontSize GetFontSize(ButtonState state = ButtonState::Current) const;

	Button& SetFontSize(const FontSize& font_size, ButtonState state = ButtonState::Default);

	Button& SetText(
		const TextContent& content, const TextColor& text_color = color::Black,
		const FontSize& font_size = {}, const ResourceHandle& font_key = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Text GetText(ButtonState state = ButtonState::Current) const;

	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Current) const;

	Button& SetBorderColor(const Color& color, ButtonState state = ButtonState::Default);

	[[nodiscard]] float GetBackgroundLineWidth() const;

	// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
	// width.
	Button& SetBackgroundLineWidth(float line_width);

	[[nodiscard]] float GetBorderWidth() const;

	Button& SetBorderWidth(float line_width);

	[[nodiscard]] impl::InternalButtonState GetInternalState() const;
};

class ToggleButton : public Button {
public:
	ToggleButton() = default;
	using Button::Button;

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

	[[nodiscard]] TextColor GetTextColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextColorToggled(
		const TextColor& text_color, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] TextContent GetTextContentToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetTextContentToggled(
		const TextContent& content, ButtonState state = ButtonState::Default
	);

	ToggleButton& SetTextToggled(
		const TextContent& content, const TextColor& text_color = color::Black,
		const FontSize& font_size = {}, const ResourceHandle& font_key = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
	);

	[[nodiscard]] Text GetTextToggled(ButtonState state = ButtonState::Current) const;

	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& SetBorderColorToggled(
		const Color& color, ButtonState state = ButtonState::Default
	);
};

namespace impl {

struct ToggleButtonGroupInfo {
	ToggleButtonGroupInfo()											   = default;
	ToggleButtonGroupInfo(const ToggleButtonGroupInfo&)				   = delete;
	ToggleButtonGroupInfo& operator=(const ToggleButtonGroupInfo&)	   = delete;
	ToggleButtonGroupInfo(ToggleButtonGroupInfo&&) noexcept			   = default;
	ToggleButtonGroupInfo& operator=(ToggleButtonGroupInfo&&) noexcept = default;
	~ToggleButtonGroupInfo();

	std::unordered_map<std::size_t, ToggleButton> buttons_;
};

} // namespace impl

class ToggleButtonGroup : public Entity {
public:
	ToggleButton& Load(std::string_view button_key, ToggleButton&& toggle_button);

	void Unload(std::string_view button_key);

	// TODO: Add more utility functions.

private:
	friend class impl::ToggleButtonGroupScript;

	void AddToggleScript(ToggleButton& target);
};

namespace impl {

class ToggleButtonGroupScript : public ptgn::Script<ToggleButtonGroupScript> {
public:
	ToggleButtonGroupScript() = default;
	explicit ToggleButtonGroupScript(const ToggleButtonGroup& group);

	void OnButtonActivate() override;

	ToggleButtonGroup toggle_button_group;
};

} // namespace impl

inline std::ostream& operator<<(std::ostream& os, ButtonState state) {
	switch (state) {
		case ButtonState::Default: os << "Default"; break;
		case ButtonState::Hover:   os << "Hover"; break;
		case ButtonState::Pressed: os << "Pressed"; break;
		default:				   PTGN_ERROR("Invalid button state")
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
		default:									  PTGN_ERROR("Invalid internal button state")
	}
	return os;
}

Button CreateButton(Scene& scene);

Button CreateTextButton(
	Scene& scene, const TextContent& text_content, const TextColor& text_color = color::Black
);

// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(Scene& scene, bool toggled = false);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

PTGN_SERIALIZER_REGISTER_ENUM(
	ButtonState, { { ButtonState::Default, "default" },
				   { ButtonState::Hover, "hover" },
				   { ButtonState::Pressed, "pressed" },
				   { ButtonState::Current, "current" } }
);

namespace impl {

PTGN_SERIALIZER_REGISTER_ENUM(
	InternalButtonState, { { InternalButtonState::IdleUp, "idle_up" },
						   { InternalButtonState::Hover, "hover" },
						   { InternalButtonState::Pressed, "pressed" },
						   { InternalButtonState::HeldOutside, "held_outside" },
						   { InternalButtonState::IdleDown, "idle_down" },
						   { InternalButtonState::HoverPressed, "hover_pressed" } }
);

} // namespace impl

} // namespace ptgn