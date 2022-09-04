#pragma once

#include <cstdint> // std::int32_t, std::uint32_t

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Font {
public:
	Font() = delete;
	Font(const char* font_path, std::uint32_t point_size, std::uint32_t index = 0);
	~Font();
	std::int32_t GetHeight() const;
	bool IsValid() const;
private:
	TTF_Font* font_{ nullptr };
};

} // namespace ptgn