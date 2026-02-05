#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_handle.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/resources/vertex.h"

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

	// TODO: Move to private.
	void FrameStart();
	// TODO: Move to private.
	void Present();

	void BindRenderTarget(
		impl::gl::StrongGLHandle<impl::gl::GLResource::FrameBuffer> framebuffer,
		const impl::gl::Viewport& viewport
	);

	void DrawTexture(
		impl::gl::StrongGLHandle<impl::gl::GLResource::Texture> texture, V2_float center,
		V2_float size
	);

	void SetShader(const impl::gl::StrongGLHandle<impl::gl::GLResource::Shader>& shader);
	void SetBlend(bool enable, BlendMode mode);
	void SetFramebuffer(
		impl::gl::StrongGLHandle<impl::gl::GLResource::FrameBuffer> fb,
		const impl::gl::Viewport& viewport
	);
	void SetDepth(bool test, bool write, GLenum func);
	void SetStencil(
		bool enable, GLenum func, GLint ref, GLuint mask, GLenum fail, GLenum zfail, GLenum zpass,
		GLuint write_mask
	);
	void SetRaster(
		bool cull, GLenum cull_mode, GLenum front_face, GLenum polygon_front_mode,
		GLenum polygon_back_mode
	);
	void SetColorMask(bool r, bool g, bool b, bool a);

	// TODO: Move to private.
	impl::gl::StrongGLHandle<impl::gl::FrameBuffer> screen_fbo;

	// TODO: Move to private.
	void FlushBatch();

private:
	friend class Application;

	std::uint32_t GetTextureSlot(impl::gl::StrongGLHandle<impl::gl::GLResource::Texture> tex);

	struct RenderState {
		// Shader
		impl::gl::StrongGLHandle<impl::gl::GLResource::Shader> shader;

		// Framebuffer
		impl::gl::StrongGLHandle<impl::gl::GLResource::FrameBuffer> framebuffer;

		// Blending
		bool blend_enable	 = false;
		BlendMode blend_mode = BlendMode::ReplaceRGBA;

		// Depth
		bool depth_test	  = false;
		bool depth_write  = false;
		GLenum depth_func = GL_LESS;

		// Stencil
		bool stencil_test	 = false;
		GLenum stencil_func	 = GL_ALWAYS;
		GLint stencil_ref	 = 0;
		GLuint stencil_mask	 = 0xFF;
		GLenum stencil_fail	 = GL_KEEP;
		GLenum stencil_zfail = GL_KEEP;
		GLenum stencil_zpass = GL_KEEP;
		GLuint stencil_write_mask{ 0xFFFFFFFF };

		// Raster
		bool cull_face			  = false;
		GLenum cull_mode		  = GL_BACK;
		GLenum front_face		  = GL_CCW;
		GLenum polygon_front_mode = GL_FILL;
		GLenum polygon_back_mode  = GL_FILL;

		// Color mask
		bool color_write_r = true;
		bool color_write_g = true;
		bool color_write_b = true;
		bool color_write_a = true;

		bool valid = false;
	};

	RenderState state;

	static constexpr std::uint32_t MaxQuads	   = 1024;
	static constexpr std::uint32_t MaxVertices = MaxQuads * 4;
	static constexpr std::uint32_t MaxIndices  = MaxQuads * 6;

	std::vector<impl::Vertex> batch_vertices;
	std::vector<impl::Index> batch_indices;

	std::vector<impl::gl::StrongGLHandle<impl::gl::GLResource::Texture>> batch_textures;

	Window& window_;

	impl::gl::StrongGLHandle<impl::gl::VertexBuffer> vbo;
	impl::gl::StrongGLHandle<impl::gl::ElementBuffer> ebo;
	impl::gl::StrongGLHandle<impl::gl::VertexArray> vao;
	impl::gl::StrongGLHandle<impl::gl::Texture> white_texture;

	impl::gl::StrongGLHandle<impl::gl::Texture> screen_texture;
};

} // namespace ptgn