#include "Font.h"

#include "managers/FontManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace font {

void Load(const char* font_key, const char* font_path, std::uint32_t font_point_size, std::uint32_t font_face_index) {
	auto& font_manager{ internal::GetFontManager() };
	font_manager.Load(math::Hash(font_key), new internal::Font{ font_path, font_point_size, font_face_index });
}

void Unload(const char* font_key) {
	auto& font_manager{ internal::GetFontManager() };
	font_manager.Unload(math::Hash(font_key));
}

std::int32_t GetHeight(const char* font_key) {
	auto& font_manager{ internal::GetFontManager() };
	assert(font_manager.Has(math::Hash(font_key)));
	internal::Font* font = font_manager.Get(math::Hash(font_key));
	return font->GetHeight();
}

} // namespace font

} // namespace ptgn