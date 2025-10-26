#pragma once

#include <cstdint>
#include <functional>
#include <ostream>
#include <string_view>
#include <unordered_map>

#include "core/ecs/components/animation.h"
#include "core/ecs/components/drawable.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/entity.h"
#include "core/ecs/game_object.h"
#include "core/input/mouse.h"
#include "core/scripting/script.h"
#include "debug/core/log.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/material/texture.h"
#include "renderer/text/text.h"
#include "serialization/json/enum.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Button;
class Manager;
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

class InternalButtonScript : public Script<InternalButtonScript, MouseScript> {
public:
	void OnMouseMoveOver() override;

	void OnMouseMoveOut() override;

	void OnMouseDownOver(Mouse mouse) override;

	void OnMouseDownOut(Mouse mouse) override;

	void OnMouseUpOver(Mouse mouse) override;

	void OnMouseUpOut(Mouse mouse) override;
};

class ToggleButtonScript : public Script<ToggleButtonScript, ButtonScript> {
public:
	void OnButtonActivate() override;
};

#define PTGN_DEFINE_BUTTON_SCRIPT(SCRIPT_NAME)                                     \
	class SCRIPT_NAME##Script : public Script<SCRIPT_NAME##Script, ButtonScript> { \
	public:                                                                        \
		SCRIPT_NAME##Script() = default;                                           \
                                                                                   \
		explicit SCRIPT_NAME##Script(const std::function<void()>& callback) :      \
			callback{ callback } {}                                                \
                                                                                   \
		void On##SCRIPT_NAME() override {                                          \
			if (callback) {                                                        \
				callback();                                                        \
			}                                                                      \
		}                                                                          \
                                                                                   \
		std::function<void()> callback;                                            \
	}

PTGN_DEFINE_BUTTON_SCRIPT(ButtonActivate);
PTGN_DEFINE_BUTTON_SCRIPT(ButtonHoverStart);
PTGN_DEFINE_BUTTON_SCRIPT(ButtonHoverStop);
PTGN_DEFINE_BUTTON_SCRIPT(ButtonHover);

struct AnimatedButtonScript : public Script<AnimatedButtonScript, ButtonScript> {
	AnimatedButtonScript() = default;

	AnimatedButtonScript(
		const Animation& activate_animation, const Animation& hover_animation = {},
		bool force_start_on_activate = true, bool force_start_on_hover_start = true,
		bool stop_on_hover_stop = true
	) :
		activate_animation{ activate_animation },
		hover_animation{ hover_animation },
		force_start_on_activate{ force_start_on_activate },
		force_start_on_hover_start{ force_start_on_hover_start },
		stop_on_hover_stop{ stop_on_hover_stop } {}

	Animation activate_animation;
	Animation hover_animation;

	bool force_start_on_activate{ true };

	bool force_start_on_hover_start{ true };
	bool stop_on_hover_stop{ true };

	void OnButtonHoverStart() override {
		if (hover_animation) {
			hover_animation.Start(force_start_on_hover_start);
		}
	}

	// void OnButtonHover() {}

	void OnButtonHoverStop() override {
		if (hover_animation && stop_on_hover_stop) {
			hover_animation.Stop();
		}
	}

	void OnButtonActivate() override {
		if (activate_animation) {
			activate_animation.Start(force_start_on_activate);
		}
	}
};

struct ButtonToggled : public BoolComponent {
	using BoolComponent::BoolComponent;
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
	ButtonText()								 = default;
	ButtonText& operator=(ButtonText&&) noexcept = default;
	ButtonText(ButtonText&&) noexcept			 = default;
	ButtonText& operator=(const ButtonText&)	 = delete;
	ButtonText(const ButtonText&)				 = delete;
	~ButtonText()								 = default;

	ButtonText(
		Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& text_properties
	);

	[[nodiscard]] TextColor GetTextColor(ButtonState state) const;
	[[nodiscard]] TextContent GetTextContent(ButtonState state) const;
	[[nodiscard]] FontSize GetFontSize(ButtonState state) const;
	[[nodiscard]] TextJustify GetTextJustify(ButtonState state) const;
	[[nodiscard]] Text Get(ButtonState state) const;
	[[nodiscard]] Text GetValid(ButtonState state) const;

	void Set(
		Entity parent, Manager& manager, ButtonState state, const TextContent& text_content,
		const TextColor& text_color, const FontSize& font_size, const ResourceHandle& font_key,
		const TextProperties& text_properties
	);

