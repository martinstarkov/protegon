#include "Draw.h"

#include "managers/WindowManager.h"
#include "managers/TextureManager.h"
#include "math/Hash.h"

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
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.Present();
}

void Clear() {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.Clear();
}

void SetColor(const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.SetDrawColor(color);
	renderer.Clear();
}

void Texture(const char* texture_key,
			 const V2_int& texture_position,
			 const V2_int& texture_size,
			 const V2_int& source_position,
			 const V2_int& source_size) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	auto& texture_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::TextureManager>() };
	const auto key{ math::Hash(texture_key) };
	assert(texture_manager.Has(key) && "Cannot draw texture which has not been loaded into the texture manager");
	const auto texture = texture_manager.Get(key);
	renderer.DrawTexture(*texture, texture_position, texture_size, source_position, source_size);
}

void Texture(const char* texture_key,
			 const V2_int& texture_position,
			 const V2_int& texture_size,
			 const V2_int& source_position,
			 const V2_int& source_size,
			 const V2_int* center_of_rotation,
			 const double angle,
			 Flip flip) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	auto& texture_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::TextureManager>() };
	const auto key{ math::Hash(texture_key) };
	assert(texture_manager.Has(key) && "Cannot draw texture which has not been loaded into the texture manager");
	const auto texture = texture_manager.Get(key);
	renderer.DrawTexture(*texture, texture_position, texture_size, source_position, source_size, center_of_rotation, angle, flip);
}

void Text(const char* text_key,
		  const V2_int& text_position,
		  const V2_int& text_size) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	auto& text_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::TextManager>() };
	const auto key{ math::Hash(text_key) };
	assert(text_manager.Has(key) && "Cannot draw text which has not been loaded into the text manager");
	const auto text = text_manager.Get(key);
	renderer.DrawTexture(text->GetTexture(), text_position, text_size, {}, {});
}

void Text(const char* font_key,
		  const char* text_content,
		  const V2_int& text_position,
		  const V2_int& text_size,
		  const Color& text_color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	ptgn::internal::Text text{ math::Hash(font_key), text_content, text_color };
	renderer.DrawTexture(text.GetTexture(), text_position, text_size, {}, {});
}

//void UI(const char* ui_key,
//		const V2_int& position,
//		const V2_int& size) {
//	const auto& renderer{ services::GetRenderer() };
//	renderer.DrawUI(ui_key, position, size);
//}

void Point(const V2_int& point,
		   const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawPoint(point, color);
}

void Line(const V2_int& origin,
		  const V2_int& destination,
		  const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawLine(origin, destination, color);
}

void Circle(const V2_int& center,
			const double radius,
			const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawCircle(center, radius, color);
}

void SolidCircle(const V2_int& center,
				 const double radius,
				 const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawSolidCircle(center, radius, color);
}

void Rectangle(const V2_int& top_left,
				const V2_int& size,
				const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawRectangle(top_left, size, color);
}

void SolidRectangle(const V2_int& top_left,
					const V2_int& size,
					const Color& color) {
	auto& window_manager{ ptgn::internal::managers::GetManager<ptgn::internal::managers::WindowManager>() };
	const auto& renderer{ window_manager.GetTargetRenderer() };
	renderer.DrawSolidRectangle(top_left, size, color);
}

} // namespace draw

} // namespace ptgn