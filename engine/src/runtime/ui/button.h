#pragma once

#include <array>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
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
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class ToggleButton;
class DrawContext;
class Scene;
class Dropdown;

inline constexpr Color kDefaultButtonTextColor{ color::Black };

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);

namespace event {

struct ButtonPress;
struct ButtonHoverStart;
struct ButtonHover;
struct ButtonHoverStop;

} // namespace event

enum class ButtonAnimationPlayback : std::uint8_t {
	StaticFrame,
	Play,
	PlayOnce,
};
PTGN_SERIALIZE_ENUM(ButtonAnimationPlayback);

struct ButtonAnimationOptions {
	ButtonAnimationPlayback playback{ ButtonAnimationPlayback::Play };

	/// @brief Used only for StaticFrame.
	std::size_t static_frame{ 0 };

	/// @brief If true, the button keeps showing this visual state until the
	/// animation completes.
	bool lock_visual_state{ false };

	/// @brief If true, repeated presses are ignored while this visual state is
	/// locked.
	bool block_press{ false };

	constexpr bool operator==(const ButtonAnimationOptions&) const = default;

	PTGN_SERIALIZE(ButtonAnimationOptions, playback, static_frame, lock_visual_state, block_press)
};

namespace impl {

inline constexpr std::size_t kButtonVisualStateCount{ magic_enum::enum_count<ButtonVisualState>() };

enum class InternalButtonState : std::uint8_t {
	IdleUp,
	Hover,
	Pressed,
	HeldOutside,
	IdleDown,
	HoverPressed,
};
PTGN_SERIALIZE_ENUM(InternalButtonState);

enum class ButtonDirty : std::uint8_t {
	None	   = 0,
	Background = 1 << 0,
	Border	   = 1 << 1,
	Sprite	   = 1 << 2,
	Text	   = 1 << 3,
	TextLayout = 1 << 4,

	Visual = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
	All	   = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4),
};

