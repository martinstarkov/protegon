#pragma once

#include <cstdint> // std::int32_t

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Font {
public:
	Font() = delete;
private:
	friend class FontManager;
	friend class Text;

	operator TTF_Font* () const;
	TTF_Font* operator&() const;

	/*
	* @return The difference between the highest and lowest points occupied by the font.
	*/
	std::int32_t GetMaxPixelHeight() const;

	/*
	* @return True if TTF_Font is not nullptr, false otherwise.
	*/
	bool IsValid() const;
	
	/*
	* Frees memory occupied by TTF_Font.
	*/
	void Destroy();

	Font(TTF_Font* font);

	/*
	* @param file - File name to load font from.
	* @param ptsize - Point size (based on 72 DPI). This basically translates to pixel height.
	* @param index - Choose a font face from multiple font faces. The first face is always index 0.
	*/
	Font(const char* file, 
		 std::uint32_t ptsize, 
		 std::uint32_t index = 0);

	TTF_Font* font_{ nullptr };
};

} // namespace ptgn

