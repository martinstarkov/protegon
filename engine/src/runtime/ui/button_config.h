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
#include "core/util/time.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "serialization/serialize.h"

namespace ptgn {

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

struct ButtonShapePartConfig {
	ButtonPart part{ ButtonPart::Background };
	ButtonVisualState state{ ButtonVisualState::Base };

	std::optional<std::variant<Rect, Circle>> shape;
	Origin origin{ Origin::Center };

	std::optional<Color> color;
	std::optional<FillStyle> fill_style;

	PTGN_SERIALIZE(ButtonShapePartConfig, part, state, shape, origin, color, fill_style)
};

struct ButtonSpritePartConfig {
	ButtonVisualState state{ ButtonVisualState::Base };

	/// @brief Texture key to use for the sprite. Must be loaded in the AssetManager.
	std::string texture;
	Origin origin{ Origin::Center };
	Transform transform;
	std::optional<V2_float> size;
	std::optional<Color> tint;

	PTGN_SERIALIZE(ButtonSpritePartConfig, state, texture, origin, transform, size, tint)
};

struct ButtonTextPartConfig {
	ButtonVisualState state{ ButtonVisualState::Base };

	std::string content;
	/// @brief Font key to use for the text. Must be loaded in the AssetManager.
	std::string font{ kDefaultFont };
	float font_size{ kDefaultFontSize };
	Color color{ color::Black };

	Origin origin{ Origin::Center };
	Transform transform;

	/// @brief Optional initial TextBox. Further text behavior should use the Text API.
	std::optional<Rect> box;

	HorizontalAlign horizontal_align{ HorizontalAlign::Center };
	VerticalAlign vertical_align{ VerticalAlign::Center };
	WrapMode wrap_mode{ WrapMode::None };
	OverflowMode overflow_mode{ OverflowMode::Overflow };

	/// @brief If true, the button may update the text box from the button shape.
	bool auto_box{ true };

	/// @brief Padding used when auto_box is true.
	Rect auto_box_padding;

	PTGN_SERIALIZE(
		ButtonTextPartConfig, state, content, font, font_size, color, origin, transform, box,
		horizontal_align, vertical_align, wrap_mode, overflow_mode, auto_box, auto_box_padding
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
	/// @brief Interactive shape. Visual background/border should be child parts.
	std::variant<Rect, Circle> shape;

	Origin origin{ Origin::Center };
	bool ui_layer{ true };
	bool enabled{ true };

	std::vector<ButtonShapePartConfig> shapes;
	std::vector<ButtonSpritePartConfig> sprites;
	std::vector<ButtonTextPartConfig> texts;

	ButtonSoundConfig sounds;

	PTGN_SERIALIZE(ButtonDesc, shape, origin, ui_layer, enabled, shapes, sprites, texts, sounds)
};

/// @brief Convenience high-level config for simple buttons.
/// This should create child entities internally, not store visuals on the button entity.
struct ButtonConfig {
	std::optional<std::string> content;
	std::optional<Color> text_color{ color::White };
	std::optional<Color> text_color_hover;
	std::optional<Color> text_color_press;

	float font_size{ kDefaultFontSize };

	/// @brief Font key to use for the button text. Must be loaded in the AssetManager.
	std::string font{ kDefaultFont };

	/// @brief Horizontal alignment of button text.
	std::optional<HorizontalAlign> horizontal_align;
	/// @brief Vertical alignment of button text.
	std::optional<VerticalAlign> vertical_align;

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
};

struct AnimatedButtonConfig {
	std::string texture;
	std::string texture_hover;
	std::optional<std::string> texture_press;

	AnimationConfig animation_hover;
	std::optional<AnimationConfig> animation_press;

	std::optional<std::string> sound_hover;
	std::optional<std::string> sound_press;
};

} // namespace ptgn