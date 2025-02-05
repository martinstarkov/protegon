#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "math/vector2.h"
#include "resources/fonts.h"
#include "utility/file.h"

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

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

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

namespace impl {

class Game;

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

class FontManager {
public:
	void Load(
		std::string_view key, const path& filepath, std::int32_t size = 20, std::int32_t index = 0
	);

	void Load(
		std::string_view key, const FontBinary& binary, std::int32_t size = 20,
		std::int32_t index = 0
	);

	void Unload(std::string_view key);

	// Empty string corresponds to the engine default font.
	void SetDefault(std::string_view key = "");

private:
	friend class Game;
	friend class Text;

	using Font = std::unique_ptr<TTF_Font, TTF_FontDeleter>;

	void Init();

	[[nodiscard]] V2_int GetSize(std::size_t key, const std::string& content) const;

	// @return Total height of the font in pixels.
	[[nodiscard]] std::int32_t GetHeight(std::size_t key) const;

	[[nodiscard]] bool Has(std::size_t key) const;

	[[nodiscard]] static Font LoadFromBinary(
		const FontBinary& binary, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] static Font LoadFromFile(
		const path& filepath, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] TTF_Font* Get(std::size_t key) const;

	std::unordered_map<std::size_t, Font> fonts_;

	std::size_t default_key_;
};

} // namespace impl

} // namespace ptgn