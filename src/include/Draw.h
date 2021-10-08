#pragma once

#include "math/Vector2.h"
#include "texture/Flip.h"
#include "renderer/Colors.h"

namespace ptgn {

namespace draw {

// Draws a texture to the screen.
void Texture(const char* texture_key,
					const V2_int& position,
					const V2_int& size,
					const V2_int& source_position = {},
					const V2_int& source_size = {});

// Draws a texture to the screen. Allows for rotation and flip.
void Texture(const char* texture_key,
					const V2_int& position,
					const V2_int& size,
					const V2_int& source_position,
					const V2_int& source_size,
					const V2_int* center_of_rotation = nullptr,
					const double angle = 0.0,
					Flip flip = Flip::NONE);

// Draws text to the screen.
void Text(const char* text_key,
				 const V2_int& position,
				 const V2_int& size);

// Draws a user interface element.
void UI(const char* ui_key,
			   const V2_int& position,
			   const V2_int& size);

// Draws a point on the screen.
void Point(const V2_int& point,
				  const Color& color = colors::DEFAULT);

// Draws line to the screen.
void Line(const V2_int& origin,
				 const V2_int& destination,
				 const Color& color = colors::DEFAULT);

// Draws hollow circle to the screen.
void Circle(const V2_int& center,
				   const double radius,
				   const Color& color = colors::DEFAULT);

// Draws filled circle to the screen.
void SolidCircle(const V2_int& center,
						const double radius,
						const Color& color = colors::DEFAULT);

// Draws hollow rectangle to the screen.
void Rectangle(const V2_int& center,
				const V2_int& size,
				const Color& color = colors::DEFAULT);

// Draws filled rectangle to the screen.
void SolidRectangle(const V2_int& center,
						   const V2_int& size,
						   const Color& color = colors::DEFAULT);

} // namespace draw

} // namespace ptgn