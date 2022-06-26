#include "Draw.h"

#include "renderer/Renderer.h"

namespace ptgn {

namespace draw {

namespace internal {

DrawCallback DrawDispatch[static_cast<int>(physics::ShapeType::COUNT)][2] = {
	{ DrawShapeCircle, DrawShapeSolidCircle },
	{ DrawShapeAABB,   DrawShapeSolidAABB }
};

void DrawShapeSolidAABB(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	auto& aabb = shape.instance->CastTo<physics::Rectangle>();
	draw::SolidRectangle(transform.position, aabb.size, color);
}

void DrawShapeSolidCircle(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	auto& circle = shape.instance->CastTo<physics::Circle>();
	draw::SolidCircle(transform.position, circle.radius, color);
}

void DrawShapeAABB(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	auto& aabb = shape.instance->CastTo<physics::Rectangle>();
	draw::Rectangle(transform.position, aabb.size, color);
}

void DrawShapeCircle(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	auto& circle = shape.instance->CastTo<physics::Circle>();
	draw::Circle(transform.position, circle.radius, color);
}

} // namespace internal

void Shape(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	return internal::DrawDispatch[static_cast<int>(shape.instance->GetType())][0](shape, transform, color);
}

void SolidShape(const component::Shape& shape, const component::Transform& transform, const Color& color) {
	return internal::DrawDispatch[static_cast<int>(shape.instance->GetType())][1](shape, transform, color);
}
	
void Present() {
	auto& renderer{ services::GetRenderer() };
	renderer.Present();
}

void Clear() {
	auto& renderer{ services::GetRenderer() };
	renderer.Clear();
}

void SetColor(const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.SetDrawColor(color);
	renderer.Clear();
}

void Texture(const char* texture_key,
			 const V2_int& position,
			 const V2_int& size,
			 const V2_int& source_position,
			 const V2_int& source_size) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawTexture(texture_key, position, size, source_position, source_size);
}

void Texture(const char* texture_key,
			 const V2_int& position,
			 const V2_int& size,
			 const V2_int& source_position,
			 const V2_int& source_size,
			 const V2_int* center_of_rotation,
			 const double angle,
			 Flip flip) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawTexture(texture_key, position, size, source_position, source_size, center_of_rotation, angle, flip);
}

void Text(const char* text_key,
		  const V2_int& position,
		  const V2_int& size) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawText(text_key, position, size);
}

void UI(const char* ui_key,
		const V2_int& position,
		const V2_int& size) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawUI(ui_key, position, size);
}

void Point(const V2_int& point,
		   const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawPoint(point, color);
}

void Line(const V2_int& origin,
		  const V2_int& destination,
		  const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawLine(origin, destination, color);
}

void Circle(const V2_int& center,
			const double radius,
			const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawCircle(center, radius, color);
}

void SolidCircle(const V2_int& center,
				 const double radius,
				 const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawSolidCircle(center, radius, color);
}

void Rectangle(const V2_int& top_left,
				const V2_int& size,
				const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawRectangle(top_left, size, color);
}

void SolidRectangle(const V2_int& top_left,
					const V2_int& size,
					const Color& color) {
	auto& renderer{ services::GetRenderer() };
	renderer.DrawSolidRectangle(top_left, size, color);
}

} // namespace draw

} // namespace ptgn