#pragma once

struct SDL_Window;
struct SDL_Renderer;

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

} // namespace ptgn