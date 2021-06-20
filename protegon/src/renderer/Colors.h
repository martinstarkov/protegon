#pragma once

#include "renderer/Color.h"

namespace ptgn {

namespace colors {

inline constexpr Color BLACK{ 0, 0, 0, 255 };
inline constexpr Color WHITE{ 255, 255, 255, 255 };
inline constexpr Color DEFAULT_DRAW_COLOR{ BLACK };
inline constexpr Color DEFAULT_BACKGROUND_COLOR{ WHITE };
inline constexpr Color TRANSPARENT{ 0, 0, 0, 0 };
inline constexpr Color COLORLESS{ TRANSPARENT };
inline constexpr Color RED{ 255, 0, 0, 255 };
inline constexpr Color DARK_RED{ 128, 0, 0, 255 };
inline constexpr Color ORANGE{ 255, 165, 0, 255 };
inline constexpr Color YELLOW{ 255, 255, 0, 255 };
inline constexpr Color GOLD{ 255, 215, 0, 255 };
inline constexpr Color GREEN{ 0, 128, 0, 255 };
inline constexpr Color LIME{ 0, 255, 0, 255 };
inline constexpr Color DARK_GREEN{ 0, 100, 0, 255 };
inline constexpr Color BLUE{ 0, 0, 255, 255 };
inline constexpr Color DARK_BLUE{ 0, 0, 128, 255 };
inline constexpr Color CYAN{ 0, 255, 255, 255 };
inline constexpr Color TEAL{ 0, 128, 128, 255 };
inline constexpr Color MAGENTA{ 255, 0, 255, 255 };
inline constexpr Color PURPLE{ 128, 0, 128, 255 };
inline constexpr Color PINK{ 255, 192, 203, 255 };
inline constexpr Color GREY{ 128, 128, 128, 255 };
inline constexpr Color LIGHT_GREY{ 83, 83, 83, 255 };
inline constexpr Color SILVER{ 192, 192, 192, 255 };

} // namespace colors

} // namespace ptgn