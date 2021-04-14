#pragma once

struct SDL_Surface;

namespace engine {

class Surface {
public:
	Surface() = default;
	Surface(SDL_Surface* surface);
	operator SDL_Surface* () const;
	SDL_Surface* operator&() const;
	bool IsValid() const;
	void Destroy();
private:
	SDL_Surface* surface{ nullptr };
};

} // namespace engine