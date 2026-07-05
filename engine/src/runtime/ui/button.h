#pragma once

#include <array>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/mouse.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/entity.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button_config.h"
#include "serialization/serialize.h"

namespace ptgn {

class Button;
class ButtonShape;
class ButtonBackground;
class ButtonBorder;
class ButtonText;
class ButtonSprite;
class ButtonAnimation;
class Scene;

namespace event {

struct ButtonPress;
struct ButtonHoverStart;
struct ButtonHover;
struct ButtonHoverStop;

} // namespace event

namespace impl {

enum class ButtonPart : std::uint8_t {
	Background,
	Border,
	Sprite,
	Text,
};
PTGN_SERIALIZE_ENUM(ButtonPart);

enum class InternalButtonState : std::uint8_t {
	IdleUp,
	Hover,
	Pressed,
	HeldOutside,
	IdleDown,
	HoverPressed,
};

enum class ButtonDirty : std::uint8_t {
	None	   = 0,
	Background = 1 << 0,
	Border	   = 1 << 1,
	Text	   = 1 << 2,
	Sprite	   = 1 << 3,

	All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3),
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

struct ButtonData {
	InternalButtonState state{ InternalButtonState::IdleUp };

	bool press_enabled{ true };
	bool hover_enabled{ true };
	bool toggled{ false };

	struct VisualLock {
		ButtonVisualState state{ ButtonVisualState::Idle };
		bool block_press{ false };
	};

	std::optional<VisualLock> visual_lock;

	std::optional<ButtonVisualState> applied_visual_state;

	ButtonDirty dirty{ ButtonDirty::All };

	PTGN_SERIALIZE(ButtonData, press_enabled, hover_enabled, toggled)
};

struct ButtonAnimationPart {
	ButtonAnimationOptions options;
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

	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;
	[[nodiscard]] bool IsToggled() const;

	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] ButtonVisualState GetVisualState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

	/// @return Unscaled interactive shape size.
	[[nodiscard]] std::variant<V2_float, float> GetSize() const;

	Button& Enable(bool enable_hover = true, bool reset_state = true);
	Button& Disable(bool disable_hover = true, bool reset_state = true);
	Button& SetEnabled(bool enable_press = true, bool enable_hover = true, bool reset_state = true);

	Button& SetToggled(bool toggled);
	Button& Toggle();

	Button& Press();
	Button& StartHover();
	Button& ContinueHover();
	Button& StopHover();

	Button& Size(V2_float size);
	Button& Size(float radius);

	ButtonBackground Background(ButtonVisualState state = ButtonVisualState::Idle);
	ButtonBorder Border(ButtonVisualState state = ButtonVisualState::Idle);
	ButtonText Text(ButtonVisualState state = ButtonVisualState::Idle);
	ButtonSprite Sprite(ButtonVisualState state = ButtonVisualState::Idle);
	ButtonAnimation Animation(ButtonVisualState state = ButtonVisualState::Idle);

	Button& RemoveBackgrounds();
	Button& RemoveBackground(ButtonVisualState state);

	Button& RemoveBorders();
	Button& RemoveBorder(ButtonVisualState state);

	Button& RemoveTexts();
	Button& RemoveText(ButtonVisualState state);

	Button& RemoveSprites();
	Button& RemoveSprite(ButtonVisualState state);

	Button& RemoveAnimations();
	Button& RemoveAnimation(ButtonVisualState state);

	Button& Sound(std::optional<std::string_view> sound_key, ButtonVisualState state);
	Button& RemoveSound(ButtonVisualState state);
	Button& RemoveSounds();
	Button& ExclusiveAudio(bool enabled = true);

	[[nodiscard]] std::optional<Audio> GetSound(ButtonVisualState state) const;

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
	friend class ButtonShape;
	friend class ButtonText;
	friend class ButtonSprite;
	friend class ButtonAnimation;
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

	void MarkDirty(impl::ButtonDirty dirty);
	void RefreshDirty();
	void RefreshVisualState() const;

	Button& LockVisualState(ButtonVisualState state, bool block_press = false);
	Button& UnlockVisualState();

	Entity EnsurePart(impl::ButtonPart part) const;
	[[nodiscard]] std::optional<Entity> FindPart(impl::ButtonPart part) const;

	ButtonShapeVisual& ShapeVisual(impl::ButtonPart part, ButtonVisualState state);
	ButtonTextVisual& TextVisual(ButtonVisualState state);
	ButtonSpriteVisual& SpriteVisual(ButtonVisualState state);

	[[nodiscard]] StyledText GetTextFallback(ButtonVisualState state) const;

	void ApplyShapeVisual(impl::ButtonPart part) const;
	void ApplyTextVisual() const;
	void ApplySpriteVisual() const;
	void ApplySpriteVisual(ButtonVisualState state, bool transient) const;

	void PlaySound(ButtonVisualState state);
	void PlayAnimation(ButtonState state) const;

	Button& RemovePart(impl::ButtonPart part, ButtonVisualState state);
	Button& RemoveParts(impl::ButtonPart part);
};

class ButtonShape {
public:
	ButtonShape& Size(V2_float size);
	ButtonShape& Size(float radius);
	ButtonShape& ClearSize();

	ButtonShape& Origin(ptgn::Origin origin);
	ButtonShape& ClearOrigin();

