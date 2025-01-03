#include "renderer/render_target.h"

#include <cstdint>

#include "core/game.h"
#include "core/window.h"
#include "event/input_handler.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/gl_renderer.h"
#include "renderer/layer_info.h"
#include "renderer/render_data.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

RenderTargetInstance::RenderTargetInstance(const Color& clear_color, BlendMode blend_mode) :
	texture_{ Texture::WindowTexture{} },
	frame_buffer_{ texture_,
				   false /* do not rebind previous frame buffer because it will be cleared */ },
	blend_mode_{ blend_mode },
	clear_color_{ clear_color } {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Failed to bind frame buffer when initializing render target"
	);
	GLRenderer::SetBlendMode(blend_mode);
	GLRenderer::ClearColor(clear_color_);
	GLRenderer::Clear();
}

RenderTargetInstance::RenderTargetInstance(
	const V2_float& size, const Color& clear_color, BlendMode blend_mode
) :
	texture_{ size },
	frame_buffer_{
		texture_, false /* do not rebind previous frame buffer because it will be cleared */
	},
	blend_mode_{ blend_mode },
	clear_color_{ clear_color } {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Failed to bind frame buffer when initializing render target"
	);
	GLRenderer::SetBlendMode(blend_mode);
	GLRenderer::ClearColor(clear_color_);
	GLRenderer::Clear();
}

void RenderTargetInstance::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer_.Bind();
}

void RenderTargetInstance::Clear() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot clear invalid or uninitialized frame buffer");
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before clearing");
	GLRenderer::Clear();
}

void RenderTargetInstance::SetClearColor(const Color& clear_color) {
	PTGN_ASSERT(
		frame_buffer_.IsValid(), "Cannot set clear color of invalid or uninitialized frame buffer"
	);
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before setting clear color");
	if (clear_color_ == clear_color) {
		return;
	}
	clear_color_ = clear_color;
	GLRenderer::ClearColor(clear_color_);
}

void RenderTargetInstance::SetBlendMode(BlendMode blend_mode) {
	PTGN_ASSERT(
		frame_buffer_.IsValid(), "Cannot set blend mode of invalid or uninitialized frame buffer"
	);
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before setting blend mode");
	if (blend_mode_ == blend_mode) {
		return;
	}
	blend_mode_ = blend_mode;
	GLRenderer::SetBlendMode(blend_mode_);
}

void RenderTargetInstance::Flush() {
	render_data_.SetViewProjection(camera_.GetPrimary().GetViewProjection());

	render_data_.Flush();
}

} // namespace impl

RenderTarget::RenderTarget(const Color& clear_color, BlendMode blend_mode) {
	Create(clear_color, blend_mode);
}

