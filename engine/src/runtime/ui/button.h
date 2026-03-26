#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "runtime/scripting/script.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class Button;
class DrawContext;
class Scene;

/// @brief If either axis of the text size is {}, it is stretched to fit the entire size of the
/// button rectangle (along that axis).
struct ButtonTextFixedSize {
	std::optional<float> x;
	std::optional<float> y;
};

struct ButtonStyle {
	ButtonStyle()								   = default;
	~ButtonStyle() noexcept						   = default;
	ButtonStyle(ButtonStyle&&) noexcept			   = default;
	ButtonStyle& operator=(ButtonStyle&&) noexcept = default;
	ButtonStyle(const ButtonStyle&)				   = delete;
	ButtonStyle& operator=(const ButtonStyle&)	   = delete;

	std::optional<std::variant<Rect, Circle>> background_shape;
	std::optional<Color> background_color;
	std::optional<FillStyle> background_fill;

	std::optional<std::variant<Rect, Circle>> border_shape;
	std::optional<Color> border_color;
	std::optional<float> border_width;

	std::optional<GameObject> sprite;

	/// @brief Applies to the sprite only.
	std::optional<Color> sprite_tint;

	/// @brief Applies to all aspects of the button.
	std::optional<Color> tint;

	std::optional<GameObject> text;
	std::optional<ButtonTextFixedSize> text_fixed_size;

	std::optional<Audio> sound;
};

struct ButtonInteractionConfig {
	ButtonInteractionConfig()											   = default;
	~ButtonInteractionConfig() noexcept									   = default;
	ButtonInteractionConfig(ButtonInteractionConfig&&) noexcept			   = default;
	ButtonInteractionConfig& operator=(ButtonInteractionConfig&&) noexcept = default;
	ButtonInteractionConfig(const ButtonInteractionConfig&)				   = delete;
	ButtonInteractionConfig& operator=(const ButtonInteractionConfig&)	   = delete;

	ButtonStyle idle;
	ButtonStyle hover;
	ButtonStyle press;
};

struct ButtonConfig {
	ButtonConfig()									 = default;
	~ButtonConfig() noexcept						 = default;
	ButtonConfig(ButtonConfig&&) noexcept			 = default;
	ButtonConfig& operator=(ButtonConfig&&) noexcept = default;
	ButtonConfig(const ButtonConfig&)				 = delete;
	ButtonConfig& operator=(const ButtonConfig&)	 = delete;

	ButtonInteractionConfig enabled;

	ButtonInteractionConfig disabled;
};

struct ToggleButtonConfig {
	ToggleButtonConfig()										 = default;
	~ToggleButtonConfig() noexcept								 = default;
	ToggleButtonConfig(ToggleButtonConfig&&) noexcept			 = default;
	ToggleButtonConfig& operator=(ToggleButtonConfig&&) noexcept = default;
	ToggleButtonConfig(const ToggleButtonConfig&)				 = delete;
	ToggleButtonConfig& operator=(const ToggleButtonConfig&)	 = delete;

	ButtonInteractionConfig enabled;

	ButtonInteractionConfig disabled;

	ButtonInteractionConfig toggled;
};

namespace impl {

constexpr Color kDefaultButtonTextColor{ color::Black };

struct ButtonAnimationCompleteScript : public Script {
	ButtonAnimationCompleteScript() = default;

	explicit ButtonAnimationCompleteScript(Entity button);

	Entity button;

	void OnEvent(EventDispatcher d) override;
};

struct ToggleButtonInteractionConfig {
	ButtonInteractionConfig toggled;
};

struct ButtonExclusiveAudio {};

} // namespace impl

enum class ButtonState : std::uint8_t {
	Idle,
	Hover,
	Press,
	Current
};

struct ButtonStyleState {
	ButtonStyleState() = default;

	ButtonStyleState(ButtonState state, bool disabled = false, bool toggled = false) : // NOSONAR
		state{ state }, disabled{ disabled }, toggled{ toggled } {}

