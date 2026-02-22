#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "core/component.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scripting/script.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Button;
class Renderer;
class Scene;
class ToggleButton;
class ToggleButtonGroup;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

namespace impl {

enum class InternalButtonState {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

class InternalButtonScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;

private:
	void OnMouseMoveOver();

	void OnMouseMoveOut();

	void OnMousePressedOver(Mouse mouse);

	void OnMousePressedOut(Mouse mouse);

	void OnMouseReleasedOver(Mouse mouse);

	void OnMouseReleasedOut(Mouse mouse);
};

// TODO: Fix.
// class ToggleButtonScript : public Script {
// public:
//	void OnEvent(EventDispatcher d) override;
//};

struct ButtonActivate : public Event<ButtonActivate> {};

struct ButtonHoverStart : public Event<ButtonHoverStart> {};

struct ButtonHoverStop : public Event<ButtonHoverStop> {};

struct ButtonHover : public Event<ButtonHover> {};

// TODO: Fix.
// struct AnimatedButtonScript : public Script {
//	AnimatedButtonScript() = default;
//
//	AnimatedButtonScript(
//		const Animation& activate_animation, const Animation& hover_animation = {},
//		bool force_start_on_activate = true, bool force_start_on_hover_start = true,
//		bool stop_on_hover_stop = true
//	) :
//		activate_animation{ activate_animation },
//		hover_animation{ hover_animation },
//		force_start_on_activate{ force_start_on_activate },
//		force_start_on_hover_start{ force_start_on_hover_start },
//		stop_on_hover_stop{ stop_on_hover_stop } {}
//
//	Animation activate_animation;
//	Animation hover_animation;
//
//	bool force_start_on_activate{ true };
//
//	bool force_start_on_hover_start{ true };
//	bool stop_on_hover_stop{ true };
//
//	void OnButtonHoverStart() override {
//		if (hover_animation) {
//			hover_animation.Start(force_start_on_hover_start);
//		}
//	}
//
//	// void OnButtonHover() {}
//
//	void OnButtonHoverStop() override {
//		if (hover_animation && stop_on_hover_stop) {
//			hover_animation.Stop();
//		}
//	}
//
//	void OnButtonActivate() override {
//		if (activate_animation) {
//			activate_animation.Start(force_start_on_activate);
//		}
//	}
//};

struct ButtonToggled {};

struct ButtonDisabledTexture : public Texture {
	using Texture::Texture;
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

	explicit ButtonColor(Color color) :
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

	ButtonTexture(Texture texture) : default_{ texture }, hover_{ texture }, pressed_{ texture } {}

	[[nodiscard]] Texture Get(ButtonState state) const;

	Texture default_;
	Texture hover_;
	Texture pressed_;

	// TODO: Fix.
	// PTGN_SERIALIZER_REGISTER_NAMED(
	//	ButtonTexture, KeyValue("default", default_), KeyValue("hover", hover_),
	//	KeyValue("pressed", pressed_)
	//)
};

struct ButtonTextureToggled : public ButtonTexture {
	using ButtonTexture::ButtonTexture;
};

struct ButtonText {
	ButtonText()								 = default;
	ButtonText& operator=(ButtonText&&) noexcept = default;
	ButtonText(ButtonText&&) noexcept			 = default;
	ButtonText& operator=(const ButtonText&)	 = delete;
	ButtonText(const ButtonText&)				 = delete;
	~ButtonText()								 = default;

	ButtonText(
		Entity parent, Scene& scene, ButtonState state, std::string_view text_content,
		Color text_color, std::optional<float> font_size, std::optional<Font> font,
		const TextProperties& text_properties
	);

	[[nodiscard]] Color GetTextColor(ButtonState state) const;
	[[nodiscard]] std::string GetTextContent(ButtonState state) const;
	[[nodiscard]] float GetFontSize(ButtonState state) const;
	[[nodiscard]] TextJustify GetTextJustify(ButtonState state) const;
	[[nodiscard]] Entity Get(ButtonState state) const;
	[[nodiscard]] Entity GetValid(ButtonState state) const;

