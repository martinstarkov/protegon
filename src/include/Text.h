#pragma once

#include "renderer/Colors.h"

namespace ptgn {

namespace text {

// Load text into text manager. Font key must exist in the font manager
void Load(const char* text_key, const char* font_key, const char* text_content, const Color& text_color = colors::BLACK);

// Unload text from text manager.
void Unload(const char* text_key);

// Set text content.
void SetContent(const char* text_key, const char* new_text_content);

// Set text color.
void SetColor(const char* text_key, const Color& new_text_color);

// Set text font to a font that has been loaded into the font manager.
void SetFont(const char* text_key, const char* new_font_key);

} // namespace text

} // namespace ptgn