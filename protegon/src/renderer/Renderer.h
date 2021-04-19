#pragma once

#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Window.h"
#include "renderer/Texture.h"
#include "renderer/Color.h"
#include "renderer/sprites/Flip.h"

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
	static void DrawTexture(const Texture& texture,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							std::size_t display_index = 0);

	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							std::size_t display_index = 0);

	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							const V2_int* center_of_rotation = nullptr,
							const double angle = 0.0,
							Flip flip = Flip::NONE,
							std::size_t display_index = 0);

	static void DrawText(const Text& text,
						 std::size_t display_index = 0);

	static void DrawPoint(const V2_int& point,
						  const Color& color = colors::DEFAULT_DRAW_COLOR,
						  std::size_t display_index = 0);

	static void DrawLine(const V2_int& origin,
						 const V2_int& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR,
						 std::size_t display_index = 0);


	static void DrawCircle(const V2_int& center,
						 const double radius,
						 const Color& color = colors::DEFAULT_DRAW_COLOR,
						 std::size_t display_index = 0);

	static void DrawSolidCircle(const V2_int& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR,
						   std::size_t display_index = 0);

	static void DrawRectangle(const V2_int& position, 
							  const V2_int& size, 
							  const Color& color = colors::DEFAULT_DRAW_COLOR,
							  std::size_t display_index = 0);
	
	static void DrawSolidRectangle(const V2_int& position, 
								   const V2_int& size, 
								   const Color& color = colors::DEFAULT_DRAW_COLOR,
								   std::size_t display_index = 0);

	Renderer() = default;

	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;
	
	bool IsValid() const;
	void Clear();
	// Display renderer objects to screen.
	void Present() const;
	void Destroy();
	
	std::size_t GetDisplayIndex() const;

	void SetDrawColor(const Color& color = colors::DEFAULT_DRAW_COLOR);
private:
	friend class Engine;

	Renderer(SDL_Renderer* renderer);
	// Renderers must be freed using Destroy().
	Renderer(const Window& window, int renderer_index = -1, std::uint32_t flags = 0);
	
	SDL_Renderer* renderer_{ nullptr };
	std::size_t display_index_{ 0 };
};

} // namespace engine