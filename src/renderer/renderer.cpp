#include "renderer/renderer.h"

#include <functional>
#include <variant>

#include "camera/camera.h"
#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "frame_buffer.h"
#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/font.h"
#include "renderer/gl_renderer.h"
#include "renderer/origin.h"
#include "renderer/render_texture.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn::impl {

void Renderer::Init() {
	GLRenderer::EnableLineSmoothing();
	GLRenderer::SetBlendMode(blend_mode_);
	current_target_.SetClearColor(color::Transparent);

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([this](const WindowResizedEvent&) { UpdateDefaultFrameBuffer(); })
	);

	UpdateDefaultFrameBuffer();

	data_.Init();
}

void Renderer::Reset() {
	default_target_ = {};
	data_			= {};
}

void Renderer::Shutdown() {
	game.event.window.Unsubscribe(this);
	Reset();
}

Color Renderer::GetClearColor() const {
	return current_target_.GetClearColor();
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (blend_mode_ == blend_mode) {
		return;
	}
	blend_mode_ = blend_mode;
	GLRenderer::SetBlendMode(blend_mode_);
}

void Renderer::SetTarget(const RenderTexture& target) {
	if (current_target_ == target ||
		current_target_ == default_target_ && target == RenderTexture{}) {
		return;
	}
	current_target_.DrawAndUnbind();
	// Set and bind new render target.
	if (target.IsValid()) {
		current_target_ = target;
	} else {
		current_target_ = default_target_;
	}
	current_target_.Bind();
	current_target_.Clear();
}

RenderTexture Renderer::GetTarget() const {
	return current_target_;
}

void Renderer::SetClearColor(const Color& color) {
	current_target_.SetClearColor(color);
}

void Renderer::Clear() {
	FrameBuffer::Unbind();
	GLRenderer::ClearColor(color::Transparent);
	GLRenderer::Clear();
	current_target_.Bind();
	current_target_.Clear();
}

void Renderer::Present() {
	current_target_.DrawAndUnbind();

	game.window.SwapBuffers();

	current_target_.Bind();
	current_target_.Clear();

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
}

void Renderer::UpdateDefaultFrameBuffer() {
	if (current_target_ != default_target_) {
		default_target_ = RenderTexture{ game.window.GetSize(), default_target_.GetClearColor() };
		return;
	}
	default_target_ = RenderTexture{ game.window.GetSize(), current_target_.GetClearColor() };
	current_target_ = default_target_;
	current_target_.Bind();
}

void Renderer::UpdateLayer(
	std::size_t layer_number, RenderLayer& layer, CameraManager& camera_manager
) const {
	if (auto it = camera_manager.primary_cameras_.find(layer_number);
		it != camera_manager.primary_cameras_.end()) {
		layer.view_projection = it->second.GetViewProjection();
	} else {
		// If no camera specified, use window camera.
		layer.view_projection = game.camera.GetWindow().GetViewProjection();
	}
	// TODO: Figure out how to check if view projection has been updated.
	// I tried comparing with previous view projection but that seems to cause strange issues with
	// the camera not following a position. Perhaps due to floating point issues?
	layer.new_view_projection = true;
}

void Renderer::Flush(std::size_t render_layer) {
	FlushImpl(render_layer, M4_float{});
}

void Renderer::Flush() {
	FlushImpl(M4_float{});
}

void Renderer::FlushImpl(std::size_t render_layer, const M4_float& shader_view_projection) {
	auto it = data_.render_layers_.find(render_layer);
	PTGN_ASSERT(
		it != data_.render_layers_.end(),
		"Cannot flush render layer which has not been used/generated by the renderer"
	);
	auto& layer{ it->second };
	auto& camera_manager{ game.scene.GetTopActive().camera };
	data_.white_texture_.Bind(0);
	UpdateLayer(render_layer, layer, camera_manager);
	bool flushed{ data_.FlushLayer(layer, shader_view_projection) };
	if (flushed) {
		current_target_.cleared_ = false;
	}
}

void Renderer::FlushImpl(const M4_float& shader_view_projection) {
	auto& camera_manager{ game.scene.GetTopActive().camera };
	data_.white_texture_.Bind(0);
	for (auto& [render_layer, layer] : data_.render_layers_) {
		UpdateLayer(render_layer, layer, camera_manager);
		bool flushed{ data_.FlushLayer(layer, shader_view_projection) };
		if (flushed) {
			current_target_.cleared_ = false;
		}
	}
}

