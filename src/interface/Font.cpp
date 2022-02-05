#include "Font.h"

#include "text/FontManager.h"
#include "math/Math.h"

namespace ptgn {

namespace font {

void Load(const char* font_key, const char* font_path, std::uint32_t font_point_size, std::uint32_t font_face_index) {
	auto& font_manager{ services::GetFontManager() };
	font_manager.LoadFont(math::Hash(font_key), font_path, font_point_size, font_face_index);
}

void Unload(const char* font_key) {
	auto& font_manager{ services::GetFontManager() };
	font_manager.UnloadFont(math::Hash(font_key));
}

std::int32_t GetHeight(const char* font_key) {
	auto& font_manager{ services::GetFontManager() };
	return font_manager.GetFontHeight(math::Hash(font_key));
}

} // namespace font

} // namespace ptgn