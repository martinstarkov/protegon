#pragma once

#include <cstdint>
#include <variant>

#include "core/manager.h"
#include "protegon/file.h"
#include "utility/handle.h"

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Surface;
class Game;

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

class Font : public Handle<TTF_Font> {
public:
	Font() = default;
	Font(const path& font_path, std::int32_t point_size, std::int32_t index = 0);

	[[nodiscard]] std::int32_t GetHeight() const;

private:
	friend class Surface;
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
	using MapManager::MapManager;
};

} // namespace impl

using FontOrKey = std::variant<Font, impl::FontManager::Key, impl::FontManager::InternalKey>;

} // namespace ptgn