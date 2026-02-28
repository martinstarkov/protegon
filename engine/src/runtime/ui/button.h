#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>

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
#include "runtime/ecs/components/text_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/scripting/script.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Renderer;
class Scene;

enum class ButtonState : std::uint8_t {
	Default,
	Hover,
	Pressed,
	Current
};

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

PTGN_SERIALIZE_ENUM(
	ButtonState, { { ButtonState::Default, "default" },
				   { ButtonState::Hover, "hover" },
				   { ButtonState::Pressed, "pressed" },
				   { ButtonState::Current, "current" } }
);

/// @brief If either axis of the text size is {}, it is stretched to fit the entire size of the
/// button rectangle (along that axis).
struct ButtonTextFixedSize {
	std::optional<float> x;
	std::optional<float> y;
};

namespace impl {

class ToggleButtonGroupScript;

enum class InternalButtonState {
	IdleUp		 = 0,
	Hover		 = 1,
	Pressed		 = 2,
	HeldOutside	 = 3,
	IdleDown	 = 4,
	HoverPressed = 5
};

inline std::ostream& operator<<(std::ostream& os, InternalButtonState state) {
	switch (state) {
		using enum InternalButtonState;
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

PTGN_SERIALIZE_ENUM(
	InternalButtonState, { { InternalButtonState::IdleUp, "idle_up" },
						   { InternalButtonState::Hover, "hover" },
						   { InternalButtonState::Pressed, "pressed" },
						   { InternalButtonState::HeldOutside, "held_outside" },
						   { InternalButtonState::IdleDown, "idle_down" },
						   { InternalButtonState::HoverPressed, "hover_pressed" } }
);

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

struct ButtonActivate : public Event<ButtonActivate> {};

struct ButtonHoverStart : public Event<ButtonHoverStart> {};

struct ButtonHoverStop : public Event<ButtonHoverStop> {};

struct ButtonHover : public Event<ButtonHover> {};

struct ButtonToggleEvent : public Event<ButtonToggleEvent> {
	bool toggled{ false };
};

template <EventType T>
struct ButtonScript : public Script {
	ButtonScript() = default;

	explicit ButtonScript(const std::function<void()>& callback) : callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<T>([this](T&) { callback_(); });
	}

private:
	std::function<void()> callback_;
};

using ButtonActivateScript	 = ButtonScript<ButtonActivate>;
using ButtonHoverStartScript = ButtonScript<ButtonHoverStart>;
using ButtonHoverStopScript	 = ButtonScript<ButtonHoverStop>;
using ButtonHoverScript		 = ButtonScript<ButtonHover>;

struct ButtonToggledScript : public Script {
	ButtonToggledScript() = default;

	explicit ButtonToggledScript(const std::function<void(bool)>& callback) :
		callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<ButtonToggleEvent>([this](ButtonToggleEvent& e) {
			std::invoke(callback_, e.toggled);
		});
	}

private:
	std::function<void(bool)> callback_;
};

class ToggleButtonScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;

private:
	void OnButtonActivate() const;
};

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

