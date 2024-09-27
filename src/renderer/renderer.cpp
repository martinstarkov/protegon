#include "renderer/renderer.h"

#include "protegon/game.h"
#include "renderer/gl_renderer.h"

namespace ptgn::impl {

void Renderer::Init() {
	GLRenderer::SetBlendMode(BlendMode::Blend);
	GLRenderer::EnableLineSmoothing();

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([this](const WindowResizedEvent& e) { SetViewport(e.size); })
	);

	data_.Init();
}

void Renderer::Reset() {
	clear_color_   = color::White;
	blend_mode_	   = BlendMode::Blend;
	viewport_size_ = {};
	data_		   = {};
}

void Renderer::Shutdown() {
	game.event.window.Unsubscribe(this);
	Reset();
}

Color Renderer::GetClearColor() const {
	return clear_color_;
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

void Renderer::SetBlendMode(BlendMode mode) {
	if (blend_mode_ == mode) {
		return;
	}
	blend_mode_ = mode;
	GLRenderer::SetBlendMode(mode);
}

void Renderer::SetClearColor(const Color& color) {
	if (clear_color_ == color) {
		return;
	}
	clear_color_ = color;
	GLRenderer::SetClearColor(color);
}

void Renderer::Clear() const {
	GLRenderer::Clear();
}

void Renderer::Present() {
	Flush();

	game.window.SwapBuffers();
}

void Renderer::UpdateViewProjection(const M4_float& view_projection) {
	data_.view_projection_	   = view_projection;
	data_.new_view_projection_ = true;
}

void Renderer::SetViewport(const V2_int& size) {
	PTGN_ASSERT(size.x > 0 && "Cannot set viewport width below 1");
	PTGN_ASSERT(size.y > 0 && "Cannot set viewport height below 1");

	if (viewport_size_ == size) {
		return;
	}

	viewport_size_ = size;
	GLRenderer::SetViewport({}, viewport_size_);
}

void Renderer::Flush() {
	UpdateViewProjection(game.scene.GetTopActive().camera.GetViewProjection());
	data_.Flush();
}

void Renderer::VertexElements(const ptgn::VertexArray& va, std::size_t index_count) const {
	PTGN_ASSERT(va.IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		va.HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	PTGN_ASSERT(
		va.HasIndexBuffer(), "Cannot submit vertex array without a set index buffer for rendering"
	);
	GLRenderer::DrawElements(va, index_count);
}

void Renderer::VertexArray(const ptgn::VertexArray& va, std::size_t vertex_count) const {
	PTGN_ASSERT(va.IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		va.HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	GLRenderer::DrawArrays(va, vertex_count);
}

void Renderer::Texture(
	const ptgn::Texture& texture, const V2_float& position, const V2_float& size,
	const TextureInfo& info
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw uninitialized or destroyed texture");

	auto tex_coords{ RendererData::GetTextureCoordinates(
		info.source.pos, info.source.size, texture.GetSize(), info.flip
	) };

	auto vertices =
		GetQuadVertices(position, size, info.source.origin, info.rotation, info.rotation_center);

	data_.Texture(vertices, texture, tex_coords, info.tint.Normalized(), info.z_index);
}

void Renderer::Point(const V2_float& position, const Color& color, float radius, float z_index) {
	data_.Point(position, color.Normalized(), radius, z_index);
}

void Renderer::Line(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width, float z_index
) {
	data_.Line(p0, p1, color.Normalized(), line_width, z_index);
}

void Renderer::Triangle(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	float z_index
) {
	data_.Triangle(a, b, c, color.Normalized(), line_width, z_index);
}

void Renderer::Rectangle(
	const V2_float& position, const V2_float& size, const Color& color, Origin draw_origin,
	float line_width, float rotation_radians, const V2_float& rotation_center, float z_index
) {
	auto vertices{
		impl::GetQuadVertices(position, size, draw_origin, rotation_radians, rotation_center)
	};
	data_.Rectangle(vertices, color.Normalized(), line_width, z_index);
}

void Renderer::Polygon(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float line_width,
	float z_index
) {
	data_.Polygon(vertices, vertex_count, color.Normalized(), line_width, z_index);
}

void Renderer::Circle(
	const V2_float& position, float radius, const Color& color, float line_width, float z_index,
	float fade
) {
	data_.Ellipse(position, { radius, radius }, color.Normalized(), line_width, z_index, fade);
}

void Renderer::RoundedRectangle(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	Origin draw_origin, float line_width, float rotation_radians, const V2_float& rotation_center,
	float z_index
) {
	data_.RoundedRectangle(
		position, size, radius, color.Normalized(), draw_origin, line_width, rotation_radians,
		rotation_center, z_index
	);
}

void Renderer::Ellipse(
	const V2_float& position, const V2_float& radius, const Color& color, float line_width,
	float z_index, float fade
) {
	data_.Ellipse(position, radius, color.Normalized(), line_width, z_index, fade);
}

void Renderer::Arc(
	const V2_float& position, float arc_radius, float start_angle_radians, float end_angle_radians,
	bool clockwise, const Color& color, float line_width, float z_index
) {
	data_.Arc(
		position, arc_radius, start_angle_radians, end_angle_radians, clockwise, color.Normalized(),
		line_width, z_index
	);
}

void Renderer::Capsule(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float line_width,
	float z_index, float fade
) {
	data_.Capsule(p0, p1, radius, color.Normalized(), line_width, fade, z_index);
}

} // namespace ptgn::impl