	static ButtonStyleState Idle() {
		return ButtonStyleState{ ButtonState::Idle };
	}

	ButtonState state{ ButtonState::Current };
	bool disabled{ false };
	bool toggled{ false };
};

inline std::ostream& operator<<(std::ostream& os, ButtonState state) {
	switch (state) {
		using enum ButtonState;
		case Idle:	  return os << "Idle";
		case Hover:	  return os << "Hover";
		case Press:	  return os << "Press";
		case Current: return os << "Current";
		default:	  PTGN_ERROR("Unknown ButtonState: ", std::to_underlying(state));
	}
}

PTGN_SERIALIZE_ENUM(
	ButtonState, { { ButtonState::Idle, "idle" },
				   { ButtonState::Hover, "hover" },
				   { ButtonState::Press, "press" },
				   { ButtonState::Current, "current" } }
);

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
		case IdleDown:	   return os << "IdleDown";
		case IdleUp:	   return os << "IdleUp";
		case Hover:		   return os << "Hover";
		case HoverPressed: return os << "HoverPressed";
		case Pressed:	   return os << "Pressed";
		case HeldOutside:  return os << "HeldOutside";
		default:		   PTGN_ERROR("Unknown InternalButtonState: ", std::to_underlying(state));
	}
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

struct ButtonPress : public Event<ButtonPress> {};

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

using ButtonPressScript		 = ButtonScript<ButtonPress>;
using ButtonHoverStartScript = ButtonScript<ButtonHoverStart>;
using ButtonHoverStopScript	 = ButtonScript<ButtonHoverStop>;
using ButtonHoverScript		 = ButtonScript<ButtonHover>;

struct ButtonToggleScript : public Script {
	ButtonToggleScript() = default;

	explicit ButtonToggleScript(const std::function<void(bool)>& callback) :
		callback_{ callback } {}

	void OnEvent(EventDispatcher d) override {
		d.Dispatch<ButtonToggleEvent>([this](ButtonToggleEvent& e) {
			std::invoke(callback_, e.toggled);
		});
	}

private:
	std::function<void(bool)> callback_;
};

class InternalToggleButtonScript : public Script {
public:
	void OnEvent(EventDispatcher d) override;

private:
	void OnButtonPress() const;
};

struct ToggleButtonGroupKey : public HashComponent {
	using HashComponent::HashComponent;
};

} // namespace impl

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::impl::ToggleButtonGroupKey> {
	std::size_t operator()(const ptgn::impl::ToggleButtonGroupKey& key) const {
		return key.GetHash();
	}
};

} // namespace std

namespace ptgn {

namespace impl {

struct ToggleButtonGroupData {
	ToggleButtonGroupData()											   = default;
	~ToggleButtonGroupData() noexcept								   = default;
	ToggleButtonGroupData(ToggleButtonGroupData&&) noexcept			   = default;
	ToggleButtonGroupData& operator=(ToggleButtonGroupData&&) noexcept = default;
	ToggleButtonGroupData(const ToggleButtonGroupData&)				   = delete;
	ToggleButtonGroupData& operator=(const ToggleButtonGroupData&)	   = delete;

	bool always_active{ true };
	std::optional<ToggleButtonGroupKey> active;
	std::vector<std::pair<ToggleButtonGroupKey, GameObject>> buttons;
};

struct ButtonToggled {};

struct ButtonEnabled {
	bool press{ true };
	bool hover{ true };

