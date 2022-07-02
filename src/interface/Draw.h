#pragma once

#include "math/Vector2.h"
#include "texture/Flip.h"
#include "renderer/Colors.h"
#include "components/Transform.h"
#include "physics/Shape.h"

namespace ptgn {

namespace draw {

namespace internal {

using DrawCallback = void(*)(const component::Shape& shape, const component::Transform& transform, const Color& color);

extern DrawCallback DrawDispatch[static_cast<int>(physics::ShapeType::COUNT)][2];

void DrawShapeSolidAABB(const component::Shape& shape, const component::Transform& transform, const Color& color);

void DrawShapeSolidCircle(const component::Shape& shape, const component::Transform& transform, const Color& color);

void DrawShapeAABB(const component::Shape& shape, const component::Transform& transform, const Color& color);

void DrawShapeCircle(const component::Shape& shape, const component::Transform& transform, const Color& color);

} // namespace internal

// Draws a hollow shape object to the screen (wrapper around draw::Rectangle, draw::Circle, etc).
void Shape(const component::Shape& shape, const component::Transform& transform, const Color& color = color::DEFAULT);

// Draws a solid shape object to the screen (wrapper around draw::Rectangle, draw::Circle, etc).
void SolidShape(const component::Shape& shape, const component::Transform& transform, const Color& color = color::DEFAULT);

// Presents the drawn objects to the screen. Must be called once drawing is done.
void Present();

// Clear the drawn objects from the screen.
void Clear();

/*
* Sets the background color of the window.
* Note that this will also clear the screen.
*/
void SetColor(const Color& color);

// Draws a texture to the screen.
void Texture(const char* texture_key,
			 const V2_int& texture_position,
			 const V2_int& texture_size,
			 const V2_int& source_position = {},
			 const V2_int& source_size = {});

// Draws a texture to the screen. Allows for rotation and flip.
void Texture(const char* texture_key,
			 const V2_int& texture_position,
			 const V2_int& texture_size,
			 const V2_int& source_position,
			 const V2_int& source_size,
			 const V2_int* center_of_rotation = nullptr,
			 const double angle = 0.0,
			 Flip flip = Flip::NONE);

/* 
* Draws text to the screen.
* @param text_key The key used to load the text into the text manager (see ptgn::text namespace)
* @param text_position Top left of box where text is drawn.
* @param text_size Size of box in which text is drawn.
*/
void Text(const char* text_key,
		  const V2_int& text_position,
		  const V2_int& text_size);

// Draws text to the screen.

/*
* Draws text to the screen.
* Note: It is preferred to load the text into the text manager 
* (see ptgn::text namespace) as this function will allocate and free
* memory on the heap once every frame which is slower.
* @param font_key The key used to load the font into the font manager (see ptgn::font namespace)
* @param text_content The content of the text (use a std::string and .c_str() if numbers are desired).
* @param text_position Top left of box where text is drawn.
* @param text_size Size of box in which text is drawn.
* @param text_color Color of the text.
*/
void Text(const char* font_key, 
		  const char* text_content,
		  const V2_int& text_position,
		  const V2_int& text_size,
		  const Color& text_color = color::BLACK);

// Draws a user interface element.
//void UI(const char* ui_key,
//		const V2_int& position,
//		const V2_int& size);

// Draws a point on the screen.
void Point(const V2_int& point,
		   const Color& color = color::DEFAULT);

// Draws line to the screen.
void Line(const V2_int& origin,
		  const V2_int& destination,
		  const Color& color = color::DEFAULT);

// Draws hollow circle to the screen.
void Circle(const V2_int& center,
			const double radius,
			const Color& color = color::DEFAULT);

// Draws filled circle to the screen.
void SolidCircle(const V2_int& center,
				 const double radius,
				 const Color& color = color::DEFAULT);

// Draws hollow rectangle to the screen.
void Rectangle(const V2_int& top_left,
			   const V2_int& size,
			   const Color& color = color::DEFAULT);

// Draws filled rectangle to the screen.
void SolidRectangle(const V2_int& top_left,
					const V2_int& size,
					const Color& color = color::DEFAULT);

} // namespace draw

} // namespace ptgn