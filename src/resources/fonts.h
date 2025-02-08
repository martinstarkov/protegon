#pragma once

namespace ptgn::impl {

#include "resources/font/LiberationSans-Regular.h"

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* buffer, unsigned int length) : buffer{ buffer }, length{ length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

const inline impl::FontBinary LiberationSansRegular{ LiberationSans_Regular_ttf,
													 LiberationSans_Regular_ttf_len };

} // namespace ptgn::impl