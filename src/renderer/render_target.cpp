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
	OrthographicCamera camera;
	camera.CenterOnArea(size);
	camera_.SetPrimary(camera);
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
	GLRenderer::ClearColor(clear_color_);
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
}

void RenderTargetInstance::Flush() {
	GLRenderer::SetBlendMode(blend_mode_);

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

void RenderTarget::SetRect(const Rect& target_destination) {
	PTGN_ASSERT(IsValid(), "Cannot set rectangle of invalid or uninitialized render target");
	auto& i{ Get() };
	i.destination_ = target_destination;
}

Rect RenderTarget::GetRect() const {
	PTGN_ASSERT(IsValid(), "Cannot get rectangle of invalid or uninitialized render target");
	auto& i{ Get() };
	Rect dest{ i.destination_ };
	if (dest.IsZero()) {
		dest = Rect::Fullscreen();
	}
	return dest;
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

void RenderTarget::Draw(const TextureInfo& texture_info) {
	Draw(texture_info, {});
}

void RenderTarget::Draw(const TextureInfo& texture_info, const LayerInfo& layer_info) {
	PTGN_ASSERT(IsValid(), "Cannot draw an invalid or uninitialized render target");
	auto& i{ Get() };
	i.Bind();
	i.Flush();
	DrawToTarget(i.destination_, texture_info, layer_info);
	i.Bind();
	i.Clear();
}

CameraManager& RenderTarget::GetCamera() {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

const CameraManager& RenderTarget::GetCamera() const {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

void RenderTarget::DrawToTarget(
	const Rect& destination, const TextureInfo& texture_info, const LayerInfo& layer_info
) const {
	auto destination_target{ layer_info.GetRenderTarget() };
	auto& i{ destination_target.Get() };
	i.Bind();
	i.Flush();
	TextureInfo info{ texture_info };
	if (info.flip == Flip::Vertical) {
		info.flip = Flip::None;
	} else if (info.flip == Flip::Both) {
		info.flip = Flip::Horizontal;
	} else if (info.flip == Flip::None) {
		info.flip = Flip::Vertical;
	}
	// TODO: Change this to use a shader draw directly.
	GetTexture().Draw(destination, info, LayerInfo{ destination_target });
	i.Flush();
}

void RenderTarget::Bind() {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized render target");
	Get().Bind();
}

V2_float RenderTarget::ScaleToTarget(const V2_float& position) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve render target scaling");
	auto& i{ Get() };
	Rect dest{ i.destination_ };
	if (dest.IsZero()) {
		dest = Rect::Fullscreen();
	}
	auto primary{ GetCamera().GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	V2_float screen_pos{ ScreenToWorld(position) };
	V2_float target_scale{ dest.size / i.texture_.GetSize() };
	V2_float target_pos{ (screen_pos - dest.Min()) / target_scale };
	return target_pos;
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
	return ScaleToTarget(game.input.GetMousePositionWindow());
}

V2_float RenderTarget::GetMousePositionPrevious() const {
	return ScaleToTarget(game.input.GetMousePositionPreviousWindow());
}

V2_float RenderTarget::GetMouseDifference() const {
	return ScaleToTarget(game.input.GetMouseDifferenceWindow());
}

impl::RenderData& RenderTarget::GetRenderData() {
	PTGN_ASSERT(
		IsValid(), "Cannot retrieve render data for an invalid or uninitialized render target"
	);
	return Get().render_data_;
}

const impl::RenderData& RenderTarget::GetRenderData() const {
	PTGN_ASSERT(
		IsValid(), "Cannot retrieve render data for an invalid or uninitialized render target"
	);
	return Get().render_data_;
}

} // namespace ptgn
