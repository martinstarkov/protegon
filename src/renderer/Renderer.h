#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "texture/Flip.h"

class SDL_Renderer;
class SDL_Window;

namespace ptgn {

namespace internal {

class Renderer {
public:
    Renderer() = default;
	Renderer(SDL_Window* window, int index, std::uint32_t flags);
    ~Renderer();
    void Present();
    void Clear();
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
    operator SDL_Renderer*() const;
private:
	SDL_Renderer* renderer_{ nullptr };
};

} // namespace internal

} // namespace ptgn