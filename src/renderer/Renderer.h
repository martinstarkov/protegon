#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "renderer/Flip.h"
#include "renderer/Texture.h"

namespace ptgn {

namespace draw {

void Init(int index = 0, std::uint32_t flags = 0);
void Release();
bool Exists();
void Present();
void Clear();
void SetColor(const Color& color = color::DEFAULT);
void Point(const V2_int& point,
                  const Color& color = color::DEFAULT);
void Line(const V2_int& origin,
                 const V2_int& destination,
                 const Color& color = color::DEFAULT);
void Circle(const V2_int& center,
                   const double radius,
                   const Color& color = color::DEFAULT);
void SolidCircle(const V2_int& center,
                        const double radius,
                        const Color& color = color::DEFAULT);
void Rectangle(const V2_int& top_left,
                      const V2_int& size,
                      const Color& color = color::DEFAULT);
void SolidRectangle(const V2_int& top_left,
                           const V2_int& size,
                           const Color& color = color::DEFAULT);
// Draws the texture to the screen.
void Texture(const ptgn::Texture& texture,
                    const V2_int& texture_position,
                    const V2_int& texture_size,
                    const V2_int& source_position,
                    const V2_int& source_size);
// Draws the texture to the screen. Allows for rotation and texture flipping.
// Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
void Texture(const ptgn::Texture& texture,
                    const V2_int& texture_position,
                    const V2_int& texture_size,
                    const V2_int& source_position,
                    const V2_int& source_size,
                    const V2_int* center_of_rotation,
                    const double angle,
                    Flip flip);

} // namespace draw

} // namespace ptgn