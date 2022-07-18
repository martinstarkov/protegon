#include "Font.h"

#include "manager/FontManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace font {

void Load(const char* font_key,
		  const char* font_path,
		  std::uint32_t font_point_size,
		  std::uint32_t font_index) {
	auto& font_manager{ manager::Get<FontManager>() };
	font_manager.Load(math::Hash(font_key), font_path, font_point_size, font_index);
}

void Unload(const char* font_key) {
	auto& font_manager{ manager::Get<FontManager>() };
	font_manager.Unload(math::Hash(font_key));
}

} // namespace font

} // namespace ptgn