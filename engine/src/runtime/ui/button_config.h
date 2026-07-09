#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <string>
#include <variant>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/easing.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/time.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "serialization/serialize.h"

namespace ptgn {

struct Padding {
	float left{ 0.0f };
	float top{ 0.0f };
	float right{ 0.0f };
	float bottom{ 0.0f };

	constexpr Padding() = default;

	/// @brief Constructs a uniform padding on all sides.
	template <Arithmetic T>
	constexpr Padding(T amount) : // NOSONAR
		Padding{ static_cast<float>(amount), static_cast<float>(amount), static_cast<float>(amount),
				 static_cast<float>(amount) } {}

	template <Arithmetic TX, Arithmetic TY>
	constexpr Padding(TX horizontal, TY vertical) :
		Padding{ static_cast<float>(horizontal), static_cast<float>(vertical),
				 static_cast<float>(horizontal), static_cast<float>(vertical) } {}

	constexpr Padding(V2_float amount) : // NOSONAR
		Padding{ amount.x, amount.y, amount.x, amount.y } {}

	template <Arithmetic TL, Arithmetic TT, Arithmetic TR, Arithmetic TB>
	constexpr Padding(TL left, TT top, TR right, TB bottom) :
		left{ static_cast<float>(left) },
		top{ static_cast<float>(top) },
		right{ static_cast<float>(right) },
		bottom{ static_cast<float>(bottom) } {}

	constexpr Padding(V2_float left_top, V2_float right_bottom) :
		Padding{ left_top.x, left_top.y, right_bottom.x, right_bottom.y } {}

	constexpr V2_float GetLeftTop() const {
		return { left, top };
	}

	constexpr V2_float GetRightBottom() const {
		return { right, bottom };
	}

	constexpr bool operator==(const Padding& o) const {
		return left == o.left && right == o.right && top == o.top && bottom == o.bottom;
	}

	PTGN_SERIALIZE(Padding, left, top, right, bottom)
};

enum class ButtonState : std::uint8_t {
	Idle,
	Hover,
	Press,
};
PTGN_SERIALIZE_ENUM(ButtonState);

enum class ButtonVisualState : std::uint8_t {
	Idle,
	Hover,
	Press,

	Disabled,
	DisabledHover,
	DisabledPress,

	Toggled,
	ToggledHover,
	ToggledPress, // Must be the last state
};
PTGN_SERIALIZE_ENUM(ButtonVisualState);

inline constexpr std::size_t kButtonVisualStateCount{
	std::to_underlying(ButtonVisualState::ToggledPress) + 1
};

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

	/// @brief If true, repeated presses are ignored while this visual state is locked.
	bool block_press{ false };

	constexpr bool operator==(const ButtonAnimationOptions&) const = default;

	PTGN_SERIALIZE(ButtonAnimationOptions, playback, static_frame, block_press)
};

struct ButtonShapeVisual {
	bool defined{ false };

	/// @brief Optional fixed size for the shape. If not set, the shape uses the button size.
	std::optional<std::variant<V2_float, float>> size;

	/// @brief Point of the child shape that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the child transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	std::optional<Color> color;

	/// @brief Only applicable for borders.
	std::optional<FillStyle> fill_style;

	PTGN_SERIALIZE(ButtonShapeVisual, defined, size, origin, anchor, transform, color, fill_style)
};

struct ButtonShapeVisuals {
	std::array<ButtonShapeVisual, kButtonVisualStateCount> states;

	PTGN_SERIALIZE_VALUE(ButtonShapeVisuals, states)
};

struct ButtonBackgroundVisuals : ButtonShapeVisuals {
	PTGN_SERIALIZE_VALUE(ButtonBackgroundVisuals, states)
};

struct ButtonBorderVisuals : ButtonShapeVisuals {
	PTGN_SERIALIZE_VALUE(ButtonBorderVisuals, states)
};

struct ButtonTextVisual {
	bool defined{ false };

	std::optional<StyledText> styled_text;

	/// @brief Optional text box. If auto_box is true, the rect may be overwritten from button size.
	std::optional<TextBox> box;

	/// @brief Point of the text box that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the text transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	/// @brief If true, the button may update the text box from the button size.
	std::optional<bool> auto_box;

	/// @brief Padding used when auto_box resolves to true.
	std::optional<Padding> padding;

	PTGN_SERIALIZE(
		ButtonTextVisual, defined, styled_text, box, origin, anchor, transform, auto_box, padding
	)
};

struct ButtonTextVisuals {
	std::array<ButtonTextVisual, kButtonVisualStateCount> states;

	PTGN_SERIALIZE_VALUE(ButtonTextVisuals, states)
};

struct ButtonSpriteVisual {
	bool defined{ false };

	/// @brief Texture key to use for the sprite. Must be loaded in the AssetManager.
	std::optional<std::string> texture;

	/// @brief Point of the sprite that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the sprite transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	/// @brief Optional fixed display size for the sprite. If not set, the sprite uses texture size.
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

	PTGN_SERIALIZE_VALUE(ButtonSpriteVisuals, states)
};

struct ButtonSounds {
	/// @brief Sound keys per visual state. Each sound must be loaded in the AssetManager.
	std::array<std::optional<std::string>, kButtonVisualStateCount> states;

	/// @brief If true when one sound plays the others are stopped.
	bool exclusive{ false };

