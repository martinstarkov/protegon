#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

namespace ptgn {

class AssetManager;

namespace impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

} // namespace impl

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

class Font {
private:
	friend class AssetManager;

	Font(const std::shared_ptr<TTF_Font>& font, float pt_size);

	std::shared_ptr<TTF_Font> font_;
	float pt_size_{ 0.0f };
};

enum class FontRenderMode : int {
	Blended = 0,
	Solid	= 1,
	Shaded	= 2
};

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

namespace impl {

// static constexpr std::int32_t default_font_size{ 18 };
// static constexpr std::int32_t default_font_index{ 0 };

// struct FontSize : public ArithmeticComponent<std::int32_t> {
//	using ArithmeticComponent::ArithmeticComponent;
//
//	FontSize() : ArithmeticComponent{ default_font_size } {}
//
//	[[nodiscard]] FontSize GetHD(const Scene& scene, const Camera& camera) const;
// };

// Empty font key corresponds to the engine default font.
// void SetDefaultFont(const ResourceHandle& key = {});

//[[nodiscard]] static SDL_RWops* GetRawBuffer(const FontBinary& binary);

// @param free_buffer If true, frees raw_buffer after use.
//[[nodiscard]] static TTF_Font* LoadFromBinary(
//	SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
//);

//[[nodiscard]] static Font LoadFromBinary(
//	const FontBinary& binary, std::int32_t size, std::int32_t index
//);

//[[nodiscard]] static Font LoadFromFile(
//	const path& filepath, std::int32_t size, std::int32_t index
//);

//[[nodiscard]] static Font LoadFromFile(const path& filepath);

//[[nodiscard]] TemporaryFont Get(const ResourceHandle& key, const FontSize& font_size = {}) const;

// ResourceHandle default_key_;

// SDL_RWops* raw_default_font_{ nullptr };

} // namespace impl

} // namespace ptgn