	GameObject<Text> default_;
	GameObject<Text> hover_;
	GameObject<Text> pressed_;
};

struct ButtonTextToggled : public ButtonText {
	using ButtonText::ButtonText;
};

struct ButtonEnabled {
	bool activate{ true };
	bool hover{ true };

	PTGN_SERIALIZER_REGISTER(ButtonEnabled, activate, hover)
};

} // namespace impl

class Button : public Entity {
public:
	Button() = default;
	Button(const Entity& entity);

	static void Draw(const Entity& entity);

	// Set button callback scripts.
	Button& OnActivate(const std::function<void()>& callback);
	Button& OnHover(const std::function<void()>& callback);
	Button& OnHoverStart(const std::function<void()>& callback);
	Button& OnHoverStop(const std::function<void()>& callback);

	// @param size {} results in texture sized button.
	Button& SetSize(const V2_float& size = {});

	// @return {} if no size is specified via SetSize, SetRadius, or button texture. If radius,
	// returns 2.0f * V2_float{ radius, radius }
	[[nodiscard]] V2_float GetSize() const;

	// @param radius 0.0f results in texture sized button.
	Button& SetRadius(float radius = 0.0f);

	Button& Enable(bool enable_hover = true, bool reset_state = true);
	Button& Disable(bool disable_hover = true, bool reset_state = true);
	Button& SetEnabled(
		bool enable_activation = true, bool enable_hover = true, bool reset_state = true
	);

	// @param check_for_hover_enabled If true, checks for button hovering being enabled instead.
	// @return True if the button activation is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;

	// Manual button script triggers.
	// Called when the mouse is clicked over the button.
	void Activate();
	// Called once when hovering starts (mouse enters button).
	void StartHover();
	// Called continuously when hovering (including when hover starts).
	void ContinueHover();
	// Called once when hovering stops (mouse exits button).
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

	[[nodiscard]] V2_float GetTextFixedSize() const;

	// (default: unscaled text size). If either axis of the text size
	// is zero, it is stretched to fit the entire size of the button rectangle (along that axis).
	Button& SetTextFixedSize(const V2_float& size = {});

	// Make it so the button text no longer has a fixed size,
	// this will cause the text to stretch based its the font size and wrap settings.
	Button& ClearTextFixedSize();

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

PTGN_DRAWABLE_REGISTER(Button);

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

struct ToggleButtonGroupKey : public HashComponent {
	using HashComponent::HashComponent;
};

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::ToggleButtonGroupKey> {
	std::size_t operator()(const ptgn::ToggleButtonGroupKey& group_key) const {
		return group_key.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct ToggleButtonGroupInfo {
	ToggleButtonGroupInfo()											   = default;
	~ToggleButtonGroupInfo()										   = default;
	ToggleButtonGroupInfo(ToggleButtonGroupInfo&&) noexcept			   = default;
	ToggleButtonGroupInfo& operator=(ToggleButtonGroupInfo&&) noexcept = default;
	ToggleButtonGroupInfo(const ToggleButtonGroupInfo&)				   = delete;
	ToggleButtonGroupInfo& operator=(const ToggleButtonGroupInfo&)	   = delete;

	ToggleButtonGroupKey active;
	std::unordered_map<ToggleButtonGroupKey, GameObject<ToggleButton>> buttons;
};

} // namespace impl

class ToggleButtonGroup : public Entity {
public:
	ToggleButton& Load(const ToggleButtonGroupKey& button_key, ToggleButton&& toggle_button);

	void Unload(const ToggleButtonGroupKey& button_key);

	void SetActive(const ToggleButtonGroupKey& button_key);

	// @return Active button, or null entity if no button is active.
	ToggleButton GetActive() const;

private:
	friend class impl::ToggleButtonGroupScript;

	void AddToggleScript(ToggleButton& target);
};

namespace impl {

class ToggleButtonGroupScript : public Script<ToggleButtonGroupScript, ButtonScript> {
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

Button CreateButton(Manager& manager);

Button CreateTextButton(
	Manager& manager, const TextContent& text_content, const TextColor& text_color = color::Black
);

// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(Manager& manager, bool toggled = false);

ToggleButtonGroup CreateToggleButtonGroup(Manager& manager);

Button CreateAnimatedButton(
	Manager& manager, const V2_float& button_size, const Animation& activate_animation,
	const Animation& hover_animation = {}, bool force_start_on_activate = true,
	bool force_start_on_hover_start = true, bool stop_on_hover_stop = true
);

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