RenderTarget::RenderTarget(const V2_float& size, const Color& clear_color, BlendMode blend_mode) {
	Create(size, clear_color, blend_mode);
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	PTGN_ASSERT(IsValid(), "Cannot set clear color of invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.SetClearColor(clear_color);
}

Color RenderTarget::GetClearColor() const {
	PTGN_ASSERT(IsValid(), "Cannot get clear color of invalid or uninitialized render target");
	return Get().clear_color_;
}

void RenderTarget::SetBlendMode(BlendMode blend_mode) {
	PTGN_ASSERT(IsValid(), "Cannot set blend mode of invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.SetBlendMode(blend_mode);
}

BlendMode RenderTarget::GetBlendMode() const {
	PTGN_ASSERT(IsValid(), "Cannot get blend mode of invalid or uninitialized render target");
	return Get().blend_mode_;
}

void RenderTarget::Flush() {
	PTGN_ASSERT(IsValid(), "Cannot flush invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.Flush();
}

void RenderTarget::Clear() const {
	PTGN_ASSERT(IsValid(), "Cannot clear invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.Clear();
}

Texture RenderTarget::GetTexture() const {
	PTGN_ASSERT(IsValid(), "Cannot get texture of invalid or uninitialized render target");
	return Get().texture_;
}

void RenderTarget::Draw(const Rect& destination, const TextureInfo& texture_info) {
	Draw(destination, texture_info, {});
}

void RenderTarget::Draw(
	const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info
) {
	PTGN_ASSERT(IsValid(), "Cannot draw an invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.Flush();
	DrawToTarget(destination, texture_info, layer_info.GetRenderTarget());
}

CameraManager& RenderTarget::GetCamera() {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

const CameraManager& RenderTarget::GetCamera() const {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

RenderTarget RenderTarget::GetCorrectRenderLayer(const RenderTarget& render_target) {
	if (render_target.IsValid()) {
		return render_target;
	}
	RenderTarget scene_target{ game.scene.GetCurrent().GetRenderTarget() };
	PTGN_ASSERT(scene_target.IsValid(), "Scene render target is invalid or uninitialized");
	return scene_target;
}

void RenderTarget::DrawToTarget(
	const Rect& destination, const TextureInfo& texture_info, RenderTarget destination_target
) const {
	auto destination_layer{ GetCorrectRenderLayer(destination_target) };
	auto& i{ destination_layer.Get() };
	i.Bind();
	i.Flush();
	GetTexture().Draw(destination, texture_info, LayerInfo{ destination_layer });
	i.Flush();
}

void RenderTarget::Bind() {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized render target");
	Get().Bind();
}

void RenderTarget::AddTexture(
	const Texture& texture, const Rect& destination, const TextureInfo& texture_info,
	std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };

	auto vertices{ destination.GetVertices(texture_info.rotation_center) };

	auto tex_coords{ texture_info.GetTextureCoordinates(destination.size) };

	// Since this engine uses top left as origin, shaders must all be flipped vertically.
	TextureInfo::FlipTextureCoordinates(tex_coords, Flip::Vertical);

	TextureInfo::FlipTextureCoordinates(tex_coords, texture_info.flip);

	i.render_data_.AddTexture(
		vertices, texture, tex_coords, texture_info.tint.Normalized(), render_layer
	);
}

void RenderTarget::AddEllipse(
	const Ellipse& ellipse, const Color& color, float line_width, float fade,
	std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddEllipse(ellipse, color.Normalized(), line_width, fade, render_layer);
}

void RenderTarget::AddCircle(
	const Circle& circle, const Color& color, float line_width, float fade,
	std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddCircle(circle, color.Normalized(), line_width, fade, render_layer);
}

void RenderTarget::AddLine(
	const Line& line, const Color& color, float line_width, std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddLine(line, color.Normalized(), line_width, render_layer);
}

void RenderTarget::AddPoint(
	const V2_float& point, const Color& color, float radius, float fade, std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddPoint(point, color.Normalized(), radius, fade, render_layer);
}

void RenderTarget::AddTriangle(
	const Triangle& triangle, const Color& color, float line_width, std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddTriangle(triangle, color.Normalized(), line_width, render_layer);
}

void RenderTarget::AddRect(
	const Rect& rect, const Color& color, float line_width, std::int32_t render_layer,
	const V2_float& rotation_center
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };

	auto vertices{ rect.GetVertices(rotation_center) };

	i.render_data_.AddRect(vertices, color.Normalized(), line_width, render_layer);
}

void RenderTarget::AddRoundedRect(
	const RoundedRect& rrect, const Color& color, float line_width, float fade,
	std::int32_t render_layer, const V2_float& rotation_center
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddRoundedRect(
		rrect, color.Normalized(), line_width, rotation_center, fade, render_layer
	);
}

void RenderTarget::AddArc(
	const Arc& arc, bool clockwise, const Color& color, float line_width, float fade,
	std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddArc(arc, clockwise, color.Normalized(), line_width, fade, render_layer);
}

void RenderTarget::AddCapsule(
	const Capsule& capsule, const Color& color, float line_width, float fade,
	std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddCapsule(capsule, color.Normalized(), line_width, fade, render_layer);
}

void RenderTarget::AddPolygon(
	const Polygon& polygon, const Color& color, float line_width, std::int32_t render_layer
) {
	auto& i{ GetCorrectRenderLayer(*this).Get() };
	i.render_data_.AddPolygon(polygon, color.Normalized(), line_width, render_layer);
}

V2_float RenderTarget::ScaleToWindow(const V2_float& position) const {
	V2_float window_size{ game.window.GetSize() };
	PTGN_ASSERT(
		window_size.x != 0 && window_size.y != 0,
		"Cannot scale position relative to a dimensionless window"
	);
	V2_float scaled_position{ GetCamera().GetPrimary().GetSize() * position / window_size };
	return scaled_position;
}

V2_float RenderTarget::WorldToScreen(const V2_float& position) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return (position - primary.GetPosition()) * scale + primary.GetSize() / 2.0f;
}

V2_float RenderTarget::ScreenToWorld(const V2_float& position) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return (position - primary.GetSize() * 0.5f) / scale + primary.GetPosition();
}

V2_float RenderTarget::ScaleToWorld(const V2_float& size) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return size / scale;
}

float RenderTarget::ScaleToWorld(float size) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return size / scale;
}

V2_float RenderTarget::ScaleToScreen(const V2_float& size) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return size * scale;
}

float RenderTarget::ScaleToScreen(float size) const {
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return size * scale;
}

V2_float RenderTarget::GetMousePosition() const {
	return ScaleToWindow(game.input.GetMousePositionWindow());
}

V2_float RenderTarget::GetMousePositionPrevious() const {
	return ScaleToWindow(game.input.GetMousePositionPreviousWindow());
}

V2_float RenderTarget::GetMouseDifference() const {
	return ScaleToWindow(game.input.GetMouseDifferenceWindow());
}

} // namespace ptgn