constexpr ButtonDirty operator|(ButtonDirty lhs, ButtonDirty rhs) {
	return static_cast<ButtonDirty>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

constexpr ButtonDirty operator&(ButtonDirty lhs, ButtonDirty rhs) {
	return static_cast<ButtonDirty>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

constexpr ButtonDirty& operator|=(ButtonDirty& lhs, ButtonDirty rhs) {
	lhs = lhs | rhs;
	return lhs;
}

constexpr bool HasDirty(ButtonDirty dirty, ButtonDirty flag) {
	return (dirty & flag) != ButtonDirty::None;
}

struct ButtonTextEditSnapshot {
	ButtonVisualState state{ ButtonVisualState::Base };

	TextBox box;
	Origin origin{ Origin::Center };
	Transform transform;
};

struct ButtonData {
	InternalButtonState state{ InternalButtonState::IdleUp };
	ButtonDirty dirty{ ButtonDirty::All };

	std::optional<ButtonVisualState> applied_visual_state;
	std::optional<std::variant<V2_float, float>> applied_size;
	std::optional<Origin> applied_origin;
	std::optional<bool> applied_visibility;
};

/// @brief Marker for a direct child entity that represents one stable button
/// part.
struct ButtonChild {
	ButtonPart part{ ButtonPart::Background };

	constexpr bool operator==(const ButtonChild&) const = default;

	PTGN_SERIALIZE(ButtonChild, part)
};

struct ButtonShapeVisual {
	bool defined{ false };

	std::optional<std::variant<V2_float, float>> size;
	std::optional<Origin> origin;
	std::optional<Origin> anchor;
	std::optional<Color> color;
	std::optional<FillStyle> fill_style;

	PTGN_SERIALIZE(ButtonShapeVisual, defined, size, origin, anchor, color, fill_style)
};

struct ButtonShapeVisuals {
	std::array<ButtonShapeVisual, kButtonVisualStateCount> states;

	PTGN_SERIALIZE(ButtonShapeVisuals, states)
};

struct ButtonSpriteVisual {
	bool defined{ false };

	std::optional<std::string> texture;
	std::optional<Origin> origin;
	std::optional<Origin> anchor;
	std::optional<Transform> transform;
	std::optional<V2_float> size;
	std::optional<Color> tint;

	std::optional<AnimationConfig> animation;
	std::optional<ButtonAnimationOptions> animation_options;

	PTGN_SERIALIZE(
		ButtonSpriteVisual, defined, texture, origin, anchor, transform, size, tint, animation,
		animation_options
	)
};

struct ButtonSpriteVisuals {
	std::array<ButtonSpriteVisual, kButtonVisualStateCount> states;

	/// @brief Runtime state. Not serialized.
	std::optional<ButtonVisualState> applied_animation_state;
	bool transient_animation{ false };

	PTGN_SERIALIZE(ButtonSpriteVisuals, states)
};

struct ButtonTextVisual {
	bool defined{ false };

	std::optional<StyledText> styled_text;
	std::optional<TextBox> box;
	std::optional<Origin> origin;
	std::optional<Origin> anchor;
	std::optional<Transform> transform;
	std::optional<bool> auto_box;
	std::optional<Padding> padding;

	PTGN_SERIALIZE(
		ButtonTextVisual, defined, styled_text, box, origin, anchor, transform, auto_box, padding
	)
};

struct ButtonTextVisuals {
	std::array<ButtonTextVisual, kButtonVisualStateCount> states;

	/// @brief The state currently loaded into the single Text entity for fluent
	/// editing. Not serialized.
	std::optional<ButtonTextEditSnapshot> editing;

	PTGN_SERIALIZE(ButtonTextVisuals, states)
};

struct ButtonEnabled {
	bool press{ true };
	bool hover{ true };

	PTGN_SERIALIZE(ButtonEnabled, press, hover)
};

struct ButtonExclusiveAudio {};

struct ButtonSounds {
	std::optional<Audio> idle;
	std::optional<Audio> hover;
	std::optional<Audio> press;
};

/// @brief Runtime options for the animation currently applied to the
/// consolidated sprite part.
struct ButtonAnimationPart {
	ButtonAnimationOptions options;
};

struct ButtonVisualOverride {
	ButtonVisualState state{ ButtonVisualState::Base };
	bool block_press{ false };
};

struct ButtonAnimationCompleteScript;

class ButtonScript : public Script {
public:
	ButtonScript() = default;

	void OnEvent(Event event) override;

private:
	void OnMouseMoveOver() const;
	void OnMouseMoveOut() const;

	void OnMousePressedOver(Mouse mouse) const;
	void OnMousePressedOut(Mouse mouse) const;

	void OnMouseReleasedOver(Mouse mouse) const;
	void OnMouseReleasedOut(Mouse mouse) const;
};

void UpdateButtons(Scene& scene);

} // namespace impl

class Button : public Entity {
public:
	Button() = default;
	explicit Button(Entity entity);

	bool IsEnabled(bool check_for_hover_enabled = false) const;
	ButtonState GetState() const;
	ButtonVisualState GetVisualState() const;
	impl::InternalButtonState GetInternalState() const;

	/// @return Unscaled interactive shape size.
	std::variant<V2_float, float> GetSize() const;

	Button& Enable(bool enable_hover = true, bool reset_state = true);
	Button& Disable(bool disable_hover = true, bool reset_state = true);
	Button& SetEnabled(
		bool enable_activation = true, bool enable_hover = true, bool reset_state = true
	);

	Button& Press();
	Button& StartHover();
	Button& ContinueHover();
	Button& StopHover();

	Button& Size(V2_float size);
	Button& Size(float radius);

	Button& Background();

	/// @brief Sets which point of the background lies at the background
	/// transform.
	Button& BackgroundOrigin(Origin origin, ButtonVisualState state = ButtonVisualState::Base);
	Button& ClearBackgroundOrigin();

	/// @brief Sets which point of the button shape the background transform is
	/// anchored to.
	Button& BackgroundAnchor(Origin anchor, ButtonVisualState state = ButtonVisualState::Base);

	Button& BackgroundColor(Color color, ButtonVisualState state = ButtonVisualState::Base);
	Button& BackgroundColors(
		std::optional<Color> idle, std::optional<Color> hover = std::nullopt,
		std::optional<Color> press = std::nullopt
	);
	Button& ToggledBackgroundColors(
		std::optional<Color> toggled, std::optional<Color> toggled_hover = std::nullopt,
		std::optional<Color> toggled_press = std::nullopt
	);
	Button& DisabledBackgroundColors(
		std::optional<Color> disabled, std::optional<Color> disabled_hover = std::nullopt,
		std::optional<Color> disabled_press = std::nullopt
	);

	Button& BackgroundSize(V2_float size, ButtonVisualState state = ButtonVisualState::Base);
	Button& BackgroundSize(float radius, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every background state and destroys the consolidated
	/// background entity.
	Button& RemoveBackground();
	/// @brief Removes one background state. The state then falls back to
	/// less-specific states.
	Button& RemoveBackground(ButtonVisualState state);

	Button& Border();

	/// @brief Sets which point of the border lies at the border transform.
	Button& BorderOrigin(Origin origin, ButtonVisualState state = ButtonVisualState::Base);
	Button& ClearBorderOrigin();

	/// @brief Sets which point of the button shape the border transform is
	/// anchored to.
	Button& BorderAnchor(Origin anchor, ButtonVisualState state = ButtonVisualState::Base);

	Button& BorderColor(Color color, ButtonVisualState state = ButtonVisualState::Base);
	Button& BorderColors(
		std::optional<Color> idle, std::optional<Color> hover = std::nullopt,
		std::optional<Color> press = std::nullopt
	);

	Button& BorderWidth(FillStyle fill, ButtonVisualState state = ButtonVisualState::Base);

	Button& BorderSize(V2_float size, ButtonVisualState state = ButtonVisualState::Base);
	Button& BorderSize(float radius, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every border state and destroys the consolidated border
	/// entity.
	Button& RemoveBorder();
	/// @brief Removes one border state. The state then falls back to
	/// less-specific states.
	Button& RemoveBorder(ButtonVisualState state);

	/// @brief Begins editing the requested state on the single consolidated text
	/// entity. The edit is committed before the next state edit or button visual
	/// refresh.
	ptgn::Text Text(ButtonVisualState state = ButtonVisualState::Base);
	ptgn::Text Text(
		std::string_view content, Color color = kDefaultButtonTextColor,
		float font_size = kDefaultFontSize, ButtonVisualState state = ButtonVisualState::Base
	);
	ptgn::Text Text(StyledText styled_text, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Sets which point of the text box lies at the text transform. If unset defaults to the
	/// text anchor, which defaults to the button center.
	Button& TextOrigin(Origin origin, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Sets which point of the button shape the text transform is anchored
	/// to. If unset defaults to the button center.
	Button& TextAnchor(Origin anchor, ButtonVisualState state = ButtonVisualState::Base);

	Button& ClearTextOrigin(ButtonVisualState state = ButtonVisualState::Base);

	Button& ClearTextAnchor(ButtonVisualState state = ButtonVisualState::Base);

	Button& TextAutoBox(bool enabled = true, ButtonVisualState state = ButtonVisualState::Base);
	Button& TextPadding(Padding padding, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every text state and destroys the consolidated text entity.
	Button& RemoveText();
	/// @brief Removes one text state. The state then falls back to less-specific
	/// states.
	Button& RemoveText(ButtonVisualState state);

	/// @param origin Origin of the sprite itself. When omitted it is inherited
	/// through the visual fallback chain and ultimately defaults to the button
	/// origin.
	Button& Sprite(
		std::string_view texture_key, std::optional<Origin> origin = std::nullopt,
		ButtonVisualState state = ButtonVisualState::Base
	);
	Button& Sprites(
		std::optional<std::string_view> idle_texture_key,
		std::optional<std::string_view> hover_texture_key = std::nullopt,
		std::optional<std::string_view> press_texture_key = std::nullopt
	);

	Button& SpriteAnchor(Origin anchor, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every sprite and animation state and destroys the
	/// consolidated sprite entity.
	Button& RemoveSprite();
	/// @brief Removes one sprite/animation state.
	Button& RemoveSprite(ButtonVisualState state);

	Button& Animation(
		AnimationConfig config, std::optional<Origin> origin, ButtonVisualState state,
		ButtonAnimationOptions options
	);
	Button& Animation(
		AnimationConfig config, std::optional<Origin> origin = std::nullopt,
		ButtonVisualState state = ButtonVisualState::Base
	);
	Button& Animation(
		std::optional<AnimationConfig> idle_animation,
		std::optional<AnimationConfig> hover_animation = std::nullopt,
		std::optional<AnimationConfig> press_animation = std::nullopt
	);

	Button& StaticAnimationFrame(
		AnimationConfig config, std::optional<Origin> origin = std::nullopt,
		ButtonVisualState state = ButtonVisualState::Idle, std::size_t frame = 0
	);

	/// @brief Preserves the old behavior: removes every sprite/animation state.
	Button& RemoveAnimation();
	/// @brief Preserves the old behavior: removes the entire sprite/animation
	/// state.
	Button& RemoveAnimation(ButtonVisualState state);

	Button& Sounds(
		std::optional<std::string_view> press_sound_key,
		std::optional<std::string_view> hover_sound_key = std::nullopt
	);
	Button& Sound(std::optional<std::string_view> sound_key, ButtonState state);

	Button& ExclusiveAudio(bool enabled);

	[[nodiscard]] std::optional<Audio> GetSound(ButtonState state) const;

	template <EventCallbackInvocable<event::ButtonPress> F>
	Button& OnPress(F&& callback) {
		return OnEvent<event::ButtonPress>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::ButtonHoverStart> F>
	Button& OnHoverStart(F&& callback) {
		return OnEvent<event::ButtonHoverStart>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::ButtonHover> F>
	Button& OnHover(F&& callback) {
		return OnEvent<event::ButtonHover>(std::forward<F>(callback));
	}

	template <EventCallbackInvocable<event::ButtonHoverStop> F>
	Button& OnHoverStop(F&& callback) {
		return OnEvent<event::ButtonHoverStop>(std::forward<F>(callback));
	}

private:
	friend class impl::ButtonScript;
	friend struct impl::ButtonAnimationCompleteScript;
	friend void impl::UpdateButtons(Scene& scene);
	friend class Dropdown;
	friend class ToggleButton;
	friend Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);
	friend Button CreateAnimatedButton(
		Scene& scene, Transform transform, const AnimatedButtonConfig& config
	);

	bool HasPart(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base) const;
	Entity Part(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base);

	ptgn::Text GetText(ButtonVisualState state);

	Button& Border(ButtonVisualState state);
	Button& Background(ButtonVisualState state);

	/// @brief Compatibility entry point used by ToggleButton/Dropdown code.
	void RefreshVisualState() const;

	Button& RemovePart(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base);
	Button& RemoveParts(ButtonPart part);

	Entity ShapePart(ButtonPart part, ButtonVisualState state, Color color, FillStyle fill);

	template <typename E, EventCallbackInvocable<E> F>
	Button& OnEvent(F&& callback) {
		AddScript<impl::EventScript<E>>(
			*this, impl::MakeEventCallback<E>(std::forward<F>(callback))
		);
		return *this;
	}

	void SetState(impl::InternalButtonState state);

	void PlaySound(ButtonState active);
	void PlayAnimation(ButtonState active) const;

	void MarkDirty(impl::ButtonDirty dirty);
	void RefreshDirty();
	void CommitTextEdit();

	void ApplyShapeVisual(ButtonPart part);
	void ApplySpriteVisual();
	void ApplySpriteVisual(ButtonVisualState state, bool transient);
	void ApplyTextVisual();

	void UpdateChildSizes() const;
	void UpdateChildLayouts() const;
};

namespace impl {

struct ButtonAnimationCompleteScript : public Script {
	ButtonAnimationCompleteScript() = default;
	explicit ButtonAnimationCompleteScript(Button button);

	Button button;

	void OnEvent(Event event) override;
};

} // namespace impl

namespace event {

struct ButtonPress {
	operator Button() const { // NOSONAR
		return button;
	}

	Button button;
};

struct ButtonHoverStart {
	operator Button() const { // NOSONAR
		return button;
	}

	Button button;
};

struct ButtonHover {
	operator Button() const { // NOSONAR
		return button;
	}

	Button button;
};

struct ButtonHoverStop {
	operator Button() const { // NOSONAR
		return button;
	}

	Button button;
};

} // namespace event

/// @brief Creates a button with the given transform and origin without a specified size.
Button CreateButton(Scene& scene, Transform transform, Origin origin = Origin::Center);

Button CreateButton(
	Scene& scene, Transform transform, V2_float size, Origin origin = Origin::Center
);

Button CreateButton(
	Scene& scene, Transform transform, float radius, Origin origin = Origin::Center
);

Button CreateButton(Scene& scene, Transform transform, V2_float size, const ButtonConfig& config);

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);

Button CreateAnimatedButton(Scene& scene, Transform transform, const AnimatedButtonConfig& config);

} // namespace ptgn