struct ToggleButtonGroupKey : public HashComponent {
	using HashComponent::HashComponent;
};

} // namespace impl

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::impl::ToggleButtonGroupKey> {
	std::size_t operator()(const ptgn::impl::ToggleButtonGroupKey& group_key) const {
		return group_key.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct ToggleButtonGroupData {
	ToggleButtonGroupData()											   = default;
	~ToggleButtonGroupData()										   = default;
	ToggleButtonGroupData(ToggleButtonGroupData&&) noexcept			   = default;
	ToggleButtonGroupData& operator=(ToggleButtonGroupData&&) noexcept = default;
	ToggleButtonGroupData(const ToggleButtonGroupData&)				   = delete;
	ToggleButtonGroupData& operator=(const ToggleButtonGroupData&)	   = delete;

	ToggleButtonGroupKey active;
	std::unordered_map<ToggleButtonGroupKey, GameObject> buttons;
};

struct ButtonToggled {};

struct ButtonDisabledTexture : public Texture {
	using Texture::Texture;

	ButtonDisabledTexture(const Texture& t);

	ButtonDisabledTexture(Texture&& t);
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

	explicit ButtonTexture(Texture texture) :
		default_{ texture }, hover_{ texture }, pressed_{ texture } {}

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
	[[nodiscard]] Text Get(ButtonState state) const;
	[[nodiscard]] Text GetValid(ButtonState state) const;

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

template <typename Derived>
class ButtonBase : public Entity {
public:
	ButtonBase() = default;
	explicit ButtonBase(Entity entity);

	static void Draw(Renderer& renderer, Entity entity);

	/// @return If no size is specified, returns {}.
	/// Otherwise returns, in order of precedence: texture size, rect size, or {2*radius, 2*radius}.
	[[nodiscard]] std::optional<V2_float> GetSize() const;
	/// @param check_for_hover_enabled If true, checks for button hovering being enabled instead.
	/// @return True if the button activation is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;
	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;
	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Texture GetTexture(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Texture GetDisabledTexture() const;
	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetTextColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::string GetTextContent(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] TextJustify GetTextJustify(ButtonState state = ButtonState::Current) const;
	/// @return A pair of (x, y) fixed text size, or {} if the text size is not fixed. If either
	/// axis is
	/// {}, it is stretched to fit the entire size of the button rectangle (along that axis).
	[[nodiscard]] ButtonTextFixedSize GetTextFixedSize() const;
	[[nodiscard]] float GetFontSize(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Entity GetText(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] float GetBackgroundLineWidth() const;
	[[nodiscard]] float GetBorderWidth() const;

	/// @brief Set button callback scripts.
	Derived& OnActivate(const std::function<void()>& callback);
	Derived& OnHover(const std::function<void()>& callback);
	Derived& OnHoverStart(const std::function<void()>& callback);
	Derived& OnHoverStop(const std::function<void()>& callback);

	Derived& Enable(bool enable_hover = true, bool reset_state = true);
	Derived& Disable(bool disable_hover = true, bool reset_state = true);
	Derived& SetEnabled(
		bool enable_activation = true, bool enable_hover = true, bool reset_state = true
	);

	/// Manual button script triggers.
	/// Called when the mouse is clicked over the button.
	Derived& Activate();
	/// Called once when hovering starts (mouse enters button).
	Derived& StartHover();
	/// Called continuously when hovering (including when hover starts).
	Derived& ContinueHover();
	/// Called once when hovering stops (mouse exits button).
	Derived& StopHover();
	/// @param Sets the button to have a rectangle interactive shape.
	Derived& SetSize(V2_float size = {});
	/// @param Sets the button to have a circle interactive shape.
	Derived& SetRadius(float radius = {});
	Derived& SetBackgroundColor(Color color, ButtonState state = ButtonState::Default);
	Derived& SetTexture(Texture texture, ButtonState state = ButtonState::Default);
	Derived& SetDisabledTexture(Texture texture);
	Derived& SetTint(Color tint, ButtonState state = ButtonState::Default);
	Derived& SetTextColor(Color text_color, ButtonState state = ButtonState::Default);
	Derived& SetTextContent(std::string_view content, ButtonState state = ButtonState::Default);
	Derived& SetTextJustify(TextJustify justify, ButtonState state = ButtonState::Default);
	/// If either axis of the text size is {}, it is stretched to fit the entire size of the button
	/// rectangle (along that axis).
	Derived& SetTextFixedSize(ButtonTextFixedSize size = {});
	Derived& SetFontSize(float font_size, ButtonState state = ButtonState::Default);
	Derived& SetText(
		std::string_view content, Color text_color = color::Black,
		std::optional<float> font_size = {}, std::optional<Font> font = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
	);
	Derived& SetBorderColor(Color color, ButtonState state = ButtonState::Default);
	/// If -1 (default), button background is a solid rectangle, otherwise uses the specified line
	/// width.
	Derived& SetBackgroundLineWidth(float line_width);
	Derived& SetBorderWidth(float line_width);

private:
	Derived& Self();
	const Derived& Self() const;
};

} // namespace impl

class Button : public impl::ButtonBase<Button> {
public:
	Button() = default;
	using impl::ButtonBase<Button>::ButtonBase;
};

class ToggleButton : public impl::ButtonBase<ToggleButton> {
public:
	ToggleButton() = default;
	using impl::ButtonBase<ToggleButton>::ButtonBase;
	operator Button() const;

	[[nodiscard]] bool IsToggled() const;
	[[nodiscard]] Color GetBackgroundColorToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Texture GetTextureToggled(ButtonState state = ButtonState::Default) const;
	[[nodiscard]] Color GetTintToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::string GetTextContentToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text GetTextToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	// TODO: Fix.
	// ToggleButton& OnToggle(const std::function<void(bool)>& callback);
	ToggleButton& SetToggled(bool toggled);
	ToggleButton& Toggle();
	ToggleButton& SetBackgroundColorToggled(Color color, ButtonState state = ButtonState::Default);
	ToggleButton& SetTextureToggled(Texture texture, ButtonState state = ButtonState::Default);
	ToggleButton& SetTintToggled(Color color, ButtonState state = ButtonState::Default);
	ToggleButton& SetTextColorToggled(Color text_color, ButtonState state = ButtonState::Default);
	ToggleButton& SetTextContentToggled(
		std::string_view content, ButtonState state = ButtonState::Default
	);
	ToggleButton& SetTextToggled(
		std::string_view content, Color text_color = color::Black,
		std::optional<float> font_size = {}, std::optional<Font> font = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Default
	);
	ToggleButton& SetBorderColorToggled(Color color, ButtonState state = ButtonState::Default);
};

class ToggleButtonGroup : public Entity {
public:
	ToggleButtonGroup() = default;
	explicit ToggleButtonGroup(Entity entity);

	ToggleButton Add(std::string_view button_key, ToggleButton toggle_button);

	void Remove(std::string_view button_key);

	void SetActive(std::string_view button_key);

	/// @return Active button, or null entity if no button is active.
	[[nodiscard]] ToggleButton GetActive() const;

	void AddToggleScript(ToggleButton toggle_button) const;

private:
	friend class impl::ToggleButtonGroupScript;

	void SetActiveKey(impl::ToggleButtonGroupKey key);
};

namespace impl {

class ToggleButtonGroupScript : public Script {
public:
	ToggleButtonGroupScript() = default;
	explicit ToggleButtonGroupScript(const ToggleButtonGroup& group);

	void OnEvent(EventDispatcher d) override;

private:
	void OnButtonActivate();

	ToggleButtonGroup toggle_button_group_;
};

} // namespace impl

Button CreateButton(Scene& scene);

Button CreateTextButton(
	Scene& scene, std::string_view text_content, Color text_color = color::Black
);

/// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(Scene& scene, bool toggled = false);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

// TODO: Fix.
// Entity CreateAnimatedButton(
//	Scene& scene, V2_float button_size, const Animation& activate_animation,
//	const Animation& hover_animation = {}, bool force_start_on_activate = true,
//	bool force_start_on_hover_start = true, bool stop_on_hover_stop = true
//);

PTGN_REGISTER_DRAWABLE(Button);

} // namespace ptgn