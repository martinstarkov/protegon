#pragma once

#include <cstdint> // std::int32_t, std::uint32_t

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

namespace internal {

class Font {
public:
	Font() = default;
	/*
	* @param font_path Path to font file.
	* @param point_size Point size (based on 72 DPI). This translates to pixel height.
	* @param index Font face index, the first face is 0.
	*/
	Font(const char* font_path, std::uint32_t point_size, std::uint32_t index = 0);
	~Font();
	std::int32_t GetHeight() const;
	operator TTF_Font*() const;
private:
	TTF_Font* font_{ nullptr };
};

} // namespace internal

} // namespace ptgn