	void Set(
		Entity parent, Scene& scene, ButtonState state, std::string_view text_content,
		Color text_color, std::optional<float> font_size, std::optional<Font> font,
		const TextProperties& text_properties
	);

	GameObject default_;
	GameObject hover_;
	GameObject pressed_;
};

struct ButtonTextToggled : public ButtonText {
	using ButtonText::ButtonText;
};

struct ButtonEnabled {
	bool activate{ true };
	bool hover{ true };

	PTGN_SERIALIZER_REGISTER(ButtonEnabled, activate, hover)
};

class ButtonDraw {
public:
	static void Draw(Renderer& renderer, Entity entity);
};

} // namespace impl

// Set button callback scripts.
void OnButtonActivate(Entity button, const std::function<void()>& callback);
void OnButtonHover(Entity button, const std::function<void()>& callback);
void OnButtonHoverStart(Entity button, const std::function<void()>& callback);
void OnButtonHoverStop(Entity button, const std::function<void()>& callback);

// @param size {} results in texture sized button.
void SetButtonSize(Entity button, std::optional<V2_float> size = {});

// @return {} if no size is specified via SetSize, SetRadius, or button texture. If radius,
// returns 2.0f * V2_float{ radius, radius }
[[nodiscard]] V2_float GetButtonSize(Entity button);

// @param radius {} results in texture sized button.
void SetButtonRadius(Entity button, std::optional<float> radius = {});

void EnableButton(Entity button, bool enable_hover = true, bool reset_state = true);
void DisableButton(Entity button, bool disable_hover = true, bool reset_state = true);
void SetButtonEnabled(
	Entity button, bool enable_activation = true, bool enable_hover = true, bool reset_state = true
);

// @param check_for_hover_enabled If true, checks for button hovering being enabled instead.
// @return True if the button activation is enabled, false otherwise.
[[nodiscard]] bool IsButtonEnabled(Entity button, bool check_for_hover_enabled = false);

// Manual button script triggers.
// Called when the mouse is clicked over the button.
void ButtonActivate(Entity button);
// Called once when hovering starts (mouse enters button).
void ButtonStartHover(Entity button);
// Called continuously when hovering (including when hover starts).
void ButtonContinueHover(Entity button);
// Called once when hovering stops (mouse exits button).
void ButtonStopHover(Entity button);

[[nodiscard]] ButtonState GetButtonState(Entity button);

[[nodiscard]] Color GetButtonBackgroundColor(
	Entity button, ButtonState state = ButtonState::Current
);

void SetButtonBackgroundColor(Entity button, Color color, ButtonState state = ButtonState::Default);

[[nodiscard]] Texture GetButtonTexture(Entity button, ButtonState state = ButtonState::Current);

void SetButtonTexture(Entity button, Texture texture, ButtonState state = ButtonState::Default);

void SetButtonDisabledTextur(Entity button, Texture texture);

[[nodiscard]] Texture GetButtonDisabledTexture(Entity button);

[[nodiscard]] Color GetButtonTint(Entity button, ButtonState state = ButtonState::Current);

void SetButtonTint(Entity button, Color tint, ButtonState state = ButtonState::Default);

[[nodiscard]] Color GetButtonTextColor(Entity button, ButtonState state = ButtonState::Current);

void SetButtonTextColor(Entity button, Color text_color, ButtonState state = ButtonState::Default);

[[nodiscard]] std::string GetButtonTextContent(
	Entity button, ButtonState state = ButtonState::Current
);

void SetButtonTextContent(
	Entity button, std::string_view content, ButtonState state = ButtonState::Default
);

[[nodiscard]] TextJustify GetButtonTextJustify(
	Entity button, ButtonState state = ButtonState::Current
);

void SetButtonTextJustify(
	Entity button, TextJustify justify, ButtonState state = ButtonState::Default
);

