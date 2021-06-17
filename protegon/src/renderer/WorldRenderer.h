#pragma once

#include "core/Camera.h"
#include "renderer/ScreenRenderer.h"

namespace ptgn {

class WorldRenderer {
public:
	// Convert coordinate from world reference frame to screen reference frame.
	static V2_int WorldToScreen(const V2_double& world_coordinate);

	// Convert coordinate from screen reference frame to world reference frame.
	static V2_int ScreenToWorld(const V2_double& screen_coordinate);
	
	static V2_int Scale(const V2_double& size);
	
	static int ScaleX(double value);

	static int ScaleY(double value);

	// Draws a texture to the screen.
	static void DrawTexture(const char* texture_key,
							const V2_double& position,
							const V2_double& size,
							const V2_int source_position = {},
							const V2_int source_size = {});

	// Draws a texture to the screen. Allows for rotation and flip.
	static void DrawTexture(const char* texture_key,
							const V2_double& position,
							const V2_double& size,
							const V2_int source_position = {},
							const V2_int source_size = {},
							const V2_int* center_of_rotation = nullptr,
							const double angle = 0.0,
							Flip flip = Flip::NONE);

	// Draws text to the screen.
	static void DrawText(const Text& text,
						 const V2_double& position,
						 const V2_double& size);

	// Draws a point on the screen.
	static void DrawPoint(const V2_double& point, const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws line to the screen.
	static void DrawLine(const V2_double& origin,
						 const V2_double& destination,
						 const Color& color = colors::DEFAULT_DRAW_COLOR);


	// Draws hollow circle to the screen.
	static void DrawCircle(const V2_double& center,
						   const double radius,
						   const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws filled circle to the screen.
	static void DrawSolidCircle(const V2_double& center,
								const double radius,
								const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws hollow rectangle to the screen.
	static void DrawRectangle(const V2_double& position, 
							  const V2_double& size, 
							  const Color& color = colors::DEFAULT_DRAW_COLOR);
	
	// Draws filled rectangle to the screen.
	static void DrawSolidRectangle(const V2_double& position, 
								   const V2_double& size, 
								   const Color& color = colors::DEFAULT_DRAW_COLOR);

	// Draws texture object to the screen.
	static void DrawTexture(const Texture& texture,
							const V2_double& position,
							const V2_double& size,
							const V2_int source_position = {},
							const V2_int source_size = {});
private:
	// Convert coordinate from world reference frame to screen reference frame.
	static V2_int WorldToScreen(const V2_double& world_coordinate, const Camera& active_camera);

	// Convert coordinate from screen reference frame to world reference frame.
	static V2_int ScreenToWorld(const V2_double& screen_coordinate, const Camera& active_camera);

	static V2_int Scale(const V2_double& size, const Camera& active_camera);

	static int ScaleX(double value, const Camera& active_camera);

	static int ScaleY(double value, const Camera& active_camera);
};

} // namespace ptgn