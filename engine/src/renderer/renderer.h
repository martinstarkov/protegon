#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_handle.h"

namespace ptgn {

class Application;
class Window;

namespace impl {

namespace gl {

class GLContext;

} // namespace gl

using Index = std::uint32_t;

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

[[nodiscard]] static constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	return {
		V2_float{ 0.0f, 0.0f },
		V2_float{ 1.0f, 0.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ 0.0f, 1.0f },
	};
}

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size,
	bool offset_texels = false
);

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

} // namespace impl

class Renderer {
public:
	Renderer() = delete;
	Renderer(Window& window);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	// TODO: Move to private.
	std::unique_ptr<impl::gl::GLContext> gl_;

	// Move to private.
	void FrameStart();
	// Move to private.
	void Present();

private:
	friend class Application;

	Window& window_;

	impl::gl::StrongGLHandle<impl::gl::VertexBuffer> vbo;
	impl::gl::StrongGLHandle<impl::gl::ElementBuffer> ebo;
	impl::gl::StrongGLHandle<impl::gl::VertexArray> vao;
	impl::gl::StrongGLHandle<impl::gl::Texture> white_texture;

	impl::gl::StrongGLHandle<impl::gl::Texture> screen_texture;
	impl::gl::StrongGLHandle<impl::gl::FrameBuffer> screen_fbo;
};

} // namespace ptgn