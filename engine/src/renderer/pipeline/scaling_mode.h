#pragma once

#include "serialization/serialize.h"

namespace ptgn {

/// @brief How the game size is scaled to the window size, resulting in display size
enum class ScalingMode {
	/// @brief There is no scaling in effect
	Disabled,
	/// @brief The rendered content is stretched to the window size
	Stretch,
	/// @brief The rendered content is fit to the largest dimension and the other dimension is
	/// letterboxed with black bars
	Letterbox,
	/// @brief The rendered content is fit to the smallest dimension and the other dimension extends
	/// beyond the window bounds
	Overscan,
	/// @brief The rendered content is scaled up by integer multiples to fit the window size
	IntegerScale,
};
PTGN_REFLECT_ENUM(ScalingMode);

/// @brief Determines which resolution the render target automatically resizes to when the game or
/// display is resized.
enum class ResizeType {
	/// @brief Resizes to the current display size.
	Display,
	/// @brief Resizes to the current game size.
	Game
};
PTGN_REFLECT_ENUM(ResizeType);

} // namespace ptgn