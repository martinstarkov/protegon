#include "Text.h"

#include "managers/TextManager.h"
#include "managers/WindowManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace text {

void Load(const char* text_key, const char* font_key, const char* text_content, const Color& text_color, internal::managers::id window = 0) {
	auto& text_manager{ internal::GetTextManager() };
	auto& window_manager{ internal::GetWindowManager() };
	assert(window_manager.Has(window) && "Cannot load texture into non-existent window");
	internal::Window* window_instance = window_manager.Get(window);
	text_manager.Load(math::Hash(text_key), new internal::Text{ window_instance->GetRenderer(), math::Hash(font_key), text_content, text_color });
}

void Unload(const char* text_key) {
	auto& text_manager{ internal::GetTextManager() };
	text_manager.Unload(math::Hash(text_key));
}

void SetContent(const char* text_key, const char* new_text_content) {
	auto& text_manager{ internal::GetTextManager() };
	assert(text_manager.Has(math::Hash(text_key)));
	internal::Text* text = text_manager.Get(math::Hash(text_key));
	text->SetContent(new_text_content);
}

void SetColor(const char* text_key, const Color& new_text_color) {
	auto& text_manager{ internal::GetTextManager() };
	assert(text_manager.Has(math::Hash(text_key)));
	internal::Text* text = text_manager.Get(math::Hash(text_key));
	text->SetColor(new_text_color);

}

void SetFont(const char* text_key, const char* new_font_key) {
	auto& text_manager{ internal::GetTextManager() };
	assert(text_manager.Has(math::Hash(text_key)));
	internal::Text* text = text_manager.Get(math::Hash(text_key));
	text->SetFont(math::Hash(new_font_key));
}

} // namespace text

} // namespace ptgn