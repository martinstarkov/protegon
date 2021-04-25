#pragma once

struct SDL_Surface;

namespace engine {

// Surfaces must be freed using Destroy().
class Surface {
public:
	Surface() = default;
	Surface(const char* file_path);
	
	operator SDL_Surface* () const;
	SDL_Surface* operator&() const;
	
	bool IsValid() const;
	void Destroy();
private:
	friend class Text;

	Surface(SDL_Surface* surface);

	SDL_Surface* surface_{ nullptr };
};

} // namespace engine