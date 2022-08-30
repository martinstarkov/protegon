#pragma once

#include <memory> // std::unique_ptr

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

namespace ptgn {

class SDLInstance {
public:
	SDLInstance();
	~SDLInstance();
	SDL_Window* GetWindow() const;
	SDL_Renderer* GetRenderer() const;
private:
	void InitSDL();
	void InitSDLImage();
	void InitSDLTTF();
	void InitSDLMixer();
	void InitWindow();
	void InitRenderer();

	SDL_Window* window_{ nullptr };
	SDL_Renderer* renderer_{ nullptr };
};

namespace global {

namespace hidden {

extern std::unique_ptr<SDLInstance> sdl;

} // namespace hidden

void InitSDL();
void DestroySDL();
SDLInstance& GetSDL();

} // namespace global

} // namespace ptgn