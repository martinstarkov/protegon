#pragma once

#include <ostream>
#include <utility>

#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "runtime/ecs/component.h"
#include "serialization/serialize.h"

namespace ptgn {

inline constexpr float kDefaultFontSize{ 18.0f };

/// @brief Defaults to default engine font size.
struct FontSize : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	FontSize() : ArithmeticComponent{ kDefaultFontSize } {}
};

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
PTGN_REFLECT_ENUM(FontRenderMode);

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};
PTGN_SERIALIZE_ENUM(FontStyle);
std::ostream& operator<<(std::ostream& os, FontStyle style);

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

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Font> {
	std::size_t operator()(const ptgn::Font& font) const {
		return std::hash<ecs::Entity>()(font.GetEntity());
	}
};

} // namespace std