#pragma once

#include <cstdint>
#include <optional>

#include "core/graphics/color.h"
#include "core/math/matrix4.h"
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

namespace ptgn {

class Stats;

namespace impl::gl {

class GLContext {
public:
	explicit GLContext(Stats& stats);
	~GLContext() noexcept					   = default;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	[[nodiscard]] BindGuard<VertexBufferId> Bind(VertexBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ElementBufferId> Bind(ElementBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<UniformBufferId> Bind(UniformBufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ShaderId> Bind(ShaderId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<TextureId> Bind(
		TextureId id, bool restore_bind = false, bool force = false
	);
	[[nodiscard]] BindGuard<RenderbufferId> Bind(RenderbufferId id, bool restore_bind = false);
	[[nodiscard]] BindGuard<FramebufferId> Bind(
		FramebufferId id, bool restore_bind = false, bool force = false
	);
	[[nodiscard]] BindGuard<VertexArrayId> Bind(VertexArrayId id, bool restore_bind = false);

	const State& GetBoundState() const;
	State& GetBoundState();
	VertexBufferId GetBoundVertexBuffer() const;
	/// @brief One important assumption in this being correct is that a protegon VertexArray's
	/// element buffer is never modified by an external library / application.
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
	/// @param replacement_texture The replacement texture id that will be bound instead of the id.
	void Destroy(TextureId id, TextureId replacement_texture);
	void Destroy(RenderbufferId id);
	/// @param replacement_texture The replacement texture id that will be bound instead of the
	/// framebuffer's attached texture id (if applicable).
	void Destroy(FramebufferId id, TextureId replacement_texture);
	void Destroy(VertexArrayId id);

	void ForgetId(VertexBufferId id);
	void ForgetId(UniformBufferId id);
	void ForgetId(ShaderId id);
	/// @param replacement_texture The replacement texture id that will be bound instead of the id.
	void ForgetId(TextureId id, TextureId replacement_texture);
	void ForgetId(RenderbufferId id);
	void ForgetId(FramebufferId id);
	void ForgetId(VertexArrayId id);

	/// @brief Enabling blending will disable depth testing.
	void SetBlend(bool enabled, bool force = false);
	/// @brief Will disable depth testing.
	void SetBlendMode(BlendMode blend, bool force = false);

	/// @brief Enabling depth testing will disable blending.
	void SetDepthTesting(bool enabled, bool force = false);
	void SetDepthMask(const DepthMaskState& mask);

	void SetColorMask(const ColorMaskState& mask);
	void SetScissor(const ScissorState& scissor);
	void SetRaster(const RasterState& raster);
	void SetStencil(const StencilState& stencil);
	void SetClearColor(Color color);
	void SetClearDepth(Depth depth);
	void SetClearStencil(Stencil stencil);

	void SetViewport(Viewport viewport);
	std::optional<Viewport> GetViewport() const;

	void SetViewProjection(const Matrix4& view_projection);

	const std::optional<Matrix4>& GetViewProjection() const;

	void SetActiveTextureSlot(std::uint32_t slot, bool force = false);

	/// @return The maximum number of texture slots available on the current hardware.
	std::size_t GetMaxTextureSlots() const;

	[[nodiscard]] bool ViewportCoversFramebuffer(FramebufferId framebuffer) const;
	[[nodiscard]] bool ScissorCoversFramebuffer(FramebufferId framebuffer) const;

	std::uint32_t GetActiveTextureSlot() const;

	void ResetState();

	Stats& stats;

private:
	// Must be constructed before shaders, because it fetches max texture slots.
	State bound_;

public:
	Buffers buffers;
	Shaders shaders;
	Textures textures;
	Renderbuffers renderbuffers;
	Framebuffers framebuffers;
	VertexArrays vertex_arrays;

private:
	friend class Framebuffers;
};

} // namespace impl::gl

} // namespace ptgn