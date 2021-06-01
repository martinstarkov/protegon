#pragma once

#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "math/Vector2.h"
#include "renderer/Texture.h"
#include "renderer/Color.h"
#include "renderer/Colors.h"
#include "renderer/sprites/Flip.h"
#include "utils/Singleton.h"

struct SDL_Renderer;

namespace ptgn {

class Window;
class Text;
class Surface;

class ScreenRenderer : public Singleton<ScreenRenderer> {
public:
	// Draws a texture to the screen.
	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {});

	// Draws a texture to the screen. Allows for rotation and flip.
	static void DrawTexture(const char* texture_key,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							const V2_int* center_of_rotation = nullptr,
							const double angle = 0.0,
							Flip flip = Flip::NONE);

	// Draws text to the screen.
	static void DrawText(const Text& text);

	// Draws a point on the screen.
	static void DrawPoint(const V2_int& point, const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws line to the screen.
	static void DrawLine(const V2_int& origin,
						 const V2_int& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR);


	// Draws hollow circle to the screen.
	static void DrawCircle(const V2_int& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws filled circle to the screen.
	static void DrawSolidCircle(const V2_int& center,
								const double radius,
								const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws hollow rectangle to the screen.
	static void DrawRectangle(const V2_int& position, 
							  const V2_int& size, 
							  const Color& color = colors::DEFAULT_DRAW_COLOR);
	
	// Draws filled rectangle to the screen.
	static void DrawSolidRectangle(const V2_int& position, 
								   const V2_int& size, 
								   const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Sets the screen draw color.
	static void SetDrawColor(const Color& color = colors::DEFAULT_DRAW_COLOR);
	
	// Clears the screen.
	static void Clear();
	// Display renderer objects to screen.
	static void Present();
	
	// Draws texture object to the screen.
	static void DrawTexture(const Texture& texture,
							const V2_int& position,
							const V2_int& size,
							const V2_int source_position = {},
							const V2_int source_size = {});

	// Use the below functions with caution.
	// Remember to always free the Texture object using the Destroy method.

	// Creates texture from surface.
	// Texture must be freed using Destroy.
	static Texture CreateTexture(const Surface& surface);
	
	// Creates texture with given size and pixel format.
	// Texture access should be chosen based on texture access frequency.
	// Texture must be freed using Destroy.
	static Texture CreateTexture(const V2_int& size,
								 PixelFormat format = PixelFormat::RGBA8888,
								 TextureAccess texture_access = TextureAccess::STREAMING);

private:
	friend class Engine;
	friend class TextureManager;
	friend class Texture;
	friend class Text;
	friend class Singleton<ScreenRenderer>;

	/*
	* Initializes the singleton renderer instance.
	* @param renderer_index Index of renderering driver, -1 for first matching flags
	* @paramm flags ScreenRenderer driver flags.
	* @return ScreenRenderer singleton instance.
	*/
	static ScreenRenderer& Init(const Window& window, 
								int renderer_index = -1, 
								std::uint32_t flags = 0);

	// Frees memory used by SDL_Renderer.
	static void Destroy();

	ScreenRenderer() = default;

	// Conversions to SDL_Renderer for internal functions.

	operator SDL_Renderer* () const;
	SDL_Renderer* operator&() const;

	/*
	* @return True if SDL_Renderer is not nullptr, false otherwise.
	*/
	bool IsValid() const;
	
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn