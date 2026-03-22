#pragma once

#include <cstdint>

#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_debug.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/viewport.h"
#include "SDL3/SDL_opengl.h"

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

namespace impl {

class RenderTargetData;

} // namespace impl

} // namespace ptgn

namespace ptgn::impl::gl {

class GLContext;

/// @brief This class exists to ensure the OpenGL context is destroyed after things like shaders.
class SDLGLContext {
private:
	friend class GLContext;

	SDLGLContext() = delete;
	explicit SDLGLContext(const Window& window);
	~SDLGLContext() noexcept;
	SDLGLContext(const SDLGLContext&)				 = delete;
	SDLGLContext(SDLGLContext&&) noexcept			 = delete;
	SDLGLContext& operator=(const SDLGLContext&)	 = delete;
	SDLGLContext& operator=(SDLGLContext&&) noexcept = delete;

	SDL_GLContextState* context_{ nullptr };
};

class GLContext {
private:
	SDLGLContext context_;

public:
	GLContext() = delete;
	explicit GLContext(const Window& window);
	~GLContext() noexcept					   = default;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	[[nodiscard]] BindGuard<VertexBufferId> Bind(VertexBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ElementBufferId> Bind(ElementBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<UniformBufferId> Bind(UniformBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ShaderId> Bind(ShaderId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<TextureId> Bind(TextureId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<RenderbufferId> Bind(RenderbufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<FramebufferId> Bind(FramebufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<VertexArrayId> Bind(VertexArrayId id, bool restore_bind = false);

	const State& GetBoundState() const;
	State& GetBoundState();
	VertexBufferId GetBoundVertexBuffer() const;
	ElementBufferId GetBoundElementBuffer() const;
	UniformBufferId GetBoundUniformBuffer() const;
	ShaderId GetBoundShader() const;
	TextureId GetBoundTexture() const;
	RenderbufferId GetBoundRenderbuffer() const;
	FramebufferId GetBoundFramebuffer() const;
	VertexArrayId GetBoundVertexArray() const;

	[[nodiscard]] bool IsBound(VertexBufferId id) const;
	[[nodiscard]] bool IsBound(ElementBufferId id) const;
	[[nodiscard]] bool IsBound(UniformBufferId id) const;
	[[nodiscard]] bool IsBound(ShaderId id) const;
	[[nodiscard]] bool IsBound(TextureId id) const;
	[[nodiscard]] bool IsBound(RenderbufferId id) const;
	[[nodiscard]] bool IsBound(FramebufferId id) const;
	[[nodiscard]] bool IsBound(VertexArrayId id) const;

	void Destroy(VertexBufferId id);
	void Destroy(ElementBufferId id);
	void Destroy(UniformBufferId id);
	void Destroy(ShaderId id);
	void Destroy(TextureId id);
	void Destroy(RenderbufferId id);
	void Destroy(FramebufferId id);
	void Destroy(VertexArrayId id);
	void Destroy(RenderTargetId id);

	void EnableGammaCorrection() const;
	void DisableGammaCorrection() const;

	/// @brief Enabling blending will disable depth testing.
	void SetBlending(bool enabled);
	void SetBlend(const BlendState& blend_state);
	/// @brief Will disable depth testing.
	void SetBlendMode(BlendMode mode);

	/// @brief Enabling depth testing will disable blending.
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

	void SetViewport(Viewport viewport);
	Viewport GetViewport() const;

	void SetActiveTextureSlot(std::uint32_t slot);

	/// @return The maximum number of texture slots available on the current hardware.
	std::size_t GetMaxTextureSlots() const;

	Buffers buffers;
	Shaders shaders;
	Textures textures;
	Renderbuffers renderbuffers;
	Framebuffers framebuffers;
	VertexArrays vertex_arrays;

	int GetInteger(GLenum pname) const;
	std::uint32_t GetActiveTextureSlot() const;

private:
	State bound_;
};

} // namespace ptgn::impl::gl