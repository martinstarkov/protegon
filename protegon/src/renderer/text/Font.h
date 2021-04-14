#pragma once

#include <cstdint>

#include "utils/TypeTraits.h"

#include "renderer/text/FontStyle.h"

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace engine {

class Font {
public:
	Font() = default;
	Font(TTF_Font* font);
	/*
	* @param file - File name to load font from.
	* @param ptsize - Point size (based on 72 DPI). This basically translates to pixel height.
	* @param index - Choose a font face from multiple font faces. The first face is always index 0.
	*/
	Font(const char* file, std::uint32_t ptsize, std::uint32_t index = 0);
	TTF_Font* operator=(TTF_Font* font);
	operator TTF_Font* () const;
	bool IsValid() const;
	TTF_Font* operator&() const;
	void Destroy();
private:
	TTF_Font* font{ nullptr };
};

} // namespace engine

