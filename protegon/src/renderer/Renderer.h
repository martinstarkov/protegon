#pragma once

#include <cstdint> // std::uint32_t

#include "renderer/Window.h"
#include "renderer/Texture.h"

#include "math/Vector2.h"

struct SDL_Renderer;

namespace engine {

class Text;

enum class RenderMode : int {
	SOLID,
	SHADED,
	BLENDED
};

class Renderer {
public:

	void DrawTexture(const Texture& texture,
					 const V2_int& position,
					 const V2_int& size,
					 const V2_int source_position = {},
					 const V2_int source_size = {}) const;

	void DrawText(const Text& text) const;

	Renderer() = default;
	Renderer(SDL_Renderer* renderer);
	Renderer(const Window& window, int renderer_index = -1, std::uint32_t flags = 0);
	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;
	bool IsValid() const;
	void Clear();
	// Display renderer objects to screen.
	void Present() const;
	void Destroy();
private:
	SDL_Renderer* renderer{ nullptr };
};

} // namespace engine