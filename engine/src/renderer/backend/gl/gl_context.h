#pragma once

#include <cstdint>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"

#ifdef __EMSCRIPTEN__

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 0;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 3;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

#define PTGN_IMPL_BLEND_CASE(name, srcRGB, dstRGB, srcA, dstA) \
	case BlendMode::name: GLCall(BlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)); break;

struct SDL_GLContextState;

namespace ptgn {

class Window;

} // namespace ptgn

namespace ptgn::impl::gl {

class GLContext;

template <typename T>
class BindGuard {
public:
	BindGuard(GLContext& gl, T id, bool restore_bind) :
		gl_{ gl }, id_{ id }, restore_bind_{ restore_bind } {}

	~BindGuard() noexcept;

	BindGuard(BindGuard&&) noexcept			   = delete;
	BindGuard& operator=(BindGuard&&) noexcept = delete;
	BindGuard(const BindGuard&)				   = delete;
	BindGuard& operator=(const BindGuard&)	   = delete;

private:
	GLContext& gl_;
	T id_;
	bool restore_bind_{ false };
};

class GLContext {
public:
	GLContext() = delete;
	explicit GLContext(const Window& window);
	~GLContext() noexcept;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	[[nodiscard]] BindGuard<VertexBuffer> Bind(VertexBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ElementBuffer> Bind(ElementBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<UniformBuffer> Bind(UniformBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Program> Bind(Program id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Texture> Bind(Texture id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Renderbuffer> Bind(Renderbuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Framebuffer> Bind(Framebuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<VertexArray> Bind(VertexArray id, bool restore_bind = false);

	[[nodiscard]] const State& GetBoundState() const;
	[[nodiscard]] State& GetBoundState();
	[[nodiscard]] VertexBuffer GetBoundVertexBuffer() const;
	[[nodiscard]] ElementBuffer GetBoundElementBuffer() const;
	[[nodiscard]] UniformBuffer GetBoundUniformBuffer() const;
	[[nodiscard]] Program GetBoundProgram() const;
	[[nodiscard]] Texture GetBoundTexture() const;
	[[nodiscard]] Renderbuffer GetBoundRenderbuffer() const;
	[[nodiscard]] Framebuffer GetBoundFramebuffer() const;
	[[nodiscard]] VertexArray GetBoundVertexArray() const;

	[[nodiscard]] bool IsBound(VertexBuffer id) const;
	[[nodiscard]] bool IsBound(ElementBuffer id) const;
	[[nodiscard]] bool IsBound(UniformBuffer id) const;
	[[nodiscard]] bool IsBound(Program id) const;
	[[nodiscard]] bool IsBound(Texture id) const;
	[[nodiscard]] bool IsBound(Renderbuffer id) const;
	[[nodiscard]] bool IsBound(Framebuffer id) const;
	[[nodiscard]] bool IsBound(VertexArray id) const;

	void EnableGammaCorrection() const;
	void DisableGammaCorrection() const;

	// Enabling blending will disable depth testing.
	void SetBlending(bool enabled);
	void SetBlend(const BlendState& blend_state);
	// Will disable depth testing.
	void SetBlendMode(BlendMode mode);

	// Enabling depth testing will disable blending.
	void SetDepthTesting(bool enabled);

	void SetDepth(const DepthState& state);
	void SetDepthMask(bool enabled);
	void SetDepthFunc(CompareFunc depth_func);
	void SetDepthRange(float near_val, float far_val);
	void SetLineWidth(float width);
	void SetLineSmoothing(bool enabled);
	void SetPolygonMode(PolygonMode front_mode, PolygonMode back_mode);
	void SetColorMask(const ColorMaskState& mask);
	void SetScissor(const ScissorState& scissor);
	void SetCull(const CullState& cull);
	void SetRaster(const RasterState& raster);
	void SetStencil(const StencilState& stencil);
	void SetClearColor(Color color);
	void SetClearDepth(double depth);
	void SetClearStencil(int stencil);

	void SetViewport(const Viewport& viewport);
	[[nodiscard]] Viewport GetViewport() const;

	void SetActiveTextureSlot(std::uint32_t slot);

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] std::size_t GetMaxTextureSlots() const;

	Buffers buffers;
	Shaders shaders;
	Textures textures;
	Renderbuffers renderbuffers;
	Framebuffers framebuffers;
	VertexArrays vertex_arrays;

private:
	[[nodiscard]] std::uint32_t GetActiveTextureSlot() const;

	int GetInteger(GLenum pname) const;

	State bound_;

	SDL_GLContextState* context_{ nullptr };
};

template <typename T>
BindGuard<T>::~BindGuard() noexcept {
	if (restore_bind_) {
		auto _ = gl_.Bind(id_, false);
	}
}

} // namespace ptgn::impl::gl