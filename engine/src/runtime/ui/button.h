#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/mouse.h"
#include "core/math/easing.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/time.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class DrawContext;
class Scene;

/// @brief If either axis of the text size is {}, it is stretched to fit the entire size of the
/// button rectangle (along that axis).
struct ButtonTextFixedSize {
	std::optional<float> x;
	std::optional<float> y;

	PTGN_SERIALIZE(ButtonTextFixedSize, x, y)
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

	std::optional<GameObject<Sprite>> sprite;

	/// @brief Applies to the sprite only.
	std::optional<Color> sprite_tint;

	/// @brief Applies to all aspects of the button.
	std::optional<Color> tint;

	std::optional<GameObject<Text>> text;
	std::optional<ButtonTextFixedSize> text_fixed_size;

	std::optional<Audio> sound;
};

struct ButtonInteractionStyle {
	ButtonInteractionStyle()											 = default;
	~ButtonInteractionStyle() noexcept									 = default;
	ButtonInteractionStyle(ButtonInteractionStyle&&) noexcept			 = default;
	ButtonInteractionStyle& operator=(ButtonInteractionStyle&&) noexcept = default;
	ButtonInteractionStyle(const ButtonInteractionStyle&)				 = delete;
	ButtonInteractionStyle& operator=(const ButtonInteractionStyle&)	 = delete;

	ButtonStyle idle;
	ButtonStyle hover;
	ButtonStyle press;
};

struct ButtonStyles {
	ButtonStyles()									 = default;
	~ButtonStyles() noexcept						 = default;
	ButtonStyles(ButtonStyles&&) noexcept			 = default;
	ButtonStyles& operator=(ButtonStyles&&) noexcept = default;
	ButtonStyles(const ButtonStyles&)				 = delete;
	ButtonStyles& operator=(const ButtonStyles&)	 = delete;

	ButtonInteractionStyle enabled;

	ButtonInteractionStyle disabled;
};

struct ToggleButtonStyles {
	ToggleButtonStyles()										 = default;
	~ToggleButtonStyles() noexcept								 = default;
	ToggleButtonStyles(ToggleButtonStyles&&) noexcept			 = default;
	ToggleButtonStyles& operator=(ToggleButtonStyles&&) noexcept = default;
	ToggleButtonStyles(const ToggleButtonStyles&)				 = delete;
	ToggleButtonStyles& operator=(const ToggleButtonStyles&)	 = delete;

	ButtonInteractionStyle enabled;

	ButtonInteractionStyle disabled;

	ButtonInteractionStyle toggled;
};

struct MoveButtonConfig {
	V2_float offset{ 20, 0 };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };

	PTGN_SERIALIZE(MoveButtonConfig, offset, duration, ease)
};

struct ScaleButtonConfig {
	float scale{ 1.25f };
	milliseconds duration{ 100 };
	Ease ease{ Ease::Linear };

	PTGN_SERIALIZE(ScaleButtonConfig, scale, duration, ease)
};

struct ButtonConfig {
	std::optional<std::string> content;
	std::optional<Color> text_color{ color::White };
	std::optional<Color> text_color_hover;
	std::optional<Color> text_color_press;
	std::optional<int> text_outline_width;
	std::optional<Color> text_outline_color;

	FontSize font_size;
	FontOrKey font;

	std::optional<TextureOrKey> texture;
	std::optional<TextureOrKey> texture_hover;
	std::optional<TextureOrKey> texture_press;

	std::optional<Color> texture_tint;
	std::optional<Color> texture_tint_hover;
	std::optional<Color> texture_tint_press;

	std::optional<Color> background_color;
	std::optional<Color> background_color_hover;
	std::optional<Color> background_color_press;

	std::optional<V2_float> background_size;

	std::optional<AudioOrKey> sound_hover;
	std::optional<AudioOrKey> sound_press;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;
};

struct AnimatedButtonConfig {
	TextureOrKey texture;
	TextureOrKey texture_hover;
	std::optional<TextureOrKey> texture_press;

	AnimationConfig animation_hover;
	std::optional<AnimationConfig> animation_press;

	std::optional<AudioOrKey> sound_hover;
	std::optional<AudioOrKey> sound_press;
};

