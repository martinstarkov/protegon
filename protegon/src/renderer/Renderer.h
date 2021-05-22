#pragma once

#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Texture.h"
#include "math/Vector2.h"
#include "renderer/Color.h"
#include "renderer/text/Text.h"
#include "renderer/sprites/Flip.h"
#include "utils/Singleton.h"

struct SDL_Renderer;

namespace engine {

class Window;
class Surface;

class Renderer : public Singleton<Renderer> {
public:
	static void DrawTexture(const Texture& texture,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {});

	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {});

	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							const V2_int* center_of_rotation = nullptr,
							const double angle = 0.0,
							Flip flip = Flip::NONE);

	static void DrawText(const Text& text);

	static void DrawPoint(const V2_int& point,
						  const Color& color = colors::DEFAULT_DRAW_COLOR);

	static void DrawLine(const V2_int& origin,
						 const V2_int& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR);


	static void DrawCircle(const V2_int& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR);

	static void DrawSolidCircle(const V2_int& center,
								const double radius,
								const Color& color = colors::DEFAULT_DRAW_COLOR);

	static void DrawRectangle(const V2_int& position, 
							  const V2_int& size, 
							  const Color& color = colors::DEFAULT_DRAW_COLOR);
	
	static void DrawSolidRectangle(const V2_int& position, 
								   const V2_int& size, 
								   const Color& color = colors::DEFAULT_DRAW_COLOR);

	static void SetDrawColor(const Color& color = colors::DEFAULT_DRAW_COLOR);

	static Texture CreateTexture(const Surface& surface);
	
	static void Clear();
	// Display renderer objects to screen.
	static void Present();

	Renderer() = default;

	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;
	
	bool IsValid() const;
private:
	friend class Engine;

	// Renderers must be freed using Destroy().
	static Renderer& Init(const Window& window, 
						  int renderer_index = -1, 
						  std::uint32_t flags = 0);
	static void Destroy();
	
	
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace engine