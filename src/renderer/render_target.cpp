#include "renderer/render_target.h"

#include <cstdint>

#include "core/game.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/layer_info.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
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
	GLRenderer::SetBlendMode(blend_mode_);
	GLRenderer::ClearToColor(clear_color_);
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
	GLRenderer::SetBlendMode(blend_mode_);
	GLRenderer::ClearToColor(clear_color_);
}

void RenderTargetInstance::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer_.Bind();
}

void RenderTargetInstance::Clear() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot clear invalid or uninitialized frame buffer");
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before clearing");
	GLRenderer::ClearToColor(clear_color_);
}

void RenderTargetInstance::SetClearColor(const Color& clear_color) {
	PTGN_ASSERT(
		frame_buffer_.IsValid(), "Cannot set clear color of invalid or uninitialized frame buffer"
	);
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before setting clear color");
	PTGN_ASSERT(clear_color_ != clear_color);
	clear_color_ = clear_color;
}

void RenderTargetInstance::SetBlendMode(BlendMode blend_mode) {
	PTGN_ASSERT(
		frame_buffer_.IsValid(), "Cannot set blend mode of invalid or uninitialized frame buffer"
	);
	PTGN_ASSERT(frame_buffer_.IsBound(), "Frame buffer must be bound before setting blend mode");
	PTGN_ASSERT(blend_mode_ != blend_mode);
	blend_mode_ = blend_mode;
	GLRenderer::SetBlendMode(blend_mode_);
}

void RenderTargetInstance::Flush() {
	PTGN_ASSERT(
		frame_buffer_.IsValid(),
		"Cannot flush render target with invalid or uninitialized frame buffer"
	);
	PTGN_ASSERT(
		frame_buffer_.IsBound(),
		"Frame buffer must be bound before flushing the render target batches"
	);
	GLRenderer::SetBlendMode(blend_mode_);

	if (bool new_view_projection{
			render_data_.SetViewProjection(camera_.GetPrimary().GetViewProjection()) };
		new_view_projection) {
		// Post mouse event when camera moves.
		game.event.mouse.Post(MouseEvent::Move, MouseMoveEvent{});
	}

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
	if (i.clear_color_ == clear_color) {
		return;
	}
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
	if (i.blend_mode_ == blend_mode) {
		return;
	}
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

Color RenderTarget::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixel of invalid render target");
	auto texture{ GetTexture() };
	V2_int size{ texture.GetSize() };
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer texture"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer texture"
	);
	auto formats{ impl::GetGLFormats(texture.GetFormat()) };
	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of render target texture with less than 3 RGB components"
	);
	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.components_ * 1 * 1));
	int y{ size.y - 1 - coordinate.y };
	PTGN_ASSERT(y >= 0);
	Bind();
	GLCall(gl::glReadPixels(
		coordinate.x, y, 1, 1, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		static_cast<void*>(v.data())
	));
	return Color{ v[0], v[1], v[2],
				  formats.components_ == 4 ? v[3] : static_cast<std::uint8_t>(255) };
}

void RenderTarget::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixels of invalid render target");
	auto texture{ GetTexture() };
	V2_int size{ texture.GetSize() };
	auto formats{ impl::GetGLFormats(texture.GetFormat()) };
	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of render target texture with less than 3 RGB components"
	);

	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.components_ * size.x * size.y));
	Bind();
	GLCall(gl::glReadPixels(
		0, 0, size.x, size.y, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		static_cast<void*>(v.data())
	));
	for (int j{ 0 }; j < size.y; j++) {
		// Ensure left-to-right and top-to-bottom iteration.
		int row{ (size.y - 1 - j) * size.x * formats.components_ };
		for (int i{ 0 }; i < size.x; i++) {
			int idx{ row + i * formats.components_ };
			PTGN_ASSERT(static_cast<std::size_t>(idx) < v.size());
			Color color{ v[static_cast<std::size_t>(idx)], v[static_cast<std::size_t>(idx + 1)],
						 v[static_cast<std::size_t>(idx + 2)],
						 formats.components_ == 4 ? v[static_cast<std::size_t>(idx + 3)]
												  : static_cast<std::uint8_t>(255) };
			std::invoke(func, V2_int{ i, j }, color);
		}
	}
}

void RenderTarget::DrawToScreen(const std::function<void()>& post_flush) {
	Flush();

	if (post_flush != nullptr) {
		std::invoke(post_flush);
	}

	game.shader.Get(ScreenShader::Default)
		.Draw(
			GetTexture(), Get().destination_, CameraManager::GetWindow().GetViewProjection(), {},
			LayerInfo{ LayerInfo::ScreenLayer{} }
		);

	// Screen target is not cleared after drawing as it needs to be unbound for SDL2 to swap it to
	// the window buffer.
}

void RenderTarget::Draw(const TextureInfo& texture_info, const Shader& shader) {
	Draw(texture_info, {}, shader);
}

void RenderTarget::Draw(
	const TextureInfo& texture_info, const LayerInfo& layer_info, const Shader& shader
) {
	Flush();

	Shader{ shader.IsValid() ? shader : game.shader.Get(ScreenShader::Default) }.Draw(
		GetTexture(), Get().destination_,
		layer_info.GetRenderTarget().GetCamera().GetPrimary().GetViewProjection(), texture_info,
		layer_info
	);

	Clear();
}

CameraManager& RenderTarget::GetCamera() {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

const CameraManager& RenderTarget::GetCamera() const {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

void RenderTarget::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized render target");
	Get().Bind();
}

V2_float RenderTarget::ScreenToTarget(const V2_float& position) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve render target scaling");
	Rect dest{ GetRect() };
	auto p{ GetCamera().GetPrimary() };
	return (p.ScreenToCamera(position) - dest.Min()) * p.GetSize() / dest.size;
}

V2_float RenderTarget::GetMousePosition() const {
	return ScreenToTarget(game.input.GetMousePositionWindow());
}

V2_float RenderTarget::GetMousePositionPrevious() const {
	return ScreenToTarget(game.input.GetMousePositionPreviousWindow());
}

V2_float RenderTarget::GetMouseDifference() const {
	return ScreenToTarget(game.input.GetMouseDifferenceWindow());
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
