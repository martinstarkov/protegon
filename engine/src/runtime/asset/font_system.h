#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"

#ifdef CreateFont
#undef CreateFont
#endif

struct TTF_Font;

struct SDL_IOStream;

namespace ptgn {

inline constexpr float kDefaultFontSize{ 18.0f };
inline constexpr const char* kDefaultFontKey{ "" };

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

	/// @brief Empty font key corresponds to the engine default font.
	void SetDefault(std::string_view key = kDefaultFontKey);

	int GetLineSkip(std::string_view key, std::optional<float> font_size) const;

	/// @param Text to calculate size of, in UTF-8 encoding.
	/// @param font_size Optional font size to check the size for. If {}, uses the current font
	/// size.
	/// @param max_wrap_width The maximum width or 0 to wrap on newline characters.
	V2_int GetSize(
		std::string_view key, std::string_view content, std::optional<float> font_size = {},
		int max_wrap_width = 0
	) const;

	V2_int GetSize(
		Font font, std::string_view text_content, std::optional<float> font_size = {},
		int max_wrap_width = 0
	) const;

	/// @param font_size Optional font size to check the height for. If {}, uses the current font
	/// size.
	int GetHeight(std::string_view key, std::optional<float> font_size = {}) const;

private:
	friend class Shader;
	friend class Texture;
	friend class AssetManager;

	std::optional<impl::Surface> CreateTextSurface(
		std::string_view text_content, Color color, float font_size, Font font_asset,
		const TextProperties& properties, float hd_scale, bool hd
	) const;

	static std::shared_ptr<TTF_Font> CreateFont(const path& font_path, float pt_size);

	std::shared_ptr<TTF_Font> GetFont(std::string_view key, std::optional<float> font_size) const;

	AssetManager& assets_;

	std::size_t default_font_key_{ 0 };

	SDL_IOStream* raw_default_font_{ nullptr };
};

} // namespace ptgn