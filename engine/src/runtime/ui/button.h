#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/mouse.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/text/text.h"
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
class Dropdown;
class Scene;

Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);

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
PTGN_REFLECT_ENUM(ButtonPart);

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

	struct VisualLock {
		ButtonVisualState state{ ButtonVisualState::Idle };
		bool block_press{ false };

		PTGN_REFLECT(VisualLock, state, block_press)
	};

	std::optional<VisualLock> visual_lock{};

	std::optional<ButtonVisualState> applied_visual_state{};

	ButtonDirty dirty{ ButtonDirty::All };

	PTGN_REFLECT(ButtonData, press_enabled, hover_enabled)
	PTGN_REFLECT_READONLY(ButtonData, state, visual_lock, applied_visual_state, dirty)
};

struct ButtonAnimationPart {
	ButtonAnimationOptions options{};

	PTGN_REFLECT_VALUE(ButtonAnimationPart, options)
};

struct ButtonAnimationCompleteScript;

struct ButtonSystem {
	/// @brief Ensures ButtonData entities participate in the interaction system.
	static void Prepare(Scene& scene);

	/// @brief Converts raw interaction events into button state changes and semantic button events.
	static void OnEvent(Entity entity, Event event);

	/// @brief Refreshes button visuals after runtime state changes.
	static void Update(Scene& scene);

private:
	static void OnMouseMoveOver(Entity entity);
	static void OnMouseMoveOut(Entity entity);

	static void OnMousePressedOver(Entity entity, Mouse mouse);
	static void OnMousePressedOut(Entity entity, Mouse mouse);

	static void OnMouseReleasedOver(Entity entity, Mouse mouse);
	static void OnMouseReleasedOut(Entity entity, Mouse mouse);
};

} // namespace impl

class Button : public Entity {
public:
	Button() = default;
	explicit Button(Entity entity);

	[[nodiscard]] bool IsEnabled(bool check_for_hover_enabled = false) const;

	[[nodiscard]] ButtonState GetState() const;
	[[nodiscard]] ButtonVisualState GetVisualState() const;
	[[nodiscard]] impl::InternalButtonState GetInternalState() const;

	/// @brief Forces a visual state without changing the logical interaction state.
	/// Passing std::nullopt restores the state resolved from interaction/toggle/disabled state.
	/// This is useful for editor previews and other non-interactive presentation.
	Button& PreviewVisualState(std::optional<ButtonVisualState> state);

	/// @return Unscaled interactive shape size.
	[[nodiscard]] std::variant<V2_float, float> GetSize() const;

	Button& Enable(bool enable_hover = true, bool reset_state = true);
	Button& Disable(bool disable_hover = true, bool reset_state = true);
	Button& SetEnabled(bool enable_press = true, bool enable_hover = true, bool reset_state = true);

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

	Button& Sound(std::optional<AudioKey> sound_key, ButtonVisualState state);
	Button& Sounds(std::optional<AudioKey> hover, std::optional<AudioKey> press);
	Button& RemoveSound(ButtonVisualState state);
	Button& RemoveSounds();
	Button& ExclusiveAudio(bool enabled = true);

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

protected:
	void MarkDirty(impl::ButtonDirty dirty);
	void RefreshDirty();

private:
	friend class ButtonShape;
	friend class ButtonText;
	friend class ButtonSprite;
	friend class ButtonAnimation;
	friend class Dropdown;
	friend struct impl::ButtonSystem;
	friend struct impl::ButtonAnimationCompleteScript;
	friend Button CreateButton(Scene& scene, Transform transform, const ButtonDesc& desc);

	template <typename E, EventCallbackInvocable<E> F>
	Button& OnEvent(F&& callback) {
		AddScript<impl::EventScript<E>>(
			*this, impl::MakeEventCallback<E>(std::forward<F>(callback))
		);
		return *this;
	}

	ButtonVisualState GetVisualState(ButtonState state, bool check_for_visual_lock) const;

	void SetState(impl::InternalButtonState state);

	void RefreshVisualState() const;

	Button& LockVisualState(ButtonVisualState state, bool block_press = false);
	Button& UnlockVisualState();

	Entity EnsurePart(impl::ButtonPart part);
	/// @return May return a null entity if part is not found.
	[[nodiscard]] Entity FindPart(impl::ButtonPart part) const;

	ButtonShapeVisual& ShapeVisual(impl::ButtonPart part, ButtonVisualState state);
	ButtonTextVisual& TextVisual(ButtonVisualState state);
	ButtonSpriteVisual& SpriteVisual(ButtonVisualState state);

	[[nodiscard]] StyledText GetTextFallback(ButtonVisualState state) const;

	void ApplyShapeVisual(impl::ButtonPart part) const;
	void ApplyTextVisual() const;
	void ApplySpriteVisual() const;
	void ApplySpriteVisual(ButtonVisualState state) const;

	void PlaySound(ButtonVisualState state);
	void PlayAnimation(ButtonState state) const;

	Button& RemovePart(impl::ButtonPart part, ButtonVisualState state);
	Button& RemoveParts(impl::ButtonPart part);
};

class ButtonShape {
public:
	operator ptgn::Button() const; // NOSONAR

	ptgn::Button Button() const;

	ButtonShape& Size(V2_float size);
	ButtonShape& Size(float radius);
	ButtonShape& ClearSize();

