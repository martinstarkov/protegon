#pragma once

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/mouse.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class ToggleButton;
class DrawContext;
class Scene;

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

inline constexpr Color kDefaultButtonTextColor{ color::White };

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
struct ButtonPart {
	ButtonPartRole role{ ButtonPartRole::Custom };
	ButtonVisualState state{ ButtonVisualState::Base };

	PTGN_SERIALIZE(ButtonPart, role, state)
};

/// @brief Optional metadata for label children. Text behavior itself stays on Text.
struct ButtonLabelAutoBox {
	bool enabled{ true };
	Rect padding;

	PTGN_SERIALIZE(ButtonLabelAutoBox, enabled, padding)
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

struct ButtonAnimationCompleteScript : public Script {
	ButtonAnimationCompleteScript() = default;
	explicit ButtonAnimationCompleteScript(Entity button);

	Entity button;

	void OnEvent(Event event) override;
};

class ButtonScript : public Script {
public:
	ButtonScript() = default;

	void OnEvent(Event event) override;

private:
	void OnMouseMoveOver();
	void OnMouseMoveOut();

	void OnMousePressedOver(Mouse mouse);
	void OnMousePressedOut(Mouse mouse);

	void OnMouseReleasedOver(Mouse mouse);
	void OnMouseReleasedOut(Mouse mouse);
};

void UpdateButtons(Scene& scene);

} // namespace impl

class Button : public Entity {
public:
	Button() = default;
	explicit Button(Entity entity);

	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;
	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] ButtonVisualState GetVisualState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

	/// @brief Interactive shape, not necessarily the visual background child.
	[[nodiscard]] std::optional<std::variant<Rect, Circle>> GetShape() const;

	Button& Enable(bool enable_hover = true, bool reset_state = true);
	Button& Disable(bool disable_hover = true, bool reset_state = true);
	Button& SetEnabled(
		bool enable_activation = true, bool enable_hover = true, bool reset_state = true
	);

	Button& Press();
	Button& StartHover();
	Button& ContinueHover();
	Button& StopHover();

	Button& SetShape(const std::optional<std::variant<Rect, Circle>>& shape = {});
	Button& SetShape(Rect rect);
	Button& SetShape(Circle circle);
	Button& SetSize(V2_float size);
	Button& RemoveShape();

	/// @brief Returns an existing direct child part or creates one.
	Entity Part(ButtonPartRole role, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Returns an existing direct child part.
	[[nodiscard]] std::optional<Entity> TryPart(
		ButtonPartRole role, ButtonVisualState state = ButtonVisualState::Base
	) const;

	[[nodiscard]] std::vector<Entity> Parts(ButtonPartRole role) const;
	[[nodiscard]] std::vector<Entity> Parts() const;

	/// @brief Removes the part marker and hides the child. Replace with entity destruction if
	/// desired.
	Button& RemovePart(ButtonPartRole role, ButtonVisualState state = ButtonVisualState::Base);

	Entity Background(ButtonVisualState state = ButtonVisualState::Base);
	Entity Border(ButtonVisualState state = ButtonVisualState::Base);

	Text Label(ButtonVisualState state = ButtonVisualState::Base);
	Sprite Icon(ButtonVisualState state = ButtonVisualState::Base);

	[[nodiscard]] std::optional<Entity> TryBackground(
		ButtonVisualState state = ButtonVisualState::Base
	) const;

	[[nodiscard]] std::optional<Entity> TryBorder(
		ButtonVisualState state = ButtonVisualState::Base
	) const;

	[[nodiscard]] std::optional<Text> TryLabel(
		ButtonVisualState state = ButtonVisualState::Base
	) const;

	[[nodiscard]] std::optional<Sprite> TryIcon(
		ButtonVisualState state = ButtonVisualState::Base
	) const;

	Button& RemoveBackground(ButtonVisualState state = ButtonVisualState::Base);
	Button& RemoveBorder(ButtonVisualState state = ButtonVisualState::Base);
	Button& RemoveLabel(ButtonVisualState state = ButtonVisualState::Base);
	Button& RemoveIcon(ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Convenience only. Further label configuration should use the Text API.
	Button& SetLabel(std::string_view content, ButtonVisualState state = ButtonVisualState::Base);

	/// @brief Convenience only. Further icon configuration should use the Sprite API.
	Button& SetIcon(
		std::string_view texture_key, ButtonVisualState state = ButtonVisualState::Base
	);

	Button& SetTexture(
		std::string_view texture_key, ButtonVisualState state = ButtonVisualState::Idle
	);

	Button& SetAnimation(
		Animation&& animation, ButtonVisualState state, ButtonAnimationOptions options
	);

	Button& SetAnimation(Animation&& animation, ButtonVisualState state);

	Button& SetStaticAnimationFrame(
		Animation&& animation, ButtonVisualState state = ButtonVisualState::Idle,
		std::size_t frame = 0
	);

	[[nodiscard]] std::optional<Animation> TryAnimation(ButtonVisualState state) const;

	Button& RemoveAnimation(ButtonVisualState state);

	Button& SetLabelAutoBox(bool enabled = true, ButtonVisualState state = ButtonVisualState::Base);

	Button& SetLabelPadding(Rect padding, ButtonVisualState state = ButtonVisualState::Base);

	Button& SetSound(std::optional<std::string_view> sound_key, ButtonState state);
	[[nodiscard]] std::optional<Audio> GetSound(ButtonState state) const;

	Button& SetExclusiveAudio(bool enabled);

	/// @brief Shows/hides state-specific child parts according to current visual state.
	void RefreshVisualState();

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

	/// @brief Declared here, implemented in toggle_button.cpp.
	ToggleButton AddToggle(bool toggled = false);
	[[nodiscard]] bool HasToggle() const;
	[[nodiscard]] ToggleButton AsToggle() const;

private:
	friend class impl::ButtonScript;
	friend struct impl::ButtonAnimationCompleteScript;
	friend void impl::UpdateButtons(Scene& scene);

	template <typename E, EventCallbackInvocable<E> F>
	Button& OnEvent(F&& callback) {
		AddScript<impl::EventScript<E>>(
			*this, impl::MakeEventCallback<E>(std::forward<F>(callback))
		);
		return *this;
	}

	void SetState(impl::InternalButtonState state);

	void PlaySound(ButtonState active);
	void PlayAnimation(ButtonState active);

	void UpdateChildLayouts();
};

namespace event {

struct ButtonPress {
	Button button;
};

struct ButtonHoverStart {
	Button button;
};

struct ButtonHover {
	Button button;
};

struct ButtonHoverStop {
	Button button;
};

} // namespace event

Button CreateButton(Scene& scene, const ButtonDesc& desc);

Button CreateButton(
	Scene& scene, V2_float position = {},
	const std::optional<std::variant<Rect, Circle>>& shape = {}, Origin draw_origin = Origin::Center
);

Button CreateButton(
	Scene& scene, V2_float position, V2_float size, const ButtonConfig& config,
	Origin draw_origin = Origin::Center
);

Button CreateAnimatedButton(
	Scene& scene, V2_float position, std::optional<V2_float> size,
	const AnimatedButtonConfig& config, Origin draw_origin = Origin::Center
);

} // namespace ptgn