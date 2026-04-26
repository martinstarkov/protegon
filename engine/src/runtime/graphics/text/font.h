#pragma once

#include <ecs/ecs.h>

#include <ostream>
#include <utility>

#include "core/util/entity_handle.h"
#include "core/util/hash.h"
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

class Font : public EntityHandle {
public:
	using EntityHandle::EntityHandle;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Font> {
	std::size_t operator()(const ptgn::Font& font) const {
		return ptgn::Hash(font.GetEntity());
	}
};