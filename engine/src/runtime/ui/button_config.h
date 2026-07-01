#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/easing.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
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
	Base,

	Idle,
	Hover,
	Press,

	Disabled,
	DisabledHover,
	DisabledPress,

	Toggled,
	ToggledHover,
	ToggledPress,
};
PTGN_SERIALIZE_ENUM(ButtonVisualState);

enum class ButtonPart : std::uint8_t {
	Background,
	Border,
	Sprite,
	Text
};
PTGN_SERIALIZE_ENUM(ButtonPart);

struct ButtonShapeConfig {
	/// @brief Can only be ButtonPart::Background or ButtonPart::Border.
	ButtonPart part{ ButtonPart::Background };
	ButtonVisualState state{ ButtonVisualState::Base };

	/// @brief Optional fixed size for the shape. If not set, the shape uses the button size.
	std::optional<std::variant<V2_float, float>> size;

	/// @brief Point of the child shape that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the child transform is placed.
	std::optional<Origin> anchor;

	std::optional<Color> color;

	/// @brief Only applicable for ButtonPart::Border.
	std::optional<FillStyle> fill_style;

	PTGN_SERIALIZE(ButtonShapeConfig, part, state, size, origin, anchor, color, fill_style)
};

struct ButtonSpriteConfig {
	ButtonVisualState state{ ButtonVisualState::Base };

	/// @brief Texture key to use for the sprite. Must be loaded in the AssetManager.
	std::string texture;

	/// @brief Point of the sprite that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the sprite transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	Transform transform;

	/// @brief Optional fixed size for the sprite. If not set, the sprite uses the texture size.
	std::optional<V2_float> size;
	std::optional<Color> tint;

	PTGN_SERIALIZE(ButtonSpriteConfig, state, texture, origin, anchor, transform, size, tint)
};

struct ButtonTextConfig {
	ButtonVisualState state{ ButtonVisualState::Base };

	std::string content;
	/// @brief Font key to use for the text. Must be loaded in the AssetManager.
	std::string font{ kDefaultFont };
	float font_size{ kDefaultFontSize };
	Color color{ color::Black };

	/// @brief Optional text box.
	TextBox box;

	/// @brief Point of the text box that lies at its transform.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the text transform is placed.
	std::optional<Origin> anchor;

	/// @brief Transform relative to the selected button anchor.
	Transform transform;

	std::optional<float> outline_width;
	Color outline_color{ color::Black };

	/// @brief If true, the button may update the text box from the button size.
	bool auto_box{ true };

	/// @brief Padding used when auto_box is true.
	Padding padding;

	PTGN_SERIALIZE(
		ButtonTextConfig, state, content, font, font_size, color, origin, anchor, transform, box,
		outline_width, outline_color, auto_box, padding
	)
};

struct ButtonSoundConfig {
	/// @brief Sound key to use for the button sounds. Must be loaded in the AssetManager.
	std::optional<std::string> idle;
	/// @brief Sound key to use for the button sounds. Must be loaded in the AssetManager.
	std::optional<std::string> hover;
	/// @brief Sound key to use for the button sounds. Must be loaded in the AssetManager.
	std::optional<std::string> press;

	PTGN_SERIALIZE(ButtonSoundConfig, idle, hover, press)
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

struct ButtonDesc {
	/// @brief Interactive shape size.
	std::variant<V2_float, float> size;

	/// @brief Origin of the button interactive shape.
	Origin origin{ Origin::Center };
	bool ui_layer{ true };
	bool enabled{ true };

	std::vector<ButtonShapeConfig> shapes;
	std::vector<ButtonSpriteConfig> sprites;
	std::vector<ButtonTextConfig> texts;

	ButtonSoundConfig sounds;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;

	PTGN_SERIALIZE(
		ButtonDesc, size, origin, ui_layer, enabled, shapes, sprites, texts, sounds, move, scale
	)
};

/// @brief Convenience high level config for simple buttons.
struct ButtonConfig {
	/// @brief Origin of the button interactive shape.
	Origin origin{ Origin::Center };

	std::optional<std::string> content;
	std::optional<Color> text_color{ color::White };
	std::optional<Color> text_color_hover;
	std::optional<Color> text_color_press;

	float font_size{ kDefaultFontSize };

	/// @brief Font key to use for the button text. Must be loaded in the AssetManager.
	std::string font{ kDefaultFont };

	TextBox text_box;

	/// @brief Optional point of the text box placed at the text transform.
	/// When absent, the resolved text anchor is used.
	std::optional<Origin> text_origin;

	/// @brief Point of the padded button content rectangle to which the text
	/// transform is anchored. When absent, defaults to Center.
	std::optional<Origin> text_anchor;

	/// @brief Transform relative to the resolved padded text anchor.
	Transform text_transform;

	std::optional<float> text_outline_width;
	Color text_outline_color{ color::Black };

	/// @brief If true, the button may update the text box from the button size.
	bool text_auto_box{ true };

	/// @brief Padding used when auto_box is true.
	Padding text_padding;

	std::optional<std::string> texture;
	std::optional<std::string> texture_hover;
	std::optional<std::string> texture_press;

	std::optional<Color> texture_tint;
	std::optional<Color> texture_tint_hover;
	std::optional<Color> texture_tint_press;

	std::optional<Color> background_color;
	std::optional<Color> background_color_hover;
	std::optional<Color> background_color_press;

	std::optional<V2_float> background_size;

	std::optional<std::string> sound_hover;
	std::optional<std::string> sound_press;

	std::optional<MoveButtonConfig> move;
	std::optional<ScaleButtonConfig> scale;

	PTGN_SERIALIZE(
		ButtonConfig, origin, content, text_color, text_color_hover, text_color_press, font_size,
		font, text_box, text_origin, text_anchor, text_transform, text_outline_width,
		text_outline_color, text_auto_box, text_padding, texture, texture_hover, texture_press,
		texture_tint, texture_tint_hover, texture_tint_press, background_color,
		background_color_hover, background_color_press, background_size, sound_hover, sound_press,
		move, scale
	)
};

struct AnimatedButtonConfig {
	/// @brief If set, the button uses this size. Otherwise it uses the idle texture size.
	std::optional<V2_float> size;

	/// @brief Origin of the animation sprite itself.
	std::optional<Origin> origin;

	/// @brief Point of the button shape at which the animation sprite is placed.
	std::optional<Origin> anchor;

	std::string texture;
	std::optional<std::string> texture_hover;
	std::optional<std::string> texture_press;

	std::optional<AnimationConfig> animation_hover;
	std::optional<AnimationConfig> animation_press;

	std::optional<std::string> sound_hover;
	std::optional<std::string> sound_press;

	PTGN_SERIALIZE(
		AnimatedButtonConfig, size, origin, anchor, texture, texture_hover, texture_press,
		animation_hover, animation_press, sound_hover, sound_press
	)
};

} // namespace ptgn
