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
	Renderer(SDL_Window* window, int index, std::uint32_t flags);
    ~Renderer();
    void Present() const;
    void Clear() const;
    void SetDrawColor(const Color& color) const;
    void DrawPoint(const V2_int& point,
                   const Color& color = color::DEFAULT) const;
    void DrawLine(const V2_int& origin,
                  const V2_int& destination,
                  const Color& color = color::DEFAULT) const;
    void DrawCircle(const V2_int& center,
                    const double radius,
                    const Color& color = color::DEFAULT) const;
    void DrawSolidCircle(const V2_int& center,
                         const double radius,
                         const Color& color = color::DEFAULT) const;
    void DrawRectangle(const V2_int& top_left,
                       const V2_int& size,
                       const Color& color = color::DEFAULT) const;
    void DrawSolidRectangle(const V2_int& top_left,
                            const V2_int& size,
                            const Color& color = color::DEFAULT) const;
    // Draws the texture to the screen.
    void DrawTexture(const Texture& texture,
                     const V2_int& texture_position,
                     const V2_int& texture_size,
                     const V2_int& source_position,
                     const V2_int& source_size) const;
    // Draws the texture to the screen. Allows for rotation and texture flipping.
    // Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
    void DrawTexture(const Texture& texture,
                     const V2_int& texture_position,
                     const V2_int& texture_size,
                     const V2_int& source_position,
                     const V2_int& source_size,
                     const V2_int* center_of_rotation,
                     const double angle,
                     Flip flip) const;
    operator SDL_Renderer*() const;
private:
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn