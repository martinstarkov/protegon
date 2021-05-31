#pragma once

struct SDL_Surface;

namespace ptgn {

class Text;
class Texture;
class TextureManager;

class Surface {
private:
	friend class Text;
	friend class Texture;
	friend class TextureManager;

	Surface() = default;

	// Create a surface from an image file path.
	Surface(const char* img_file_path);

	// Conversion operators.

	operator SDL_Surface* () const;
	SDL_Surface* operator&() const;

	/*
	* @return True if SDL_Surface is not nullptr, false otherwise.
	*/
	bool IsValid() const;

	/*
	* Frees memory used by SDL_Surface.
	*/
	void Destroy();

	Surface(SDL_Surface* surface);

	SDL_Surface* surface_{ nullptr };
};

} // namespace ptgn