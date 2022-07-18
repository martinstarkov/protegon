#pragma once

#include <cstdint> // std::uint32_t

namespace ptgn {

namespace font {

/*
* @param font_key Unique identifier for the font.
* @param font_path Path to font file.
* @param font_point_size Point size (based on 72 DPI). This translates to pixel height.
* @param font_index Font face index, the first face is 0.
*/
void Load(const char* font_key,
		  const char* font_path,
		  std::uint32_t font_point_size,
		  std::uint32_t font_index = 0);

void Unload(const char* font_key);

} // namespace font

} // namespace ptgn