[[nodiscard]] V2_float GetButtonTextFixedSize(Entity button);

// If either axis of the text size is {}, it is stretched to fit the entire size of the button
// rectangle (along that axis).
void SetButtonTextFixedSize(
	Entity button, std::optional<float> x = {}, std::optional<float> y = {}
);

// Make it so the button text no longer has a fixed size,
// this will cause the text to stretch based its the font size and wrap settings.
void ClearButtonTextFixedSize(Entity button);

[[nodiscard]] float GetButtonFontSize(Entity button, ButtonState state = ButtonState::Current);

void SetButtonFontSize(Entity button, float font_size, ButtonState state = ButtonState::Default);

void SetButtonText(
	Entity button, std::string_view content, Color text_color = color::Black,
	std::optional<float> font_size = {}, std::optional<Font> font = {},
	const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
);

[[nodiscard]] Entity GetButtonText(Entity button, ButtonState state = ButtonState::Current);

[[nodiscard]] Color GetButtonBorderColor(Entity button, ButtonState state = ButtonState::Current);

void SetButtonBorderColor(Entity button, Color color, ButtonState state = ButtonState::Default);

[[nodiscard]] float GetButtonBackgroundLineWidth(Entity button);

// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
// width.
void SetButtonBackgroundLineWidth(Entity button, float line_width);

[[nodiscard]] float GetButtonBorderWidth(Entity button);

void SetButtonBorderWidth(Entity button, float line_width);

[[nodiscard]] impl::InternalButtonState GetButtonInternalState(Entity button);

// TODO: Fix.
// class ToggleButton : public Button {
// public:
//	ToggleButton() = default;
//	using Button::Button;
//
//	[[nodiscard]] bool IsToggled() const;
//
//	Entity SetToggled(bool toggled);
//
//	Entity Toggle();
//
//	[[nodiscard]] Color GetBackgroundColorToggled(ButtonState state = ButtonState::Current) const;
//
//	Entity SetBackgroundColorToggled(
//		const Color& color, ButtonState state = ButtonState::Default
//	);
//
//	[[nodiscard]] const Texture& GetTextureKeyToggled(ButtonState state = ButtonState::Default)
//		const;
//
//	Entity SetTextureKeyToggled(
//		const Texture& texture, ButtonState state = ButtonState::Default
//	);
//
//	[[nodiscard]] Color GetButtonTintToggled(ButtonState state = ButtonState::Current) const;
//
//	Entity SetButtonTintToggled(
//		const Color& color, ButtonState state = ButtonState::Default
//	);
//
//	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;
//
//	Entity SetTextColorToggled(Color text_color, ButtonState state = ButtonState::Default);
//
//	[[nodiscard]] std::string GetTextContentToggled(ButtonState state = ButtonState::Current) const;
//
//	Entity SetTextContentToggled(
//		std::string_view content, ButtonState state = ButtonState::Default
//	);
//
//	Entity SetTextToggled(
//		std::string_view content, Color text_color = color::Black,
//		std::optional<float> font_size = {}, std::optional<Font> font = {},
//		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
//	);
//
//	[[nodiscard]] Text GetTextToggled(ButtonState state = ButtonState::Current) const;
//
//	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;
//
//	Entity SetBorderColorToggled(
//		const Color& color, ButtonState state = ButtonState::Default
//	);
// };
//
// struct ToggleButtonGroupKey : public HashComponent {
//	using HashComponent::HashComponent;
// };

// TODO: Fix.
// namespace std {
//
// template <>
// struct hash<ptgn::ToggleButtonGroupKey> {
//	std::size_t operator()(const ptgn::ToggleButtonGroupKey& group_key) const {
//		return group_key.GetHash();
//	}
//};
//
//} // namespace std

