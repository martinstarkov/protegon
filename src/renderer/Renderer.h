#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "renderer/Flip.h"
#include "renderer/Texture.h"

struct SDL_Renderer;
struct SDL_Window;

namespace ptgn {

class Renderer {
public:
    Renderer() = delete;
    ~Renderer() = delete;
    static void Create(SDL_Window* window, int index = 0, std::uint32_t flags = 0);
    static void Destroy();
    static SDL_Renderer* Get() { return renderer_; }
    static void Present();
    static void Clear();
    static void SetDrawColor(const Color& color = color::DEFAULT);
    static void DrawPoint(const V2_int& point,
                   const Color& color = color::DEFAULT);
    static void DrawLine(const V2_int& origin,
                  const V2_int& destination,
                  const Color& color = color::DEFAULT);
    static void DrawCircle(const V2_int& center,
                    const double radius,
                    const Color& color = color::DEFAULT);
    static void DrawSolidCircle(const V2_int& center,
                         const double radius,
                         const Color& color = color::DEFAULT);
    static void DrawRectangle(const V2_int& top_left,
                       const V2_int& size,
                       const Color& color = color::DEFAULT);
    static void DrawSolidRectangle(const V2_int& top_left,
                            const V2_int& size,
                            const Color& color = color::DEFAULT);
    // Draws the texture to the screen.
    static void DrawTexture(const Texture& texture,
                     const V2_int& texture_position,
                     const V2_int& texture_size,
                     const V2_int& source_position,
                     const V2_int& source_size);
    // Draws the texture to the screen. Allows for rotation and texture flipping.
    // Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
    static void DrawTexture(const Texture& texture,
                            const V2_int& texture_position,
                            const V2_int& texture_size,
                            const V2_int& source_position,
                            const V2_int& source_size,
                            const V2_int* center_of_rotation,
                            const double angle,
                            Flip flip);
private:
	static SDL_Renderer* renderer_;
};

} // namespace ptgn