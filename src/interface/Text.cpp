#include "Text.h"

#include "manager/TextManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace text {

void Load(const char* text_key,
		  const char* texture_key,
		  const char* font_key,
		  const char* text_content,
		  const Color& text_color) {
	auto& text_manager{ manager::Get<TextManager>() };
	text_manager.Load(math::Hash(text_key), texture_key, font_key, text_content, text_color);
}

void Unload(const char* text_key) {
	auto& text_manager{ manager::Get<TextManager>() };
	text_manager.Unload(math::Hash(text_key));
}

} // namespace text

} // namespace ptgn