namespace impl {

constexpr Color kDefaultButtonTextColor{ color::Black };

struct ButtonAnimationCompleteScript : public Script {
	ButtonAnimationCompleteScript() = default;

	explicit ButtonAnimationCompleteScript(Entity button);

	Entity button;

	void OnEvent(Event event) override;
};

struct ToggleButtonInteractionStyle {
	ButtonInteractionStyle toggled;
};

struct ButtonExclusiveAudio {};

} // namespace impl

enum class ButtonState : std::uint8_t {
	Idle,
	Hover,
	Press,
	Current
};
PTGN_SERIALIZE_ENUM(ButtonState);

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

namespace event {

struct ToggleButtonToggle;

} // namespace event

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
PTGN_SERIALIZE_ENUM(InternalButtonState);

class ButtonScript : public Script {
public:
	void OnEvent(Event event) override;

private:
	void OnMouseMoveOver();

	void OnMouseMoveOut();

	void OnMousePressedOver(Mouse mouse);

	void OnMousePressedOut(Mouse mouse);

	void OnMouseReleasedOver(Mouse mouse);

	void OnMouseReleasedOut(Mouse mouse);
};

class ToggleButtonScript : public Script {
public:
	void OnEvent(Event event) override;

private:
	void OnButtonPress() const;
};

struct ToggleButtonGroupKey : public HashComponent {
	using HashComponent::HashComponent;
};

struct ToggleButtonGroupData {
	ToggleButtonGroupData()											   = default;
	~ToggleButtonGroupData() noexcept								   = default;
	ToggleButtonGroupData(ToggleButtonGroupData&&) noexcept			   = default;
	ToggleButtonGroupData& operator=(ToggleButtonGroupData&&) noexcept = default;
	ToggleButtonGroupData(const ToggleButtonGroupData&)				   = delete;
	ToggleButtonGroupData& operator=(const ToggleButtonGroupData&)	   = delete;

	bool always_active{ true };
	std::optional<ToggleButtonGroupKey> active;
	std::vector<std::pair<ToggleButtonGroupKey, GameObject<>>> buttons;
};

struct ButtonToggledState {};

struct ButtonEnabled {
	bool press{ true };
	bool hover{ true };

	PTGN_SERIALIZE(ButtonEnabled, press, hover)
};

namespace event {

template <typename T>
struct ButtonBasePress {
	T button;
};

template <typename T>
struct ButtonBaseHoverStart {
	T button;
};

template <typename T>
struct ButtonBaseHover {
	T button;
};

template <typename T>
struct ButtonBaseHoverStop {
	T button;
};

} // namespace event

template <typename Derived>
class ButtonBase : public Entity {
public:
	ButtonBase() = default;
	explicit ButtonBase(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity);

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

	// TODO: Fix.
	// std::optional<TextJustify> GetTextJustify(ButtonStyleState state = {}) const;

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

	/// @return Null entity if button has no sprite (or fallback option) for the given state.
	Entity GetSprite(ButtonStyleState state = {}) const;

	template <typename F>
	Derived& OnPress(F&& callback) {
		AddScript<impl::EventScript<event::ButtonBasePress<Derived>>>(
			*this,
			impl::MakeEventCallback<event::ButtonBasePress<Derived>>(std::forward<F>(callback))
		);
		return Self();
	}

	template <typename F>
	Derived& OnHover(F&& callback) {
		AddScript<impl::EventScript<event::ButtonBaseHover<Derived>>>(
			*this,
			impl::MakeEventCallback<event::ButtonBaseHover<Derived>>(std::forward<F>(callback))
		);
		return Self();
	}

	template <typename F>
	Derived& OnHoverStart(F&& callback) {
		AddScript<impl::EventScript<event::ButtonBaseHoverStart<Derived>>>(
			*this,
			impl::MakeEventCallback<event::ButtonBaseHoverStart<Derived>>(std::forward<F>(callback))
		);
		return Self();
	}

