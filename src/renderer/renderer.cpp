#include "renderer/renderer.h"

#include <functional>

#include "event/event_handler.h"
#include "protegon/color.h"
#include "protegon/events.h"
#include "protegon/game.h"
#include "protegon/matrix4.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "protegon/vertex_array.h"
#include "renderer/batch.h"
#include "renderer/gl_renderer.h"
#include "renderer/origin.h"
#include "renderer/vertices.h"
#include "utility/debug.h"

namespace ptgn::impl {

void Renderer::Init() {
	GLRenderer::SetBlendMode(BlendMode::Blend);
	GLRenderer::EnableLineSmoothing();

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([this](const WindowResizedEvent& e) { SetViewportSize(e.size); })
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

void Renderer::SetViewportSize(const V2_int& viewport_size) {
	PTGN_ASSERT(viewport_size.x > 0 && "Cannot set viewport width below 1");
	PTGN_ASSERT(viewport_size.y > 0 && "Cannot set viewport height below 1");

	viewport_size_ = viewport_size;
}

V2_int Renderer::GetViewportSize() const {
	return viewport_size_;
}

void Renderer::SetViewportScale(const V2_float& viewport_scale) {
	PTGN_ASSERT(viewport_scale.x != 0 && "Cannot set viewport scale x to 0");
	PTGN_ASSERT(viewport_scale.y != 0 && "Cannot set viewport scale y to 0");

	viewport_scale_ = viewport_scale;
}

V2_float Renderer::GetViewportScale() const {
	return viewport_scale_;
}

void Renderer::Clear() const {
	V2_float window_size{ game.window.GetSize() };
	GLRenderer::SetViewport(
		V2_float{ 0.0f, window_size.y - static_cast<float>(viewport_size_.y) * viewport_scale_.y },
		viewport_size_ * viewport_scale_
	);
	GLRenderer::Clear();
}

void Renderer::Present() {
	Flush();

	game.window.SwapBuffers();
}

void Renderer::UpdateLayer(
	std::size_t layer_number, RenderLayer& layer, CameraManager& camera_manager
) {
	if (auto it = camera_manager.primary_cameras_.find(layer_number);
		it != camera_manager.primary_cameras_.end()) {
		layer.view_projection = it->second.GetViewProjection();
	} else {
		// If no camera specified, use screen camera.
		OrthographicCamera c;
		c.SetSizeToWindow(false);
		c.CenterOnWindow(false);
		layer.view_projection = c.GetViewProjection();
	}
	layer.new_view_projection = true;
}

void Renderer::Flush(std::size_t render_layer) {
	auto it = data_.render_layers_.find(render_layer);
	if (it == data_.render_layers_.end()) {
		return;
	}
	auto& layer{ it->second };
	UpdateLayer(render_layer, layer, game.scene.GetTopActive().camera);
	data_.white_texture_.Bind(0);
	data_.FlushLayer(it->second);
}

void Renderer::Flush() {
	auto& camera_manager{ game.scene.GetTopActive().camera };
	for (auto& [layer_number, layer] : data_.render_layers_) {
		UpdateLayer(layer_number, layer, camera_manager);
	}
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

	data_.Texture(
		vertices, texture, tex_coords, info.tint.Normalized(), info.z_index, info.render_layer
	);
}

void Renderer::Point(
	const V2_float& position, const Color& color, float radius, float z_index,
	std::size_t render_layer
) {
	data_.Point(position, color.Normalized(), radius, z_index, render_layer);
}

void Renderer::Line(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width, float z_index,
	std::size_t render_layer
) {
	data_.Line(p0, p1, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Triangle(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	float z_index, std::size_t render_layer
) {
	data_.Triangle(a, b, c, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Rectangle(
	const V2_float& position, const V2_float& size, const Color& color, Origin draw_origin,
	float line_width, float rotation_radians, const V2_float& rotation_center, float z_index,
	std::size_t render_layer
) {
	auto vertices{
		impl::GetQuadVertices(position, size, draw_origin, rotation_radians, rotation_center)
	};
	data_.Rectangle(vertices, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Polygon(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float line_width,
	float z_index, std::size_t render_layer
) {
	data_.Polygon(vertices, vertex_count, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Circle(
	const V2_float& position, float radius, const Color& color, float line_width, float z_index,
	std::size_t render_layer, float fade
) {
	data_.Ellipse(
		position, { radius, radius }, color.Normalized(), line_width, z_index, fade, render_layer
	);
}

void Renderer::RoundedRectangle(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	Origin draw_origin, float line_width, float rotation_radians, const V2_float& rotation_center,
	float z_index, std::size_t render_layer
) {
	data_.RoundedRectangle(
		position, size, radius, color.Normalized(), draw_origin, line_width, rotation_radians,
		rotation_center, z_index, render_layer
	);
}

void Renderer::Ellipse(
	const V2_float& position, const V2_float& radius, const Color& color, float line_width,
	float z_index, std::size_t render_layer, float fade
) {
	data_.Ellipse(position, radius, color.Normalized(), line_width, z_index, fade, render_layer);
}

void Renderer::Arc(
	const V2_float& position, float arc_radius, float start_angle_radians, float end_angle_radians,
	bool clockwise, const Color& color, float line_width, float z_index, std::size_t render_layer
) {
	data_.Arc(
		position, arc_radius, start_angle_radians, end_angle_radians, clockwise, color.Normalized(),
		line_width, z_index, render_layer
	);
}

void Renderer::Capsule(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float line_width,
	float z_index, std::size_t render_layer, float fade
) {
	data_.Capsule(p0, p1, radius, color.Normalized(), line_width, fade, z_index, render_layer);
}

} // namespace ptgn::impl