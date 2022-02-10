#include "Text.h"

#include "text/TextManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace text {

void Load(const char* text_key, const char* font_key, const char* text_content, const Color& text_color) {
	auto& text_manager{ services::GetTextManager() };
	text_manager.LoadText(math::Hash(text_key), math::Hash(font_key), text_content, text_color);
}

void Unload(const char* text_key) {
	auto& text_manager{ services::GetTextManager() };
	text_manager.UnloadText(math::Hash(text_key));
}

void SetContent(const char* text_key, const char* new_text_content) {
	auto& text_manager{ services::GetTextManager() };
	text_manager.SetTextContent(math::Hash(text_key), new_text_content);
}

void SetColor(const char* text_key, const Color& new_text_color) {
	auto& text_manager{ services::GetTextManager() };
	text_manager.SetTextColor(math::Hash(text_key), new_text_color);

}

void SetFont(const char* text_key, const char* new_font_key) {
	auto& text_manager{ services::GetTextManager() };
	text_manager.SetTextFont(math::Hash(text_key), math::Hash(new_font_key));
}

} // namespace text

} // namespace ptgn