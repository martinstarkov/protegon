#pragma once

namespace ptgn::impl {

#include "resources/font/LiberationSans-Regular.h"

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

const inline impl::FontBinary LiberationSansRegular{ LiberationSans_Regular_ttf,
													 LiberationSans_Regular_ttf_len };

} // namespace ptgn::impl