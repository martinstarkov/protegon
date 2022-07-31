#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "renderer/Colors.h"
#include "renderer/Flip.h"

namespace ptgn {

class Text;

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

/*
* @param center Center point of the arc circle.
* @param direction Vector from center to point at which arc circle starts.
* @param radius Radius of the arc circle.
* @param angle Angle in radians by which arc is rotated (positive is clockwise, negative is anti-clockwise).
* @param color Color of arc.
* @param resolution Number of individual points with which to draw the arc, must be at least 2.
*/
void Arc(const V2_int& center,
         const double radius,
         double start_angle,
         double end_angle,
         const Color& color = color::DEFAULT);

void Capsule(const V2_int& origin,
             const V2_int& destination,
             const double radius,
             const Color& color = color::DEFAULT,
             bool draw_centerline = false);

void Rectangle(const V2_int& top_left,
               const V2_int& size,
               const Color& color = color::DEFAULT);

void SolidRectangle(const V2_int& top_left,
                    const V2_int& size,
                    const Color& color = color::DEFAULT);

// Draws the texture to the screen.
void Texture(const char* texture_key,
             const V2_int& texture_position,
             const V2_int& texture_size,
             const V2_int& source_position = {},
             const V2_int& source_size = {});

// Draws the texture to the screen. Allows for rotation and texture flipping.
// Set center_of_rotation to nullptr if center of rotation is desired to be the center of the texture.
void Texture(const char* texture_key,
             const V2_int& texture_position,
             const V2_int& texture_size,
             const V2_int& source_position,
             const V2_int& source_size,
             const V2_int* center_of_rotation,
             const double angle,
             Flip flip);

// Draws text to the screen.
void Text(const ptgn::Text& text,
          const V2_int& text_position,
          const V2_int& text_size);

// Draws text to the screen.
void Text(const char* text_key,
          const V2_int& text_position,
          const V2_int& text_size);

// Draws text to the screen which is reallocated each frame.
// Good for changing text such as counters.
void TemporaryText(const char* texture_key,
                   const char* font_key,
                   const char* text_content,
                   const Color& text_color,
                   const V2_int& text_position,
                   const V2_int& text_size);

} // namespace draw

} // namespace ptgn