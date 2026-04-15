#pragma once

#include <cstdint>
#include <optional>

#include "core/graphics/color.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"

namespace ptgn::impl::gl {

class GLContext;

class GLContext {
public:
	GLContext() = delete;
	explicit GLContext();
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
	std::optional<VertexBufferId> GetBoundVertexBuffer() const;
	/// @brief One important assumption in this being correct is that a protegon VertexArray's
	/// element buffer is never modified by an external library / application.
	std::optional<ElementBufferId> GetBoundElementBuffer() const;
	std::optional<UniformBufferId> GetBoundUniformBuffer() const;
	std::optional<ShaderId> GetBoundShader() const;
	std::optional<TextureId> GetBoundTexture() const;
	std::optional<RenderbufferId> GetBoundRenderbuffer() const;
	std::optional<FramebufferId> GetBoundFramebuffer() const;
	std::optional<VertexArrayId> GetBoundVertexArray() const;

	/// @brief Note, this only checks if the state things the given id is bound, so it may be
	/// incorrect if the state is out of sync with the actual OpenGL state. As is this case in the
	/// beginning of each frame.
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

	/// @brief Enabling blending will disable depth testing.
	void SetBlend(bool enabled);
	/// @brief Will disable depth testing.
	void SetBlendMode(BlendMode blend);

	/// @brief Enabling depth testing will disable blending.
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);

	void SetColorMask(const ColorMaskState& mask);
	void SetScissor(const ScissorState& scissor);
	void SetRaster(const RasterState& raster);
	void SetStencil(const StencilState& stencil);
	void SetClearColor(Color color);
	void SetClearDepth(double depth);
	void SetClearStencil(int stencil);

	void SetViewport(Viewport viewport);
	std::optional<Viewport> GetViewport() const;

	void SetActiveTextureSlot(std::uint32_t slot);

	/// @return The maximum number of texture slots available on the current hardware.
	std::size_t GetMaxTextureSlots() const;

	Buffers buffers;
	Shaders shaders;
	Textures textures;
	Renderbuffers renderbuffers;
	Framebuffers framebuffers;
	VertexArrays vertex_arrays;

	int GetInteger(std::uint32_t pname) const;
	std::uint32_t GetActiveTextureSlot() const;

	void InvalidateState();

private:
	State bound_;
};

} // namespace ptgn::impl::gl