	PTGN_SERIALIZE(ButtonSounds, states, exclusive)
};

struct MoveButtonConfig {
	V2_float offset{ 20.0f, 0.0f };
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

struct ButtonDesc {
	/// @brief Interactive shape size.
	std::optional<std::variant<V2_float, float>> size;

	/// @brief Origin of the button interactive shape.
	Origin origin{ Origin::Center };

	bool ui_layer{ true };
	bool enabled{ true };

	ButtonBackgroundVisuals background;
	ButtonBorderVisuals border;
	ButtonTextVisuals text;
	ButtonSpriteVisuals sprite;
	ButtonSounds sounds;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;

	PTGN_SERIALIZE(
		ButtonDesc, size, origin, ui_layer, enabled, background, border, text, sprite, sounds, move,
		scale
	)
};

struct ButtonShapeConfig {
	/// @brief Optional fixed size for the shape. If not set, the shape uses the button size.
	std::optional<std::variant<V2_float, float>> size;

	/// @brief Point of the child shape that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the child transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	std::optional<Color> color;
	std::optional<Color> color_hover;
	std::optional<Color> color_press;

	std::optional<FillStyle> fill_style;

	PTGN_SERIALIZE(
		ButtonShapeConfig, size, origin, anchor, transform, color, color_hover, color_press,
		fill_style
	)
};

struct ButtonTextConfig {
	std::optional<std::string> content;

	std::optional<Color> color{ color::White };
	std::optional<Color> color_hover;
	std::optional<Color> color_press;

	float font_size{ kDefaultFontSize };

	/// @brief Font key to use for the button text. Must be loaded in the AssetManager.
	std::string font{ kDefaultFont };

	TextBox box;

	/// @brief Point of the text box that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the text transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	std::optional<float> outline_width;
	Color outline_color{ color::Black };

	/// @brief If true, the button may update the text box from the button size.
	bool auto_box{ true };

	/// @brief Padding used when auto_box is true.
	Padding padding;

	PTGN_SERIALIZE(
		ButtonTextConfig, content, color, color_hover, color_press, font_size, font, box, origin,
		anchor, transform, outline_width, outline_color, auto_box, padding
	)
};

struct ButtonSpriteConfig {
	std::optional<std::string> texture;
	std::optional<std::string> texture_hover;
	std::optional<std::string> texture_press;

	std::optional<Color> tint;
	std::optional<Color> tint_hover;
	std::optional<Color> tint_press;

	/// @brief Point of the sprite that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the sprite transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	std::optional<Transform> transform;

	/// @brief Optional fixed display size for the sprite. If not set, the sprite uses texture size.
	std::optional<V2_float> size;

	PTGN_SERIALIZE(
		ButtonSpriteConfig, texture, texture_hover, texture_press, tint, tint_hover, tint_press,
		origin, anchor, transform, size
	)
};

struct ButtonSoundConfig {
	std::optional<std::string> idle;
	std::optional<std::string> hover;
	std::optional<std::string> press;

	std::optional<std::string> disabled;
	std::optional<std::string> disabled_hover;
	std::optional<std::string> disabled_press;

	std::optional<std::string> toggled;
	std::optional<std::string> toggled_hover;
	std::optional<std::string> toggled_press;

	PTGN_SERIALIZE(
		ButtonSoundConfig, idle, hover, press, disabled, disabled_hover, disabled_press, toggled,
		toggled_hover, toggled_press
	)
};

/// @brief Convenience high level config for simple buttons. This should be converted into
/// ButtonDesc.
struct ButtonConfig {
	/// @brief Interactive shape size.
	std::optional<std::variant<V2_float, float>> size;

	/// @brief Origin of the button interactive shape.
	Origin origin{ Origin::Center };

	bool ui_layer{ true };
	bool enabled{ true };

	ButtonShapeConfig background;
	ButtonShapeConfig border;
	ButtonTextConfig text;
	ButtonSpriteConfig sprite;
	ButtonSoundConfig sounds;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;

	PTGN_SERIALIZE(
		ButtonConfig, size, origin, ui_layer, enabled, background, border, text, sprite, sounds,
		move, scale
	)
};

/// @brief Convenience high level config for animated sprite buttons. This should be converted into
/// ButtonDesc.
struct AnimatedButtonConfig {
	/// @brief If set, the button uses this size. Otherwise it may use the idle texture or animation
	/// size.
	std::optional<V2_float> size;

	/// @brief Origin of the button interactive shape.
	Origin origin{ Origin::Center };

	bool ui_layer{ true };
	bool enabled{ true };

	ButtonSpriteConfig sprite;

	std::optional<AnimationConfig> animation;
	std::optional<AnimationConfig> animation_hover;
	std::optional<AnimationConfig> animation_press;

	ButtonAnimationOptions animation_options;
	ButtonAnimationOptions animation_options_hover;
	ButtonAnimationOptions animation_options_press{
		.playback	  = ButtonAnimationPlayback::PlayOnce,
		.static_frame = 0,
		.block_press  = false,
	};

	ButtonSoundConfig sounds;

	PTGN_SERIALIZE(
		AnimatedButtonConfig, size, origin, ui_layer, enabled, sprite, animation, animation_hover,
		animation_press, animation_options, animation_options_hover, animation_options_press, sounds
	)
};

} // namespace ptgn