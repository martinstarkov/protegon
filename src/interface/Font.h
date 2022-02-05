#pragma once

#include <cstdint> // std::uint32_t, std::int32_t

namespace ptgn {

namespace font {

/* Load a font into the font manager.
* @param font_point_size Point size (based on 72 DPI). This translates to pixel height.
* @param font_face_index Font face index, the first face is 0.
*/
void Load(const char* font_key, const char* font_path, std::uint32_t font_point_size, std::uint32_t font_face_index = 0);

// Unload a font from the font manager.
void Unload(const char* font_key);

// Retrieve the pixel height of a font.
std::int32_t GetHeight(const char* font_key); 

} // namespace font

} // namespace ptgn