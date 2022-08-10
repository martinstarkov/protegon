#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "renderer/Flip.h"
#include "physics/Types.h"

namespace ptgn {

class Text;

namespace draw {

// TODO: Fix flags to be an enum with | operators.
void Init(int index = 0, std::uint32_t flags = 0);

void Release();

bool Exists();

void Present();

void Clear();

void SetColor(const Color& color = color::DEFAULT);

void Point(const ptgn::Point<int>& p,
           const Color& color = color::DEFAULT);

void Line(const ptgn::Line<int>& l,
          const Color& color = color::DEFAULT);

void Circle(const ptgn::Circle<int>& c,
            const Color& color = color::DEFAULT);

void SolidCircle(const ptgn::Circle<int>& c,
                 const Color& color = color::DEFAULT);

void Arc(const ptgn::Circle<int>& arc_circle,
         float start_angle,
         float end_angle,
         const Color& color = color::DEFAULT);

void Capsule(const ptgn::Capsule<int>& c,
             const Color& color = color::DEFAULT,
             bool draw_centerline = false);

// AABB = Axis-aligned bounding-box
void AABB(const ptgn::AABB<int>& a,
               const Color& color = color::DEFAULT);

void SolidAABB(const ptgn::AABB<int>& a,
                    const Color& color = color::DEFAULT);

// Draws the texture to the screen.
void Texture(const char* texture_key,
             const ptgn::AABB<int>& texture,
             const ptgn::AABB<int>& source = {});

// Draws the texture to the screen. Allows for rotation and texture flipping.
// Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
void Texture(const char* texture_key,
             const ptgn::AABB<int>& texture,
             const ptgn::AABB<int>& source,
             const V2_int* center_of_rotation,
             const float angle,
             Flip flip);

// Draws text to the screen.
void Text(const ptgn::Text& text,
          const ptgn::AABB<int>& box);

// Draws text to the screen.
void Text(const char* text_key,
          const ptgn::AABB<int>& box);

// Draws text to the screen which is reallocated each frame.
// Good for changing text such as counters.
void TemporaryText(const char* texture_key,
                   const char* font_key,
                   const char* text_content,
                   const Color& text_color,
                   const ptgn::AABB<int>& box);

} // namespace draw

} // namespace ptgn