	PTGN_SERIALIZER_REGISTER(ButtonEnabled, press, hover)
};

template <typename Derived>
class ButtonBase : public Entity {
public:
	ButtonBase() = default;
	explicit ButtonBase(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	/// @return In order of precedence: rect size, circle radius, texture size.
	std::optional<std::variant<Rect, Circle>> GetShape() const;

	/// @param check_for_hover_enabled If true, checks for button hovering being enabled instead.
	/// @return True if the button activation is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;

	ButtonState GetState() const;

	ButtonStyleState GetStyleState() const;

	impl::InternalButtonState GetInternalState() const;

	std::optional<std::variant<Rect, Circle>> GetBackgroundShape(ButtonStyleState state = {}) const;

	std::optional<Color> GetBackgroundColor(ButtonStyleState state = {}) const;

	std::optional<Texture> GetTexture(ButtonStyleState state = {}) const;

	std::optional<Color> GetTextureTint(ButtonStyleState state = {}) const;

	std::optional<Color> GetTint(ButtonStyleState state = {}) const;

	std::optional<Color> GetTextColor(ButtonStyleState state = {}) const;

	std::optional<std::string> GetTextContent(ButtonStyleState state = {}) const;

	std::optional<TextJustify> GetTextJustify(ButtonStyleState state = {}) const;

	/// @return A pair of (x, y) fixed text size, or {} if the text size is not fixed. If either
	/// axis is
	/// {}, it is stretched to fit the entire size of the button rectangle (along that axis).
	std::optional<ButtonTextFixedSize> GetTextFixedSize(ButtonStyleState state = {}) const;

	std::optional<FontSize> GetFontSize(ButtonStyleState state = {}) const;

	std::optional<Text> GetText(ButtonStyleState state = {}) const;

	std::optional<Audio> GetSound(ButtonStyleState state = {}) const;

	std::optional<Animation> GetAnimation(ButtonStyleState state = {}) const;

	std::optional<std::variant<Rect, Circle>> GetBorderShape(ButtonStyleState state = {}) const;

	std::optional<Color> GetBorderColor(ButtonStyleState state = {}) const;

	std::optional<FillStyle> GetBackgroundFillStyle(ButtonStyleState state = {}) const;

	std::optional<float> GetBorderWidth(ButtonStyleState state = {}) const;

	std::optional<Entity> GetSprite(ButtonStyleState state = {}) const;

	/// @brief Set button callback scripts.
	Derived& OnPress(const std::function<void()>& callback);
	Derived& OnHover(const std::function<void()>& callback);
	Derived& OnHoverStart(const std::function<void()>& callback);
	Derived& OnHoverStop(const std::function<void()>& callback);

	Derived& Enable(bool enable_hover = true, bool reset_state = true);
	Derived& Disable(bool disable_hover = true, bool reset_state = true);
	Derived& SetEnabled(
		bool enable_activation = true, bool enable_hover = true, bool reset_state = true
	);

	/// @brief Manual button script triggers.
	/// Called when the mouse is pressed over the button.
	Derived& Press();
	/// @brief Called once when hovering starts (mouse enters button).
	Derived& StartHover();
	/// @brief Called continuously when hovering (including when hover starts).
	Derived& ContinueHover();
	/// @brief Called once when hovering stops (mouse exits button).
	Derived& StopHover();

	/// @param Sets the shape of the button interactive area. If nullopt, uses the
	/// texture size.
	Derived& SetShape(const std::optional<std::variant<Rect, Circle>>& shape = {});

	/// @brief Makes it so the button has no shape. This is primarily for custom buttons which rely
	/// on parent shapes.
	Derived& RemoveShape();

	/// @param sound If nullopt, removes the button sound.
	Derived& SetSound(
		std::optional<AudioOrKey> sound, ButtonStyleState state = ButtonStyleState::Idle()
	);

	Derived& SetAnimation(Animation&& animation, ButtonStyleState state = ButtonStyleState::Idle());

	Derived& RemoveAnimation(ButtonStyleState state = ButtonStyleState::Idle());

	Derived& SetBackgroundShape(
		std::optional<std::variant<Rect, Circle>> shape,
		ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetBackgroundColor(
		std::optional<Color> color, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetTexture(
		std::optional<TextureOrKey> texture, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetTextureTint(
		std::optional<Color> texture_tint, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetTint(std::optional<Color> tint, ButtonStyleState state = ButtonStyleState::Idle());
	Derived& SetTextColor(Color text_color, ButtonStyleState state = ButtonStyleState::Idle());
	Derived& SetTextContent(
		std::string_view text_content, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetTextJustify(TextJustify justify, ButtonStyleState state = ButtonStyleState::Idle());
	/// If either axis of the text size is {}, it is stretched to fit the entire size of the button
	/// rectangle (along that axis).
	Derived& SetTextFixedSize(
		std::optional<ButtonTextFixedSize> size = {},
		ButtonStyleState state					= ButtonStyleState::Idle()
	);
	Derived& SetFontSize(FontSize font_size, ButtonStyleState state = ButtonStyleState::Idle());
	Derived& SetText(
		std::string_view text_content, Color text_color = color::Black, FontSize font_size = {},
		FontOrKey font = {}, const TextProperties& text_properties = {},
		ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetBorderShape(
		std::optional<std::variant<Rect, Circle>> shape,
		ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetBorderColor(
		std::optional<Color> color, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetBackgroundFillStyle(
		FillStyle fill_style, ButtonStyleState state = ButtonStyleState::Idle()
	);
	Derived& SetBorderWidth(float line_width, ButtonStyleState state = ButtonStyleState::Idle());

	Derived& SetExclusiveAudio(bool enabled);

private:
	friend class impl::InternalButtonScript;
	friend struct impl::ButtonAnimationCompleteScript;

	Derived& Self();
	const Derived& Self() const;

	struct ButtonStyles {
		ButtonStyle& enabled_idle;
		ButtonStyle& idle;
		ButtonStyle& desired;
	};

	struct ConstButtonStyles {
		const ButtonStyle& enabled_idle;
		const ButtonStyle& idle;
		const ButtonStyle& desired;
	};

	ConstButtonStyles GetStyle(ButtonStyleState state) const;

	ButtonStyles GetStyle(ButtonStyleState state);

	void SetText(
		GameObject& text, std::string_view text_content = {}, std::optional<Color> text_color = {},
		FontSize font_size = {}, FontOrKey font = {}, const TextProperties& text_properties = {}
	);

	void SetState(InternalButtonState new_state);

	void OnStateChange(InternalButtonState from, InternalButtonState to);

	void PlaySound(ButtonState active);
	void PlayAnimation(ButtonState active);
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
	operator Button() const; // NOSONAR

	[[nodiscard]] bool IsToggled() const;

	ToggleButton& OnToggle(const std::function<void(bool)>& callback);
	ToggleButton& SetToggled(bool toggled);
	ToggleButton& Toggle();
};

class ToggleButtonGroup : public Entity {
public:
	ToggleButtonGroup() = default;
	explicit ToggleButtonGroup(Entity entity);

	/// @brief If true (default), the button group will always have an active button (i.e. pressing
	/// an active button does not toggle it).
	/// @param button_key If always_active is true, the button with this key will be the one that is
	/// set to active. If nullopt, uses the first loaded button in the toggle
	/// button group. If no buttons are loaded, button_key does nothing.
	void SetAlwaysOneActive(bool always_active, std::optional<std::string_view> button_key = {});

	ToggleButton Add(std::string_view button_key, ToggleButton&& toggle_button);

	void Remove(std::string_view button_key);

	void SetActive(std::string_view button_key);

	/// @return Active button, or nullopt if no button is active.
	std::optional<ToggleButton> GetActive() const;

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
	void OnButtonPress();

	ToggleButtonGroup toggle_button_group_;
};

} // namespace impl

/// @param shape If nullopt, uses the texture size of the button. If no texture is
/// provided, text size is used. If no text is provided, calls debug assertion.
Button CreateButton(
	Scene& scene, const std::optional<std::variant<Rect, Circle>>& shape = {},
	ButtonConfig config = ButtonConfig{}, bool ui_layer = true
);

/// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(
	Scene& scene, const std::optional<std::variant<Rect, Circle>>& shape = {},
	ToggleButtonConfig config = ToggleButtonConfig{}, bool toggled = false
);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

PTGN_REGISTER_DRAWABLE(Button);

} // namespace ptgn