void Renderer::VertexArray(const ptgn::VertexArray& va) const {
	PTGN_ASSERT(va.IsValid(), "Cannot submit invalid vertex array for rendering");
	PTGN_ASSERT(
		va.HasVertexBuffer(), "Cannot submit vertex array without a set vertex buffer for rendering"
	);
	if (va.HasIndexBuffer()) {
		auto count{ va.GetIndexBuffer().GetCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 indices");
		GLRenderer::DrawElements(va, count);
	} else {
		auto count{ va.GetVertexBuffer().GetCount() };
		PTGN_ASSERT(count > 0, "Cannot draw vertex array with 0 vertices");
		GLRenderer::DrawArrays(va, count);
	}
}

void Renderer::Text(
	const std::string_view& text_content, const V2_float& destination_position,
	const Color& text_color, Origin draw_origin, V2_float destination_size, const FontOrKey& font,
	const TextInfo& info
) {
	if (!info.visible) {
		return;
	}

	Font f;

	if (font == FontOrKey{}) {
		f = game.font.GetDefault();
	} else {
		f = game.font.GetFontOrKey(font);
	}

	PTGN_ASSERT(f.IsValid(), "Cannot draw text with invalid font");

	ptgn::Text temp_text{ text_content,			 text_color,	   font,
						  info.font_style,		 info.render_mode, info.shading_color,
						  info.wrap_after_pixels };

	TextDrawInfo draw_info{ info.rotation, info.flip, info.rotation_center, info.z_index,
							info.render_layer };

	Renderer::Text(temp_text, destination_position, draw_origin, destination_size, draw_info);
}

void Renderer::Text(
	const ptgn::Text& text, const V2_float& destination_position, Origin draw_origin,
	V2_float destination_size, const TextDrawInfo& info
) {
	if (!text.IsValid()) {
		return;
	}
	if (!text.GetVisibility()) {
		return;
	}
	if (text.GetContent().empty()) {
		return;
	}

	const ptgn::Texture& texture{ text.GetTexture() };

	if (!texture.IsValid()) {
		return;
	}

	if (destination_size.IsZero()) {
		destination_size = text.GetSize();
	}

	game.draw.Texture(
		texture, destination_position, destination_size,
		{ {},
		  {},
		  draw_origin,
		  info.flip,
		  info.rotation,
		  info.rotation_center,
		  info.z_index,
		  color::White,
		  info.render_layer }
	);
}

void Renderer::Shader(
	ScreenShader screen_shader, const ptgn::Texture& texture, BlendMode blend_mode, float z_index,
	std::size_t render_layer
) {
	Shader(
		game.shader.Get(screen_shader), texture, {}, {}, Origin::Center, blend_mode, Flip::None,
		0.0f, { 0.5f, 0.5f }, z_index, render_layer
	);
}

void Renderer::Shader(
	const ptgn::Shader& shader, const ptgn::Texture& texture, const V2_float& position,
	V2_float size, Origin draw_origin, BlendMode blend_mode, Flip flip, float rotation_radians,
	const V2_float& rotation_center, float z_index, std::size_t render_layer
) {
	// Fullscreen shader.
	if (size.IsZero()) {
		size		= game.window.GetSize();
		draw_origin = Origin::TopLeft;
	}

	ptgn::Rect rect{ position, size, draw_origin, rotation_radians };

	auto tex_coords{ RendererData::GetTextureCoordinates({}, {}, rect.size, flip) };
	// Since this engine uses top left as origin, shaders must all be flipped vertically.
	RendererData::FlipTextureCoordinates(tex_coords, Flip::Vertical);

	ptgn::Texture t{ texture.IsValid() ? texture : current_target_.GetTexture() };

	data_.Shader(
		shader, rect.GetVertices(rotation_center), t, blend_mode, tex_coords, z_index, render_layer
	);
}

void Renderer::Texture(
	const ptgn::Texture& texture, const V2_float& position, const V2_float& size,
	const TextureInfo& info
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw uninitialized or destroyed texture");

	data_.Texture(
		ptgn::Rect(position, size, info.source.origin, info.rotation)
			.GetVertices(info.rotation_center),
		texture,
		RendererData::GetTextureCoordinates(
			info.source.position, info.source.size, texture.GetSize(), info.flip
		),
		info.tint.Normalized(), info.z_index, info.render_layer
	);
}

void Renderer::Point(
	const V2_float& position, const Color& color, float radius, float z_index,
	std::size_t render_layer
) {
	data_.Point(position, color.Normalized(), radius, z_index, render_layer);
}

void Renderer::Points(
	const V2_float* points, std::size_t point_count, const Color& color, float radius,
	float z_index, std::size_t render_layer
) {
	for (std::size_t i{ 0 }; i < point_count; ++i) {
		Point(points[i], color, radius, z_index, render_layer);
	}
}

void Renderer::Line(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width, float z_index,
	std::size_t render_layer
) {
	data_.Line(p0, p1, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Axis(
	const V2_float& point, const V2_float& direction, const Color& color, float line_width,
	float z_index, std::size_t render_layer
) {
	V2_float ws{ game.window.GetSize() };
	float mag{ ws.MagnitudeSquared() };
	// Find line points on the window extents.
	V2_float p0{ point + direction * mag };
	V2_float p1{ point - direction * mag };

	data_.Line(p0, p1, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Triangle(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	float z_index, std::size_t render_layer
) {
	data_.Triangle(a, b, c, color.Normalized(), line_width, z_index, render_layer);
}

void Renderer::Rect(
	const V2_float& position, const V2_float& size, const Color& color, Origin draw_origin,
	float line_width, float rotation_radians, const V2_float& rotation_center, float z_index,
	std::size_t render_layer
) {
	ptgn::Rect rect{ position, size, draw_origin, rotation_radians };

	data_.Rect(
		rect.GetVertices(rotation_center), color.Normalized(), line_width, z_index, render_layer
	);
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

void Renderer::RoundedRect(
	const V2_float& position, const V2_float& size, float radius, const Color& color,
	Origin draw_origin, float line_width, float rotation_radians, const V2_float& rotation_center,
	float z_index, std::size_t render_layer
) {
	data_.RoundedRect(
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