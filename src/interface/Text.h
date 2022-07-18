#pragma once

#include "renderer/Colors.h"

namespace ptgn {

namespace text {

void Load(const char* text_key,
		  const char* texture_key,
		  const char* font_key,
		  const char* text_content,
		  const Color& text_color = color::BLACK);

void Unload(const char* text_key);

} // namespace text

} // namespace ptgn