#pragma once

namespace ptgn::impl {

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

[[nodiscard]] FontBinary GetLiberationSansRegular();

} // namespace ptgn::impl