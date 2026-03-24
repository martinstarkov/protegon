#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"

#ifdef CreateFont
#undef CreateFont
#endif

struct TTF_Font;

struct SDL_IOStream;

namespace ptgn {

class AssetManager;

namespace impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

} // namespace impl

class FontSystem {
public:
	explicit FontSystem(AssetManager& assets);
	~FontSystem() noexcept;
	FontSystem(const FontSystem&)				 = delete;
	FontSystem& operator=(const FontSystem&)	 = delete;
	FontSystem(FontSystem&&) noexcept			 = delete;
	FontSystem& operator=(FontSystem&&) noexcept = delete;

	Font GetDefault() const;

	/// @param font Default ({}) key corresponds to the engine default font.
	void SetDefault(FontOrKey font = {});

	int GetLineSkip(FontOrKey font, FontSize font_size = {}) const;

	/// @param text_content Text to calculate size of, in UTF-8 encoding.
	/// @param font_size Optional font size to check the size for. If {}, uses the current font
	/// size.
	/// @param max_wrap_width The maximum width or 0 to wrap on newline characters.
	V2_int GetSize(
		FontOrKey font, std::string_view text_content, FontSize font_size = {},
		int max_wrap_width = 0
	) const;

	/// @param font_size Optional font size to check the height for. If {}, uses the current font
	/// size.
	int GetHeight(FontOrKey font, FontSize font_size = {}) const;

private:
	friend class Shader;
	friend class Texture;
	friend class AssetManager;

	[[nodiscard]] static std::optional<impl::Surface> CreateTextSurface(
		std::string_view text_content, Color color, FontSize font_size, Font font_asset,
		const TextProperties& properties, std::optional<float> hd_scale
	);

	[[nodiscard]] static std::shared_ptr<TTF_Font> CreateFont(
		const path& font_path, FontSize font_size
	);

	std::shared_ptr<TTF_Font> GetFont(FontOrKey font, FontSize font_size) const;

	AssetManager& assets_;

	FontOrKey default_font_;

	SDL_IOStream* raw_default_font_{ nullptr };
};

} // namespace ptgn