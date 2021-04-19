#pragma once

#include <unordered_map> // std::unordered_map
#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "renderer/Texture.h"

namespace engine {

class TextureManager {
private:
public:
	static void Load(const char* texture_key,
					 const char* texture_path,
					 std::size_t display_index = 0);

	static Texture GetTexture(const char* texture_key);

	//static void RenderTexture(const Renderer& renderer, const Texture& texture, const AABB* source = nullptr, const AABB* destination = nullptr);

	// Return the location of a 4 byte integer value containg the RGBA32 color of the pixel on an SDL_Surface or SDL_Texture.
	static std::uint32_t& GetTexturePixel(void* pixels, 
										  const int pitch, 
										  const V2_int& position);

	//static Color GetDefaultRendererColor();
	//static void SetDrawColor(const Color& color);

	/*static void DrawPoint(const V2_int& point, const Color& color);
	static void DrawLine(const V2_int& origin, const V2_int& destination, const Color& color);
	static void DrawSolidRectangle(const V2_int& position, const V2_int& size, const Color& color);
	static void DrawRectangle(const V2_int& position, const V2_int& size, const Color& color);*/
	//static void DrawRectangle(const V2_int& position, const V2_int& size, const double rotation, const Color& color, V2_double* center_of_rotation = nullptr);
	//static void DrawRectangle(const char* texture_key, const V2_int& src_position, const V2_int& src_size, const V2_int& dest_position, const V2_int& dest_size, Flip flip = Flip::NONE, V2_double* center_of_rotation = nullptr, double angle = 0.0);
	//static void DrawCircle(const V2_int& center, const double radius, const Color& color);
	//static void DrawSolidCircle(const V2_int& center, const double radius, const Color& color);

	//static void SetDrawColor(const Renderer& renderer, const Color& color);
	//static void DrawPoint(const Renderer& renderer, const V2_int& point, const Color& color);
	//static void DrawLine(const Renderer& renderer, const V2_int& origin, const V2_int& destination, const Color& color);

	friend class Image;
	static void Clear();
private:
	//static void RemoveTexture(const char* texture_key);
	static std::unordered_map<std::size_t, Texture> texture_map_;
};

} // namespace engine