	ButtonShape& Anchor(ptgn::Origin anchor);
	ButtonShape& ClearAnchor();

	ButtonShape& Transform(ptgn::Transform transform = {});

	ButtonShape& Color(ptgn::Color color);
	ButtonShape& ClearColor();

	ButtonShape& Fill(FillStyle fill_style);
	ButtonShape& ClearFill();

	ButtonShape& Clear();

protected:
	ButtonShape(Button button, impl::ButtonPart part, ButtonVisualState state);

	Button button_;
	impl::ButtonPart part{ impl::ButtonPart::Background };
	ButtonVisualState state{ ButtonVisualState::Idle };
};

class ButtonBackground : public ButtonShape {
public:
	ButtonBackground(Button button, ButtonVisualState state);
};

class ButtonBorder : public ButtonShape {
public:
	ButtonBorder(Button button, ButtonVisualState state);
};

class ButtonText {
public:
	ButtonText(Button button, ButtonVisualState state);

	ButtonText& Clear();
	ButtonText& Select(std::size_t index);

	ButtonText& Content(std::string_view content);
	ButtonText& Content(StyledText styled_text);
	ButtonText& ClearContent();

	ButtonText& Box(TextBox box);
	ButtonText& ClearBox();

	ButtonText& Origin(ptgn::Origin origin);
	ButtonText& ClearOrigin();

	ButtonText& Anchor(ptgn::Origin anchor);
	ButtonText& ClearAnchor();

	ButtonText& Transform(ptgn::Transform transform = {});

	ButtonText& AutoBox(bool enabled = true);
	ButtonText& ClearAutoBox();

	ButtonText& Padding(ptgn::Padding padding);
	ButtonText& ClearPadding();

	ButtonText& Font(std::string_view font);
	ButtonText& Color(ptgn::Color color);
	ButtonText& Size(float font_size);

	ButtonText& Style(FontStyle flags);
	ButtonText& Bold(bool enabled = true, float weight = kDefaultBoldWeight);
	ButtonText& Italic(bool enabled = true);
	ButtonText& Underline(bool enabled = true);
	ButtonText& Strikethrough(bool enabled = true);

	ButtonText& Outline(ptgn::Color color, float width, float softness = 1.0f);
	ButtonText& Shadow(ptgn::Color color, V2_float offset, float softness = 1.0f);
	ButtonText& Shadow(ptgn::Color color, V2_float offset, float width, float softness);
	ButtonText& OuterGlow(ptgn::Color color, float width, float softness = 1.0f);
	ButtonText& InnerGlow(ptgn::Color color, float width, float softness = 1.0f);

	ButtonText& ClearSdfEffects();

	ButtonText& Effect(
		GlyphEffectType type, float amplitude, float frequency, float speed, float phase = 0.0f
	);

private:
	StyledText& StyledTextForEdit();
	TextRun& CurrentRun();
	void MarkTextDirty();

	Button button_;
	ButtonVisualState state{ ButtonVisualState::Idle };
};

class ButtonSprite {
public:
	ButtonSprite(Button button, ButtonVisualState state);

	ButtonSprite& Texture(std::string_view texture_key);
	ButtonSprite& ClearTexture();

	ButtonSprite& Origin(ptgn::Origin origin);
	ButtonSprite& ClearOrigin();

	ButtonSprite& Anchor(ptgn::Origin anchor);
	ButtonSprite& ClearAnchor();

	ButtonSprite& Transform(ptgn::Transform transform = {});

	ButtonSprite& Size(V2_float size);
	ButtonSprite& ClearSize();

	ButtonSprite& Tint(ptgn::Color tint);
	ButtonSprite& ClearTint();

	ButtonSprite& Clear();

protected:
	Button button_;
	ButtonVisualState state{ ButtonVisualState::Idle };
};

class ButtonAnimation : public ButtonSprite {
public:
	ButtonAnimation(Button button, ButtonVisualState state);

	ButtonAnimation& Texture(std::string_view texture_key);
	ButtonAnimation& ClearTexture();

	ButtonAnimation& Origin(ptgn::Origin origin);
	ButtonAnimation& ClearOrigin();

	ButtonAnimation& Anchor(ptgn::Origin anchor);
	ButtonAnimation& ClearAnchor();

	ButtonAnimation& Transform(ptgn::Transform transform = {});

	ButtonAnimation& Size(V2_float size);
	ButtonAnimation& ClearSize();

	ButtonAnimation& Tint(ptgn::Color tint);
	ButtonAnimation& ClearTint();

	ButtonAnimation& Config(AnimationConfig config, ButtonAnimationOptions options = {});
	ButtonAnimation& StaticFrame(AnimationConfig config, std::size_t frame = 0);
	ButtonAnimation& ClearConfig();

	ButtonAnimation& Clear();
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

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);

Button CreateButton(Scene& scene, Transform transform, V2_float size, const ButtonConfig& config);

Button CreateAnimatedButton(Scene& scene, Transform transform, const AnimatedButtonConfig& config);

/// @brief Creates a button with the given transform and origin without a specified size.
Button CreateButton(Scene& scene, Transform transform, Origin origin = Origin::Center);

Button CreateButton(
	Scene& scene, Transform transform, V2_float size, Origin origin = Origin::Center
);

Button CreateButton(
	Scene& scene, Transform transform, float radius, Origin origin = Origin::Center
);

} // namespace ptgn
