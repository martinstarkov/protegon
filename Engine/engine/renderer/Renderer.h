#pragma once

struct SDL_Renderer;

namespace engine {

struct Renderer {
	Renderer() = default;
	Renderer(SDL_Renderer* renderer);
	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;
	operator bool() const;
	void Clear();
	void Present();
	void Destroy();
	SDL_Renderer* renderer = nullptr;
};

} // namespace engine