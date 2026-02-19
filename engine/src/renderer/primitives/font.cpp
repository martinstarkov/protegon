#include "renderer/primitives/font.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "ecs/ecs.h"

namespace ptgn {

namespace impl {

void TTF_FontDeleter::operator()(TTF_Font* font) const {
	TTF_CloseFont(font);
}

} // namespace impl

std::shared_ptr<TTF_Font> Font::Get(std::optional<float> font_size) const {
	return entity_.Get<impl::AssetManagerPtr>().ptr->Get(*this, font_size);
}

int Font::GetLineSkip(std::optional<float> font_size) const {
	return TTF_GetFontLineSkip(Get(font_size).get());
}

int Font::GetHeight(std::optional<float> font_size) const {
	return TTF_GetFontHeight(Get(font_size).get());
}

V2_int Font::GetSize(const std::string& content, std::optional<float> font_size, int max_wrap_width)
	const {
	V2_int size;

	if (content.empty()) {
		size.x = 0;
		size.y = GetHeight(font_size);
		return size;
	}

	auto success{ TTF_GetStringSizeWrapped(
		Get(font_size).get(), content.c_str(), 0, max_wrap_width, &size.x, &size.y
	) };

	PTGN_ASSERT(success, "Failed to get size of wrapped font string");

	return size;
}

} // namespace ptgn

// FontSize FontSize::GetHD(const Scene& scene, const Camera& camera) const {
//		FontSize final_font_size{ *this };
//		auto render_target_scale{ scene.GetRenderTargetScaleRelativeTo(camera) };
//		final_font_size =
//		static_cast<std::int32_t>(static_cast<float>(final_font_size) * render_target_scale.y);
//		return final_font_size;
//}