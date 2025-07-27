#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "components/generic.h"
#include "math/vector2.h"
#include "resources/fonts.h"
#include "serialization/enum.h"
#include "utility/file.h"

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif
struct SDL_RWops;

namespace ptgn {

class Text;

enum class FontRenderMode : int {
	Solid	= 0,
	Shaded	= 1,
	Blended = 2
};

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

struct FontSize : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	FontSize() : ArithmeticComponent{ std::numeric_limits<std::int32_t>::infinity() } {}
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

struct FontKey : public HashComponent {
	using HashComponent::HashComponent;
};

namespace impl {

class Game;

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

class FontManager {
public:
	void Load(
		const FontKey& key, const path& filepath, std::int32_t size = 20, std::int32_t index = 0
	);

	void Load(
		const FontKey& key, const FontBinary& binary, std::int32_t size = 20, std::int32_t index = 0
	);

	void Unload(const FontKey& key);

	// Empty font key corresponds to the engine default font.
	void SetDefault(const FontKey& key = {});

	[[nodiscard]] int GetLineSkip(const FontKey& key, const FontSize& font_size = {}) const;

	// @param font_size If left with default {}, will use currently set font size of the provided
	// font key.
	[[nodiscard]] V2_int GetSize(
		const FontKey& key, const std::string& content, const FontSize& font_size = {}
	) const;

	// @return Total height of the font in pixels.
	[[nodiscard]] std::int32_t GetHeight(const FontKey& key, const FontSize& font_size = {}) const;

private:
	friend class Game;
	friend class ptgn::Text;

	using Font = std::unique_ptr<TTF_Font, TTF_FontDeleter>;

	void Init();

	[[nodiscard]] bool Has(const FontKey& key) const;

	[[nodiscard]] static SDL_RWops* GetRawBuffer(const FontBinary& binary);

	// @param free_buffer If true, frees raw_buffer after use.
	[[nodiscard]] static TTF_Font* LoadFromBinary(
		SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
	);

	[[nodiscard]] static Font LoadFromBinary(
		const FontBinary& binary, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] static Font LoadFromFile(
		const path& filepath, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] std::shared_ptr<TTF_Font> Get(
		const FontKey& key, const FontSize& font_size = {}
	) const;

	std::unordered_map<std::size_t, std::string> font_paths_;
	std::unordered_map<std::size_t, Font> fonts_;

	FontKey default_key_;

	SDL_RWops* raw_default_font_{ nullptr };
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	FontRenderMode, { { FontRenderMode::Solid, "solid" },
					  { FontRenderMode::Shaded, "shaded" },
					  { FontRenderMode::Blended, "blended" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	FontStyle, { { FontStyle::Normal, "normal" },
				 { FontStyle::Bold, "bold" },
				 { FontStyle::Italic, "italic" },
				 { FontStyle::Underline, "underline" },
				 { FontStyle::Strikethrough, "strikethrough" } }
);

} // namespace ptgn