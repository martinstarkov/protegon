#pragma once

#include "renderer/Color.h"

namespace engine {

namespace colors {

inline const Color BLACK{ 0, 0, 0, 255 };
inline const Color WHITE{ 255, 255, 255, 255 };
inline const Color DEFAULT_DRAW_COLOR{ BLACK };
inline const Color DEFAULT_BACKGROUND_COLOR{ WHITE };
inline const Color TRANSPARENT{ 0, 0, 0, 0 };
inline const Color COLORLESS{ TRANSPARENT };
inline const Color RED{ 255, 0, 0, 255 };
inline const Color DARK_RED{ 128, 0, 0, 255 };
inline const Color ORANGE{ 255, 165, 0, 255 };
inline const Color YELLOW{ 255, 255, 0, 255 };
inline const Color GOLD{ 255, 215, 0, 255 };
inline const Color GREEN{ 0, 128, 0, 255 };
inline const Color LIME{ 0, 255, 0, 255 };
inline const Color DARK_GREEN{ 0, 100, 0, 255 };
inline const Color BLUE{ 0, 0, 255, 255 };
inline const Color DARK_BLUE{ 0, 0, 128, 255 };
inline const Color CYAN{ 0, 255, 255, 255 };
inline const Color TEAL{ 0, 128, 128, 255 };
inline const Color MAGENTA{ 255, 0, 255, 255 };
inline const Color PURPLE{ 128, 0, 128, 255 };
inline const Color PINK{ 255, 192, 203, 255 };
inline const Color GREY{ 128, 128, 128, 255 };
inline const Color SILVER{ 192, 192, 192, 255 };

} // namespace colors

} // namespace engine