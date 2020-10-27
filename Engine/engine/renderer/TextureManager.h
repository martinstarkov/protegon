#pragma once

#include <string>
#include <unordered_map>

#include "utils/Vector2.h"
#include "renderer/Color.h"
#include "renderer/Flip.h"
#include "renderer/Texture.h"

namespace engine {

// Default color of renderer window
#define DEFAULT_RENDERER_COLOR WHITE
// Default color of renderer objects
#define DEFAULT_RENDER_COLOR BLACK

class TextureManager {
public:
	static Texture Load(std::string& key, const std::string& path);

	static Color GetDefaultRendererColor();
	static void SetDrawColor(Color color);

	static void DrawPoint(V2_int point, Color color = DEFAULT_RENDER_COLOR);
	static void DrawLine(V2_int origin, V2_int destination, Color color = DEFAULT_RENDER_COLOR);
	static void DrawSolidRectangle(V2_int position, V2_int size, Color color = DEFAULT_RENDER_COLOR);
	static void DrawRectangle(V2_int position, V2_int size, Color color = DEFAULT_RENDER_COLOR);
	static void DrawRectangle(const std::string& key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip, double angle, V2_int center_of_rotation);
	static void DrawRectangle(const std::string& key, V2_int src_position, V2_int src_size, V2_int dest_position, V2_int dest_size, Flip flip = Flip::NONE, double angle = 0.0);
	static void DrawCircle(V2_int center, int radius, Color color = DEFAULT_RENDER_COLOR);

	static void Clean();
	static void RemoveTexture(const std::string& key);
private:
	static Texture GetTexture(const std::string& key);
	static std::unordered_map<std::string, Texture> texture_map_;
};

} // namespace engine