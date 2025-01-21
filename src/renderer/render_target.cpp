#include "renderer/render_target.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

RenderTargetInstance::RenderTargetInstance(const Color& clear_color) :
	texture_{ Texture::WindowTexture{} },
	frame_buffer_{ texture_,
				   false /* do not rebind previous frame buffer because it will be cleared */ },
	clear_color_{ clear_color } {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Failed to bind frame buffer when initializing render target"
	);
	GLRenderer::ClearToColor(clear_color_);
}

RenderTargetInstance::RenderTargetInstance(const V2_float& size, const Color& clear_color) :
	texture_{ size },
	frame_buffer_{
		texture_, false /* do not rebind previous frame buffer because it will be cleared */
	},
	clear_color_{ clear_color } {
	// TODO: Consider if this is correct or not.
	camera_.CenterOnArea(size);
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Failed to bind frame buffer when initializing render target"
	);
	GLRenderer::ClearToColor(clear_color_);
}

void RenderTargetInstance::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer_.Bind();
}

} // namespace impl

RenderTarget::RenderTarget(const Color& clear_color) {
	Create(clear_color);
}

RenderTarget::RenderTarget(const V2_float& size, const Color& clear_color) {
	Create(size, clear_color);
}

void RenderTarget::SetViewport(Rect viewport) {
	PTGN_ASSERT(IsValid(), "Cannot set viewport of invalid or uninitialized render target");
	if (viewport.IsZero()) {
		viewport = Rect{ {}, game.window.GetSize(), Origin::TopLeft };
	} else if (viewport.size.IsZero()) {
		viewport = Rect{ viewport.position, Get().texture_.GetSize(), viewport.origin };
	}
	Get().viewport_ = viewport;
}

Rect RenderTarget::GetViewport() const {
	PTGN_ASSERT(IsValid(), "Cannot get viewport of invalid or uninitialized render target");
	auto& i{ Get() };
	if (i.viewport_.IsZero()) {
		return { {}, game.window.GetSize(), Origin::TopLeft };
	} else if (i.viewport_.size.IsZero()) {
		return { i.viewport_.position, i.texture_.GetSize(), i.viewport_.origin };
	}
	return i.viewport_;
}

const Texture& RenderTarget::GetTexture() const {
	PTGN_ASSERT(IsValid(), "Cannot get texture of invalid or uninitialized render target");
	return Get().texture_;
}

Color RenderTarget::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(IsValid(), "Cannot retrieve pixel of invalid render target");
	const auto& texture{ GetTexture() };
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
	const auto& texture{ GetTexture() };
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

V2_float RenderTarget::ScreenToTarget(const V2_float& screen_coordinate) const {
	PTGN_ASSERT(
		IsValid(), "Cannot get coordinate relative to invalid or uninitialized render target"
	);
	auto& i{ Get() };
	return ScreenToViewport(i.viewport_, i.camera_, screen_coordinate);
}

void RenderTarget::SetCamera(const Camera& camera) {
	PTGN_ASSERT(IsValid(), "Cannot set camera of invalid or uninitialized render target");
	auto& i{ Get() };
	i.camera_ = camera;
}

const Camera& RenderTarget::GetCamera() const {
	PTGN_ASSERT(IsValid(), "Cannot get camera of invalid or uninitialized render target");
	return Get().camera_;
}

void RenderTarget::Draw(const TextureInfo& texture_info, Shader shader) const {
	PTGN_ASSERT(IsValid(), "Cannot draw invalid or uninitialized render target");
	PTGN_ASSERT(
		game.renderer.GetRenderTarget() != *this ||
			game.renderer.GetRenderTarget() == game.renderer.screen_target_,
		"Cannot draw a render target to itself"
	);

	const auto& i{ Get() };

	if (i.viewport_.IsZero()) {
		GLRenderer::SetViewport({}, game.window.GetSize());
	} else if (i.viewport_.size.IsZero()) {
		Rect r{ i.viewport_.position, i.texture_.GetSize(), i.viewport_.origin };
		GLRenderer::SetViewport(r.Min(), r.size);
	}

	GetTexture().Draw(i.viewport_, texture_info, shader, i.camera_);

	Bind();
	// Render target is cleared after drawing.
	GLRenderer::ClearToColor(GetClearColor());
}

Color RenderTarget::GetClearColor() const {
	PTGN_ASSERT(IsValid(), "Cannot get clear color of invalid or uninitialized render target");
	return Get().clear_color_;
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	PTGN_ASSERT(IsValid(), "Cannot set clear color of invalid or uninitialized render target");
	Get().clear_color_ = clear_color;
}

void RenderTarget::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind invalid or uninitialized render target");
	Get().Bind();
}

} // namespace ptgn
