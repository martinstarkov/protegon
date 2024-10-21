#pragma once

#include <cstdint>
#include <variant>

#include "core/manager.h"
#include "utility/file.h"
#include "utility/handle.h"

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Text;
class Surface;

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

enum class FontRenderMode : int {
	Solid	= 0,
	Shaded	= 1,
	Blended = 2
};

namespace impl {

class Renderer;
class Game;
class FontManager;
struct FontBinary;

} // namespace impl

class Font : public Handle<TTF_Font> {
public:
	Font() = default;
	Font(const path& font_path, std::int32_t point_size, std::int32_t index = 0);

	explicit Font(const impl::FontBinary& binary, std::int32_t point_size = 20);

	[[nodiscard]] std::int32_t GetHeight() const;

private:
	friend class Surface;
	friend class FontManager;
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

namespace impl {

class FontManager : public MapManager<Font> {
public:
	using FontOrKey = std::variant<Font, Key, InternalKey>;

	using MapManager::MapManager;

	void SetDefault(const FontOrKey& font);
	[[nodiscard]] Font GetDefault() const;

private:
	friend class Game;
	friend class Text;
	friend class Renderer;

	void Init();

	[[nodiscard]] Font GetFontOrKey(const FontOrKey& font) const;

	Font default_font_;
};

} // namespace impl

using FontOrKey = impl::FontManager::FontOrKey;

} // namespace ptgn