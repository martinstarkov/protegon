#pragma once

namespace ptgn {

namespace impl {

#include "resources/font/LiberationSans-Regular.h"

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* buffer, unsigned int length) : buffer{ buffer }, length{ length } {}

private:
	friend class ptgn::Font;

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

} // namespace impl

namespace font {

const inline impl::FontBinary LiberationSansRegular{ impl::LiberationSans_Regular_ttf,
													 impl::LiberationSans_Regular_ttf_len };

} // namespace font

} // namespace ptgn
