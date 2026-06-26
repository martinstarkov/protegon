#pragma once

#include <cstdint>
#include <optional>
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

struct ButtonAnimationOptions {
	ButtonAnimationPlayback playback{ ButtonAnimationPlayback::Play };

	/// @brief Used only for StaticFrame.
	std::size_t static_frame{ 0 };

	/// @brief If true, the button keeps showing this visual state until the animation completes.
	bool lock_visual_state{ false };

	/// @brief If true, repeated presses are ignored while this visual state is locked.
	bool block_press{ false };
};

namespace impl {

enum class InternalButtonState : std::uint8_t {
	IdleUp,
	Hover,
	Pressed,
	HeldOutside,
	IdleDown,
	HoverPressed,
};
PTGN_SERIALIZE_ENUM(InternalButtonState);

struct ButtonData {
	InternalButtonState state{ InternalButtonState::IdleUp };
};

/// @brief Marker for direct child entities that are part of a button view.
struct ButtonChild {
	ButtonPart part{ ButtonPart::Background };
	ButtonVisualState state{ ButtonVisualState::Base };

	constexpr bool operator==(const ButtonChild& o) const = default;

	PTGN_SERIALIZE(ButtonChild, part, state)
};

struct ButtonShapeSync {};

struct ButtonOriginSync {};

/// @brief Optional metadata for text children. Text behavior itself stays on Text.
struct ButtonTextAutoBox {
	bool enabled{ true };
	Padding padding;
	Origin origin{ Origin::Center };

	PTGN_SERIALIZE(ButtonTextAutoBox, enabled, padding, origin)
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

	/// @brief Sets the origin of the background shape for all the visual states.
	/// By default the origin will be the same as the button's origin.
	Button& BackgroundOrigin(Origin origin);
	Button& ClearBackgroundOrigin();

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

	Button& BackgroundShape(Rect rect, ButtonVisualState state = ButtonVisualState::Base);
	Button& BackgroundShape(Circle circle, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every background state.
	Button& RemoveBackground();
	Button& RemoveBackground(ButtonVisualState state);

	/// @brief Removes every border state.
	Button& RemoveBorder();
	Button& RemoveBorder(ButtonVisualState state);

	Button& Border();

	/// @brief Sets the origin of the border for all the visual states.
	/// By default the origin will be the same as the button's origin.
	Button& BorderOrigin(Origin origin);
	Button& ClearBorderOrigin();

	Button& BorderColor(Color color, ButtonVisualState state = ButtonVisualState::Base);
	Button& BorderColors(
		std::optional<Color> idle, std::optional<Color> hover = std::nullopt,
		std::optional<Color> press = std::nullopt
	);

	Button& BorderWidth(FillStyle fill, ButtonVisualState state = ButtonVisualState::Base);

	Button& BorderShape(Rect rect, ButtonVisualState state = ButtonVisualState::Base);
	Button& BorderShape(Circle circle, ButtonVisualState state = ButtonVisualState::Base);

	ptgn::Text Text(ButtonVisualState state = ButtonVisualState::Base);
	ptgn::Text Text(
		std::string_view content, Color color = kDefaultButtonTextColor,
		float font_size = kDefaultFontSize, ButtonVisualState state = ButtonVisualState::Base
	);
	ptgn::Text Text(StyledText styled_text, ButtonVisualState state = ButtonVisualState::Base);

	Button& TextOrigin(Origin origin, ButtonVisualState state = ButtonVisualState::Base);
	Button& TextAutoBox(bool enabled = true, ButtonVisualState state = ButtonVisualState::Base);
	Button& TextPadding(Padding padding, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Removes every text state.
	Button& RemoveText();
	Button& RemoveText(ButtonVisualState state);

	/// @param origin If not specified, the origin of the sprite will be the same as the button's
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

	/// @brief Removes every sprite state.
	Button& RemoveSprite();
	Button& RemoveSprite(ButtonVisualState state);

	Button& Animation(
		ptgn::Animation animation, ButtonVisualState state, ButtonAnimationOptions options
	);
	Button& Animation(ptgn::Animation animation, ButtonVisualState state);

	Button& StaticAnimationFrame(
		ptgn::Animation animation, ButtonVisualState state = ButtonVisualState::Idle,
		std::size_t frame = 0
	);

	/// @brief Removes every sprite (animation) state.
	Button& RemoveAnimation();
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

	/// @return True if the button has a direct child part for the given visual state.
	bool HasPart(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base) const;

	/// @return An existing direct child part or a newly created one.
	Entity Part(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base);

	[[nodiscard]] std::vector<Entity> Parts(ButtonPart part) const;
	[[nodiscard]] std::vector<Entity> Parts() const;

private:
	friend class impl::ButtonScript;
	friend struct impl::ButtonAnimationCompleteScript;
	friend void impl::UpdateButtons(Scene& scene);
	friend class Dropdown;
	friend class ToggleButton;

	ptgn::Text GetText(ButtonVisualState state);

	Button& Border(ButtonVisualState state);
	Button& Background(ButtonVisualState state);

	/// @brief Shows/hides state specific child parts according to current visual state.
	void RefreshVisualState() const;

	/// @brief Destroys the direct child part for the given visual state if it exists.
	Button& RemovePart(ButtonPart part, ButtonVisualState state = ButtonVisualState::Base);

	Button& RemoveParts(ButtonPart part);

	/// @brief Sets the color and fill style of a direct child part for the given visual state.
	Button& ShapePart(ButtonPart part, ButtonVisualState state, Color color, FillStyle fill);

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

	void UpdateChildShapes() const;
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