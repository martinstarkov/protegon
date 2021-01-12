#pragma once

#include <unordered_map> // std::unordered_map

#include "utils/Vector2.h"
#include "renderer/Color.h"
#include "renderer/Flip.h"
#include "renderer/Renderer.h"

namespace engine {

// Default color of renderer window
#define DEFAULT_RENDERER_COLOR WHITE
// Default color of renderer objects
#define DEFAULT_RENDER_COLOR BLACK

struct Texture;

class TextureManager {
private:
public:
	static void Load(const char* texture_key, const char* texture_path);

	static Color GetDefaultRendererColor();
	static void SetDrawColor(Color color);

	static void DrawPoint(V2_int point, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawPoint(V2_double point, Color color = DEFAULT_RENDER_COLOR);
	static void DrawLine(V2_int origin, V2_int destination, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawLine(V2_double origin, V2_double destination, Color color = DEFAULT_RENDER_COLOR);
	static void DrawSolidRectangle(V2_int position, V2_int size, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawSolidRectangle(V2_double position, V2_double size, Color color = DEFAULT_RENDER_COLOR);
	static void DrawRectangle(V2_int position, V2_int size, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawRectangle(V2_double position, V2_double size, Color color = DEFAULT_RENDER_COLOR);
	static void DrawRectangle(V2_int position, V2_int size, double rotation, V2_double* center_of_rotation = nullptr, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawRectangle(V2_double position, V2_double size, double rotation, V2_double* center_of_rotation = nullptr, Color color = DEFAULT_RENDER_COLOR);
	static void DrawRectangle(const char* texture_key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip = Flip::NONE, V2_double* center_of_rotation = nullptr, double angle = 0.0);
	//static void DrawRectangle(const char* texture_key, V2_double src_position, V2_double src_size, V2_double dest_position, V2_double dest_size, Flip flip = Flip::NONE, V2_double* center_of_rotation = nullptr, double angle = 0.0);
	static void DrawCircle(V2_int center, int radius, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawCircle(V2_double center, int radius, Color color = DEFAULT_RENDER_COLOR);

	static void SetDrawColor(Renderer renderer, Color color);
	static void DrawPoint(Renderer renderer, V2_int point, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawPoint(Renderer renderer, V2_double point, Color color = DEFAULT_RENDER_COLOR);
	static void DrawLine(Renderer renderer, V2_int origin, V2_int destination, Color color = DEFAULT_RENDER_COLOR);
	//static void DrawLine(Renderer renderer, V2_double origin, V2_double destination, Color color = DEFAULT_RENDER_COLOR);

	friend class Image;
	static void Clean();
private:
	static void RemoveTexture(const char* texture_key);
	static Texture GetTexture(const char* texture_key);
	static std::unordered_map<std::size_t, Texture> texture_map_;
};

} // namespace engine