// TODO: Fix.
// namespace ptgn {
//
// namespace impl {
//
// struct ToggleButtonGroupInfo {
//	ToggleButtonGroupInfo()											   = default;
//	~ToggleButtonGroupInfo()										   = default;
//	ToggleButtonGroupInfo(ToggleButtonGroupInfo&&) noexcept			   = default;
//	ToggleButtonGroupInfo& operator=(ToggleButtonGroupInfo&&) noexcept = default;
//	ToggleButtonGroupInfo(const ToggleButtonGroupInfo&)				   = delete;
//	ToggleButtonGroupInfo& operator=(const ToggleButtonGroupInfo&)	   = delete;
//
//	ToggleButtonGroupKey active;
//	std::unordered_map<ToggleButtonGroupKey, GameObject<ToggleButton>> buttons;
//};
//
//} // namespace impl
//
// Entity AddToToggleButtonGroup(const ToggleButtonGroupKey& button_key, Entity& toggle_button);
//
// void RemoveFromToggleButtonGroup(const ToggleButtonGroupKey& button_key);
//
// void SetActive(const ToggleButtonGroupKey& button_key);
//
//// @return Active button, or null entity if no button is active.
// ToggleButton GetActive() const;
//
// void AddToggleScript(Entity target);
//
// namespace impl {
//
// class ToggleButtonGroupScript : public Script {
// public:
//	ToggleButtonGroupScript() = default;
//	explicit ToggleButtonGroupScript(const ToggleButtonGroup& group);
//
//	void OnEvent(EventDispatcher d) override;
//
//	ToggleButtonGroup toggle_button_group;
// };
//
// } // namespace impl

inline std::ostream& operator<<(std::ostream& os, ButtonState state) {
	switch (state) {
		using enum ptgn::ButtonState;
		case Default: os << "Default"; break;
		case Hover:	  os << "Hover"; break;
		case Pressed: os << "Pressed"; break;
		default:	  PTGN_ERROR("Invalid button state");
	}
	return os;
}

inline std::ostream& operator<<(std::ostream& os, impl::InternalButtonState state) {
	switch (state) {
		using enum ptgn::impl::InternalButtonState;
		case IdleDown:	   os << "Idle Down"; break;
		case IdleUp:	   os << "Idle Up"; break;
		case Hover:		   os << "Hover"; break;
		case HoverPressed: os << "Hover Pressed"; break;
		case Pressed:	   os << "Pressed"; break;
		case HeldOutside:  os << "Held Outside"; break;
		default:		   PTGN_ERROR("Invalid internal button state");
	}
	return os;
}

Button CreateButton(Scene& scene);

Button CreateTextButton(
	Scene& scene, std::string_view text_content, Color text_color = color::Black
);

// TODO: Fix.
// @param toggled Whether or not the button start in the toggled state.
// ToggleButton CreateToggleButton(Scene& scene, bool toggled = false);

// TODO: Fix.
// ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

// TODO: Fix.
// Button CreateAnimatedButton(
//	Scene& scene, V2_float button_size, const Animation& activate_animation,
//	const Animation& hover_animation = {}, bool force_start_on_activate = true,
//	bool force_start_on_hover_start = true, bool stop_on_hover_stop = true
//);

PTGN_SERIALIZE_ENUM(
	ButtonState, { { ButtonState::Default, "default" },
				   { ButtonState::Hover, "hover" },
				   { ButtonState::Pressed, "pressed" },
				   { ButtonState::Current, "current" } }
);

namespace impl {

PTGN_SERIALIZE_ENUM(
	InternalButtonState, { { InternalButtonState::IdleUp, "idle_up" },
						   { InternalButtonState::Hover, "hover" },
						   { InternalButtonState::Pressed, "pressed" },
						   { InternalButtonState::HeldOutside, "held_outside" },
						   { InternalButtonState::IdleDown, "idle_down" },
						   { InternalButtonState::HoverPressed, "hover_pressed" } }
);

} // namespace impl

PTGN_REGISTER_DRAWABLE(impl::ButtonDraw);

} // namespace ptgn