	template <typename F>
	Derived& OnHoverStop(F&& callback) {
		AddScript<impl::EventScript<event::ButtonBaseHoverStop<Derived>>>(
			*this,
			impl::MakeEventCallback<event::ButtonBaseHoverStop<Derived>>(std::forward<F>(callback))
		);
		return Self();
	}

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
	// TODO: Fix.
	// Derived& SetTextColor(Color text_color, ButtonStyleState state = ButtonStyleState::Idle());
	// TODO: Fix.
	// Derived& SetTextContent(
	//	std::string_view text_content, ButtonStyleState state = ButtonStyleState::Idle()
	//);
	// TODO: Fix.
	// Derived& SetTextJustify(TextJustify justify, ButtonStyleState state =
	// ButtonStyleState::Idle());
	/// If either axis of the text size is {}, it is stretched to fit the entire size of the button
	/// rectangle (along that axis).
	Derived& SetTextFixedSize(
		std::optional<ButtonTextFixedSize> size = {},
		ButtonStyleState state					= ButtonStyleState::Idle()
	);
	// TODO: Fix.
	// Derived& SetFontSize(FontSize font_size, ButtonStyleState state = ButtonStyleState::Idle());
	// TODO: Fix.
	// Derived& SetText(
	//	std::string_view text_content, Color text_color = color::Black, FontSize font_size = {},
	//	FontOrKey font = {}, const TextProperties& text_properties = {},
	//	ButtonStyleState state = ButtonStyleState::Idle()
	//);
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
	friend class impl::ButtonScript;
	friend struct impl::ButtonAnimationCompleteScript;

	Derived& Self();
	const Derived& Self() const;

	struct ButtonStyleTuple {
		ButtonStyle& enabled_idle;
		ButtonStyle& idle;
		ButtonStyle& desired;
	};

	struct ConstButtonStyleTuple {
		const ButtonStyle& enabled_idle;
		const ButtonStyle& idle;
		const ButtonStyle& desired;
	};

	ConstButtonStyleTuple GetStyle(ButtonStyleState state) const;

	ButtonStyleTuple GetStyle(ButtonStyleState state);

	// TODO: Fix.
	// void SetText(
	//	GameObject<Text>& text, std::string_view text_content = {},
	//	std::optional<Color> text_color = {}, FontSize font_size = {}, FontOrKey font = {},
	//	const TextProperties& text_properties = {}
	//);

	void SetState(InternalButtonState new_state);

	void PlaySound(ButtonState active);
	void PlayAnimation(ButtonState active);
};

} // namespace impl

class Button : public impl::ButtonBase<Button> {
public:
	Button() = default;
	using impl::ButtonBase<Button>::ButtonBase;
};

class ToggleButton;

class ToggleButton : public impl::ButtonBase<ToggleButton> {
public:
	ToggleButton() = default;
	using impl::ButtonBase<ToggleButton>::ButtonBase;
	operator Button() const; // NOSONAR

	[[nodiscard]] bool IsToggled() const;

	template <typename F>
	ToggleButton& OnToggle(F&& callback) {
		AddScript<impl::EventScript<event::ToggleButtonToggle>>(
			*this, impl::MakeEventCallback<event::ToggleButtonToggle>(std::forward<F>(callback))
		);
		return *this;
	}

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

	void OnEvent(Event event) override;

private:
	void OnButtonPress();

	ToggleButtonGroup toggle_button_group_;
};

} // namespace impl

/// @param shape If nullopt, uses the texture size of the button. If no texture is
/// provided, text size is used. If no text is provided, calls debug assertion.
Button CreateButton(
	Scene& scene, V2_float position = {},
	const std::optional<std::variant<Rect, Circle>>& shape = {},
	Origin draw_origin = Origin::Center, ButtonStyles styles = ButtonStyles{}, bool ui_layer = true
);

Button CreateButton(
	Scene& scene, V2_float position, V2_float size, const ButtonConfig& config,
	Origin draw_origin = Origin::Center
);

Button CreateAnimatedButton(
	Scene& scene, V2_float position, std::optional<V2_float> size,
	const AnimatedButtonConfig& config, Origin draw_origin = Origin::Center
);

/// @param toggled Whether or not the button start in the toggled state.
ToggleButton CreateToggleButton(
	Scene& scene, V2_float position = {},
	const std::optional<std::variant<Rect, Circle>>& shape = {},
	Origin draw_origin = Origin::Center, ToggleButtonStyles styles = ToggleButtonStyles{},
	bool toggled = false
);

ToggleButtonGroup CreateToggleButtonGroup(Scene& scene);

PTGN_REGISTER_DRAWABLE(Button);

} // namespace ptgn