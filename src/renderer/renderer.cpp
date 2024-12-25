#include "renderer/renderer.h"

#include <functional>
#include <string_view>
#include <variant>

#include "camera/camera.h"
#include "core/window.h"
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
	default_target_ = RenderTexture{ true };
	current_target_ = default_target_;
	PTGN_ASSERT(FrameBuffer::GetBoundId() == 0);
	PTGN_ASSERT(RenderBuffer::GetBoundId() == 0);
	current_target_.Bind();
	data_.Init();
}

void Renderer::Reset() {
	current_target_ = {};
	default_target_ = {};
	data_			= {};
}

void Renderer::Shutdown() {
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

void Renderer::SetTarget(
	const RenderTexture& target, bool draw_previously_bound_target, bool force_draw
) {
	if (current_target_ == target ||
		current_target_ == default_target_ && target == RenderTexture{}) {
		return;
	}
	if (draw_previously_bound_target) {
		current_target_.DrawAndUnbind(force_draw);
	}
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
	current_target_.DrawAndUnbind(true);

	game.window.SwapBuffers();

	current_target_.Bind();
	current_target_.Clear();

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
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
		current_target_.SetCleared(false);
	}
}

void Renderer::FlushImpl(const M4_float& shader_view_projection) {
	auto& camera_manager{ game.scene.GetTopActive().camera };
	data_.white_texture_.Bind(0);
	for (auto& [render_layer, layer] : data_.render_layers_) {
		UpdateLayer(render_layer, layer, camera_manager);
		bool flushed{ data_.FlushLayer(layer, shader_view_projection) };
		if (flushed) {
			current_target_.SetCleared(false);
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
	const std::string_view& text_content, const Color& text_color, const ptgn::Rect& destination,
	const FontOrKey& font, TextInfo text_info, LayerInfo layer_info
) {
	if (!text_info.visible) {
		return;
	}

	Font f;

	if (font == FontOrKey{}) {
		f = game.font.GetDefault();
	} else {
		f = game.font.GetFontOrKey(font);
	}

	PTGN_ASSERT(f.IsValid(), "Cannot draw text with invalid font");

	ptgn::Text temp_text{ text_content,
						  text_color,
						  font,
						  text_info.font_style,
						  text_info.render_mode,
						  text_info.shading_color,
						  text_info.wrap_after_pixels };

	Renderer::Text(temp_text, destination, layer_info);
}

void Renderer::Text(const ptgn::Text& text, ptgn::Rect destination, LayerInfo layer_info) {
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

	if (destination.size.IsZero()) {
		destination.size = text.GetSize();
	}

    Renderer::Texture(texture, destination);
}

void Renderer::Shader(
	ScreenShader screen_shader, const ptgn::Texture& texture, BlendMode blend_mode,
	LayerInfo layer_info
) {
	Shader(
		game.shader.Get(screen_shader), texture, {}, blend_mode, Flip::None, { 0.5f, 0.5f },
		layer_info
	);
}

void Renderer::Shader(
	const ptgn::Shader& shader, const ptgn::Texture& texture, ptgn::Rect destination,
	BlendMode blend_mode, Flip flip, const V2_float& rotation_center, LayerInfo layer_info
) {
	// Fullscreen shader.
	if (destination.size.IsZero()) {
		destination.size	 = game.window.GetSize();
		destination.origin	 = Origin::TopLeft;
		destination.position = {};
	}

	auto tex_coords{ RendererData::GetTextureCoordinates({}, {}, destination.size, flip) };
	// Since this engine uses top left as origin, shaders must all be flipped vertically.
	RendererData::FlipTextureCoordinates(tex_coords, Flip::Vertical);

	ptgn::Texture t{ texture.IsValid() ? texture : current_target_.GetTexture() };

	data_.Shader(
		shader, destination.GetVertices(rotation_center), t, blend_mode, tex_coords,
		layer_info.z_index, layer_info.render_layer
	);
}

void Renderer::Texture(
	const ptgn::Texture& texture, ptgn::Rect destination, TextureInfo texture_info,
	LayerInfo layer_info
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw uninitialized or destroyed texture");

	// Fullscreen texture.
	if (destination.size.IsZero()) {
		destination.size	 = game.window.GetSize();
		destination.origin	 = Origin::TopLeft;
		destination.position = {};
	}

	data_.Texture(
		destination.GetVertices(texture_info.rotation_center), texture,
		RendererData::GetTextureCoordinates(
			texture_info.source_position, texture_info.source_size, texture.GetSize(),
			texture_info.flip
		),
		texture_info.tint.Normalized(), layer_info.z_index, layer_info.render_layer
	);
}

void Renderer::Point(
	const V2_float& position, const Color& color, float radius, LayerInfo layer_info
) {
	data_.Point(
		position, color.Normalized(), radius, layer_info.z_index, layer_info.render_layer,
		default_fade_
	);
}

void Renderer::Points(
	const V2_float* points, std::size_t point_count, const Color& color, float radius,
	LayerInfo layer_info
) {
	for (std::size_t i{ 0 }; i < point_count; ++i) {
		Point(points[i], color, radius, layer_info);
	}
}

void Renderer::Line(
	const V2_float& p0, const V2_float& p1, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Line(p0, p1, color.Normalized(), line_width, layer_info.z_index, layer_info.render_layer);
}

void Renderer::Axis(
	const V2_float& point, const V2_float& direction, const Color& color, float line_width,
	LayerInfo layer_info
) {
	V2_float ws{ game.window.GetSize() };
	float mag{ ws.MagnitudeSquared() };
	// Find line points on the window extents.
	V2_float p0{ point + direction * mag };
	V2_float p1{ point - direction * mag };

	data_.Line(p0, p1, color.Normalized(), line_width, layer_info.z_index, layer_info.render_layer);
}

void Renderer::Triangle(
	const V2_float& a, const V2_float& b, const V2_float& c, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Triangle(
		a, b, c, color.Normalized(), line_width, layer_info.z_index, layer_info.render_layer
	);
}

void Renderer::Rect(
	const ptgn::Rect& rect, const Color& color, float line_width, const V2_float& rotation_center,
	LayerInfo layer_info
) {
	data_.Rect(
		rect.GetVertices(rotation_center), color.Normalized(), line_width, layer_info.z_index,
		layer_info.render_layer
	);
}

void Renderer::Polygon(
	const V2_float* vertices, std::size_t vertex_count, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Polygon(
		vertices, vertex_count, color.Normalized(), line_width, layer_info.z_index,
		layer_info.render_layer
	);
}

void Renderer::Circle(
	const V2_float& position, float radius, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Ellipse(
		position, { radius, radius }, color.Normalized(), line_width, layer_info.z_index,
		layer_info.render_layer, default_fade_
	);
}

void Renderer::RoundedRect(
	const ptgn::Rect& rect, float radius, const Color& color, float line_width,
	const V2_float& rotation_center, LayerInfo layer_info
) {
	data_.RoundedRect(
		rect.position, rect.size, radius, color.Normalized(), rect.origin, line_width,
		rect.rotation, rotation_center, layer_info.z_index, layer_info.render_layer, default_fade_
	);
}

void Renderer::Ellipse(
	const V2_float& position, const V2_float& radius, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Ellipse(
		position, radius, color.Normalized(), line_width, layer_info.z_index,
		layer_info.render_layer, default_fade_
	);
}

void Renderer::Arc(
	const V2_float& position, float arc_radius, float start_angle_radians, float end_angle_radians,
	bool clockwise, const Color& color, float line_width, LayerInfo layer_info
) {
	data_.Arc(
		position, arc_radius, start_angle_radians, end_angle_radians, clockwise, color.Normalized(),
		line_width, layer_info.z_index, layer_info.render_layer, default_fade_
	);
}

void Renderer::Capsule(
	const V2_float& p0, const V2_float& p1, float radius, const Color& color, float line_width,
	LayerInfo layer_info
) {
	data_.Capsule(
		p0, p1, radius, color.Normalized(), line_width, layer_info.z_index, layer_info.render_layer,
		default_fade_
	);
}

Color Renderer::GetPixel(const V2_int& coordinate) const {
	return current_target_.GetFrameBuffer().GetPixel(coordinate);
}

void Renderer::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	return current_target_.GetFrameBuffer().ForEachPixel(func);
}

} // namespace ptgn::impl