	ButtonShape& Origin(ptgn::Origin origin);
	ButtonShape& ClearOrigin();

	ButtonShape& Anchor(ptgn::Origin anchor);
	ButtonShape& ClearAnchor();

	ButtonShape& Transform(ptgn::Transform transform = {});

	ButtonShape& Color(ptgn::Color color);
	ButtonShape& Colors(
		std::optional<ptgn::Color> idle, std::optional<ptgn::Color> hover = std::nullopt,
		std::optional<ptgn::Color> press = std::nullopt
	);
	ButtonShape& ClearColor();

	ButtonShape& Fill(FillStyle fill_style);
	ButtonShape& ClearFill();

	ButtonShape& Clear();

protected:
	ButtonShape(ptgn::Button button, impl::ButtonPart part, ButtonVisualState state);

private:
	ButtonShape& Color(ptgn::Color color, ButtonVisualState state);

	ptgn::Button button_;
	impl::ButtonPart part_{ impl::ButtonPart::Background };
	ButtonVisualState state_{ ButtonVisualState::Idle };
};

class ButtonBackground : public ButtonShape {
public:
	ButtonBackground(ptgn::Button button, ButtonVisualState state);
};

class ButtonBorder : public ButtonShape {
public:
	ButtonBorder(ptgn::Button button, ButtonVisualState state);
};

class ButtonText {
public:
	ButtonText(ptgn::Button button, ButtonVisualState state);

	operator ptgn::Button() const; // NOSONAR

	ptgn::Button Button() const;

	/// @brief Removes all text visuals from the button.
	ButtonText& Clear();

	ButtonText& Content(std::string_view content);
	ButtonText& Content(StyledText styled_text);

	/// @brief Compiles rich-text markup into the StyledText for this visual state.
	ButtonText& SetRichText(std::string_view source);

	ButtonText& ClearContent();

	ButtonText& Box(TextBox box);
	ButtonText& ClearBox();

	ButtonText& Align(ptgn::Origin origin);
	ButtonText& Align(Alignment alignment);
	ButtonText& Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical);
	ButtonText& HorizontalAlign(ptgn::HorizontalAlign align);
	ButtonText& VerticalAlign(ptgn::VerticalAlign align);
	ButtonText& ClearAlignment();

	ButtonText& Origin(ptgn::Origin origin);
	ButtonText& ClearOrigin();

	ButtonText& Anchor(ptgn::Origin anchor);
	ButtonText& ClearAnchor();

	ButtonText& Transform(ptgn::Transform transform = {});

	ButtonText& AutoBox(bool enabled = true);
	ButtonText& ClearAutoBox();

	ButtonText& Padding(ptgn::Padding padding);
	ButtonText& ClearPadding();

	ButtonText& Font(FontKey font);
	ButtonText& Color(ptgn::Color color);
	ButtonText& Colors(
		std::optional<ptgn::Color> idle, std::optional<ptgn::Color> hover = std::nullopt,
		std::optional<ptgn::Color> press = std::nullopt
	);
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
	ButtonText& Color(ptgn::Color color, ButtonVisualState state);

	StyledText& StyledTextForEdit(ButtonVisualState state);
	StyledText& StyledTextForEdit();
	void MarkTextDirty();

	ptgn::Button button_;
	ButtonVisualState state_{ ButtonVisualState::Idle };
};

class ButtonSprite {
public:
	ButtonSprite(ptgn::Button button, ButtonVisualState state);

	operator ptgn::Button() const; // NOSONAR

	ptgn::Button Button() const;

	ButtonSprite& Texture(TextureKey texture_key);
	ButtonSprite& Textures(
		std::optional<TextureKey> idle, std::optional<TextureKey> hover = std::nullopt,
		std::optional<TextureKey> press = std::nullopt
	);
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
	ptgn::Button button_;
	ButtonVisualState state_{ ButtonVisualState::Idle };

private:
	ButtonSprite& Texture(TextureKey texture_key, ButtonVisualState state);
};

class ButtonAnimation : public ButtonSprite {
public:
	ButtonAnimation(ptgn::Button button, ButtonVisualState state);

	ButtonAnimation& Texture(TextureKey texture_key);
	ButtonAnimation& Textures(
		std::optional<TextureKey> idle, std::optional<TextureKey> hover = std::nullopt,
		std::optional<TextureKey> press = std::nullopt
	);
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
	ButtonAnimation& Configs(
		std::optional<AnimationConfig> idle, std::optional<AnimationConfig> hover = std::nullopt,
		std::optional<AnimationConfig> press = std::nullopt
	);
	ButtonAnimation& StaticFrame(AnimationConfig config, std::size_t frame = 0);
	ButtonAnimation& ClearConfig();

	ButtonAnimation& Clear();

private:
	ButtonAnimation& Config(
		AnimationConfig config, ButtonAnimationOptions options, ButtonVisualState state
	);
};

namespace impl {

struct ButtonAnimationCompleteScript : public Script {
	ButtonAnimationCompleteScript() = default;
	explicit ButtonAnimationCompleteScript(Button button_entity);

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
Button CreateButton(Scene& scene, Transform transform = {}, Origin origin = Origin::Center);

Button CreateButton(
	Scene& scene, Transform transform, V2_float size, Origin origin = Origin::Center
);

Button CreateButton(
	Scene& scene, Transform transform, float radius, Origin origin = Origin::Center
);

} // namespace ptgn
