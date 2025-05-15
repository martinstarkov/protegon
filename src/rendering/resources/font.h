#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "components/generic.h"
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

private:
	friend class Game;
	friend class ptgn::Text;

	using Font = std::unique_ptr<TTF_Font, TTF_FontDeleter>;

	void Init();

	[[nodiscard]] V2_int GetSize(const FontKey& key, const std::string& content) const;

	// @return Total height of the font in pixels.
	[[nodiscard]] std::int32_t GetHeight(const FontKey& key) const;

	[[nodiscard]] bool Has(const FontKey& key) const;

	[[nodiscard]] static Font LoadFromBinary(
		const FontBinary& binary, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] static Font LoadFromFile(
		const path& filepath, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] TTF_Font* Get(const FontKey& key) const;

	std::unordered_map<std::size_t, Font> fonts_;

	FontKey default_key_;
};

} // namespace impl

} // namespace ptgn