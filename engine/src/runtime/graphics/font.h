#pragma once

#include <ostream>
#include <utility>

#include "core/util/entity_handle.h"
#include "runtime/ecs/component.h"

namespace ptgn {

namespace impl {

struct FontSize : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

} // namespace impl

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
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

class Font : public EntityHandle {
public:
	using EntityHandle::EntityHandle;
};

std::ostream& operator<<(std::ostream& o, const Font& f);

} // namespace ptgn