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

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "platform/input/mouse.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
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

class DrawContext;
class Scene;

struct ButtonStyle {
	std::optional<std::variant<Rect, Circle>> background;
	std::optional<Color> background_color;
	std::optional<FillStyle> background_fill;

	std::optional<std::variant<Rect, Circle>> border;
	std::optional<Color> border_color;
	std::optional<float> border_width;

	std::optional<GameObject> sprite;
	std::optional<Color> tint;

	std::optional<GameObject> text;

	std::optional<Audio> sound;
};

struct ButtonInteractionConfig {
	ButtonStyle idle;
	ButtonStyle hover;
	ButtonStyle pressed;
};

struct ButtonConfig {
	ButtonInteractionConfig enabled;

	ButtonInteractionConfig disabled;
};

struct ToggleButtonConfig : public ButtonConfig {
	ButtonInteractionConfig toggled;
};

namespace impl {

struct ToggleButtonInteractionConfig {
	ButtonInteractionConfig toggled;
};

} // namespace impl

enum class ButtonState : std::uint8_t {
	Idle,
	Hover,
	Pressed,
	Current
};

inline std::ostream& operator<<(std::ostream& os, ButtonState state) {
	switch (state) {
		using enum ButtonState;
		case Idle:	  return os << "Idle";
		case Hover:	  return os << "Hover";
		case Pressed: return os << "Pressed";
		case Current: return os << "Current";
		default:	  PTGN_ERROR("Unknown button state: ", std::to_underlying(state));
	}
}

PTGN_SERIALIZE_ENUM(
	ButtonState, { { ButtonState::Idle, "idle" },
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
		case IdleDown:	   return os << "Idle Down";
		case IdleUp:	   return os << "Idle Up";
		case Hover:		   return os << "Hover";
		case HoverPressed: return os << "Hover Pressed";
		case Pressed:	   return os << "Pressed";
		case HeldOutside:  return os << "Held Outside";
		default:		   PTGN_ERROR("Unknown internal button state: ", std::to_underlying(state));
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
	void OnButtonActivate() const;
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
	bool activate{ true };
	bool hover{ true };

	PTGN_SERIALIZER_REGISTER(ButtonEnabled, activate, hover)
};

template <typename Derived>
class ButtonBase : public Entity {
public:
	ButtonBase() = default;
	explicit ButtonBase(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

	/// @return In order of precedence: rect size, circle radius, texture size.
	[[nodiscard]] std::variant<Rect, Circle> GetShape() const;
	/// @param check_for_hover_enabled If true, checks for button hovering being enabled instead.
	/// @return True if the button activation is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;
	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;
	[[nodiscard]] Color GetBackgroundColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<Texture> GetTexture(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<Texture> GetDisabledTexture() const;
	[[nodiscard]] Color GetTint(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<Color> GetTextColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<std::string> GetTextContent(
		ButtonState state = ButtonState::Current
	) const;
	[[nodiscard]] std::optional<TextJustify> GetTextJustify(
		ButtonState state = ButtonState::Current
	) const;
	/// @return A pair of (x, y) fixed text size, or {} if the text size is not fixed. If either
	/// axis is
	/// {}, it is stretched to fit the entire size of the button rectangle (along that axis).
	[[nodiscard]] ButtonTextFixedSize GetTextFixedSize() const;
	[[nodiscard]] std::optional<float> GetFontSize(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<Text> GetText(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetBorderColor(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::optional<FillStyle> GetBackgroundFillStyle(
		ButtonState state = ButtonState::Current
	) const;
	[[nodiscard]] std::optional<float> GetBorderWidth(ButtonState state = ButtonState::Current)
		const;

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
	/// @param Sets the shape of the button interactive area. If monostate (default), uses the
	/// texture size.
	Derived& SetShape(std::variant<std::monostate, Rect, Circle> shape = {});
	Derived& SetBackgroundColor(Color color, ButtonState state = ButtonState::Idle);
	Derived& SetTexture(std::optional<Texture> texture, ButtonState state = ButtonState::Idle);
	Derived& SetDisabledTexture(std::optional<Texture> texture);
	Derived& SetTint(Color tint, ButtonState state = ButtonState::Idle);
	Derived& SetTextColor(Color text_color, ButtonState state = ButtonState::Idle);
	Derived& SetTextContent(std::string_view content, ButtonState state = ButtonState::Idle);
	Derived& SetTextJustify(TextJustify justify, ButtonState state = ButtonState::Idle);
	/// If either axis of the text size is {}, it is stretched to fit the entire size of the button
	/// rectangle (along that axis).
	Derived& SetTextFixedSize(ButtonTextFixedSize size = {});
	Derived& SetFontSize(float font_size, ButtonState state = ButtonState::Idle);
	Derived& SetText(
		std::string_view content, Color text_color = color::Black,
		std::optional<float> font_size = {}, std::optional<Font> font = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Idle
	);
	Derived& SetBorderColor(Color color, ButtonState state = ButtonState::Idle);
	Derived& SetBackgroundFillStyle(FillStyle fill_style, ButtonState state = ButtonState::Idle);
	Derived& SetBorderWidth(float line_width, ButtonState state = ButtonState::Idle);

private:
	Derived& Self();
	const Derived& Self() const;

	template <typename T>
	struct ButtonStyles {
		T idle;
		T desired;
	};

	std::pair<const ButtonStyle&, const ButtonStyle&> GetStyle(ButtonState state, bool enabled)
		const;
	std::pair<ButtonStyle&, ButtonStyle&> GetStyle(ButtonState state, bool enabled);
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
	[[nodiscard]] Texture GetTextureToggled(ButtonState state = ButtonState::Idle) const;
	[[nodiscard]] Color GetTintToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetTextColorToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] std::string GetTextContentToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Text GetTextToggled(ButtonState state = ButtonState::Current) const;
	[[nodiscard]] Color GetBorderColorToggled(ButtonState state = ButtonState::Current) const;

	ToggleButton& OnToggle(const std::function<void(bool)>& callback);
	ToggleButton& SetToggled(bool toggled);
	ToggleButton& Toggle();
	ToggleButton& SetBackgroundColorToggled(Color color, ButtonState state = ButtonState::Idle);
	ToggleButton& SetTextureToggled(Texture texture, ButtonState state = ButtonState::Idle);
	ToggleButton& SetTintToggled(Color color, ButtonState state = ButtonState::Idle);
	ToggleButton& SetTextColorToggled(Color text_color, ButtonState state = ButtonState::Idle);
	ToggleButton& SetTextContentToggled(
		std::string_view content, ButtonState state = ButtonState::Idle
	);
	ToggleButton& SetTextToggled(
		std::string_view content, Color text_color = color::Black,
		std::optional<float> font_size = {}, std::optional<Font> font = {},
		const TextProperties& text_properties = {}, ButtonState state = ButtonState::Idle
	);
	ToggleButton& SetBorderColorToggled(Color color, ButtonState state = ButtonState::Idle);
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
	[[nodiscard]] std::optional<ToggleButton> GetActive() const;

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

/// @param shape If monostate (default) uses the texture size of the button. If no texture is
/// provided, text size is used. If no text is provided, calls debug assertion.
Button CreateButton(
	Scene& scene, std::variant<std::monostate, Rect, Circle> shape = {},
	const ButtonConfig& config = {}, bool ui_layer = true
);

/// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(
	Scene& scene, std::variant<std::monostate, Rect, Circle> shape = {},
	const ToggleButtonConfig& config = {}, bool toggled = false
);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

PTGN_REGISTER_DRAWABLE(Button);

} // namespace ptgn