#include "renderer/backend/gl/gl_context.h"

#include <glad/gl.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/id_map.h"
#include "platform/window.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_buffer.h"
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

#define PTGN_IMPL_BLEND_CASE(name, srcRGB, dstRGB, srcA, dstA) \
	case BlendMode::name: GLCall(glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)); break;

namespace ptgn::impl::gl {

GLContext::GLContext(const Window& window) :
	buffers{ *this },
	shaders{ *this },
	textures{ *this },
	renderbuffers{ *this },
	framebuffers{ *this },
	vertex_arrays{ *this } {
	auto max_texture_slots{ static_cast<std::size_t>(GetInteger(GL_MAX_TEXTURE_IMAGE_UNITS)) };
	PTGN_ASSERT(max_texture_slots > 0);
	bound_.texture_units.resize(max_texture_slots, {});

	auto max_color_attachments{ static_cast<std::uint32_t>(GetInteger(GL_MAX_COLOR_ATTACHMENTS)) };
	PTGN_ASSERT(max_color_attachments > 0);

	framebuffers.Init(max_color_attachments);

	shaders.Populate(max_texture_slots);
}

BindGuard<VertexBufferId> GLContext::Bind(VertexBufferId id, bool restore_bind) {
	auto previous{ GetBoundVertexBuffer() };

	if (id == previous) {
		return BindGuard<VertexBufferId>{ *this, VertexBufferId{}, false };
	}

	constexpr BufferTarget target{ BufferTarget::ArrayBuffer };

	GLCall(glBindBuffer(std::to_underlying(target), id));
	bound_.vertex_buffer = id;

	return BindGuard<VertexBufferId>{ *this, previous, restore_bind };
}

BindGuard<ElementBufferId> GLContext::Bind(ElementBufferId id, bool restore_bind) {
	auto previous{ GetBoundElementBuffer() };

	if (id == previous) {
		return BindGuard<ElementBufferId>{ *this, ElementBufferId{}, false };
	}

	constexpr BufferTarget target{ BufferTarget::ElementArrayBuffer };

	GLCall(glBindBuffer(std::to_underlying(target), id));

	if (bound_.vertex_array) {
		vertex_arrays.cache_.Get(bound_.vertex_array).element_buffer = id;
	}

	return BindGuard<ElementBufferId>{ *this, previous, restore_bind };
}

BindGuard<UniformBufferId> GLContext::Bind(UniformBufferId id, bool restore_bind) {
	auto previous{ GetBoundUniformBuffer() };

	if (id == previous) {
		return BindGuard<UniformBufferId>{ *this, UniformBufferId{}, false };
	}

	constexpr BufferTarget target{ BufferTarget::UniformBuffer };

	GLCall(glBindBuffer(std::to_underlying(target), id));

	bound_.uniform_buffer = id;

	return BindGuard<UniformBufferId>{ *this, previous, restore_bind };
}

BindGuard<ShaderId> GLContext::Bind(ShaderId id, bool restore_bind) {
	auto previous{ GetBoundShader() };

	if (id == previous) {
		return BindGuard<ShaderId>{ *this, ShaderId{}, false };
	}

	GLCall(glUseProgram(id));

	bound_.shader_program = id;

	return BindGuard<ShaderId>{ *this, previous, restore_bind };
}

BindGuard<RenderbufferId> GLContext::Bind(RenderbufferId id, bool restore_bind) {
	auto previous{ GetBoundRenderbuffer() };

	if (id == previous) {
		return BindGuard<RenderbufferId>{ *this, RenderbufferId{}, false };
	}

	constexpr AttachmentObject target{ AttachmentObject::Renderbuffer };

	GLCall(glBindRenderbuffer(std::to_underlying(target), id));

	bound_.renderbuffer = id;

	return BindGuard<RenderbufferId>{ *this, previous, restore_bind };
}

BindGuard<TextureId> GLContext::Bind(TextureId id, bool restore_bind) {
	auto previous{ GetBoundTexture() };

	if (id == previous) {
		return BindGuard<TextureId>{ *this, TextureId{}, false };
	}

	auto slot{ GetActiveTextureSlot() };
	PTGN_ASSERT(slot < GetMaxTextureSlots(), "Slot out of range of max slots");
	PTGN_ASSERT(bound_.texture_units[slot].id != id);

	constexpr AttachmentObject target{ AttachmentObject::Texture2D };

	GLCall(glBindTexture(std::to_underlying(target), id));
	bound_.texture_units[slot].id = id;

	return BindGuard<TextureId>{ *this, previous, restore_bind };
}

BindGuard<FramebufferId> GLContext::Bind(FramebufferId id, bool restore_bind) {
	auto previous{ GetBoundFramebuffer() };

	if (id == previous) {
		return BindGuard<FramebufferId>{ *this, FramebufferId{}, false };
	}

	GLCall(glBindFramebuffer(kFrameBufferTarget, id));
	bound_.framebuffer = id;

	return BindGuard<FramebufferId>{ *this, previous, restore_bind };
}

BindGuard<VertexArrayId> GLContext::Bind(VertexArrayId id, bool restore_bind) {
	auto previous{ GetBoundVertexArray() };

	if (id == previous) {
		return BindGuard<VertexArrayId>{ *this, VertexArrayId{}, false };
	}

	// On Mac we cannot bind 0 for vertex arrays, so we skip it.
#ifdef PTGN_PLATFORM_MACOS
	if (id) {
#endif
		GLCall(glBindVertexArray(id));
#ifdef PTGN_PLATFORM_MACOS
	}
#endif

	bound_.vertex_array = id;

	return BindGuard<VertexArrayId>{ *this, previous, restore_bind };
}

VertexBufferId GLContext::GetBoundVertexBuffer() const {
	return bound_.vertex_buffer;
}

ElementBufferId GLContext::GetBoundElementBuffer() const {
	return bound_.vertex_array ? vertex_arrays.cache_.Get(bound_.vertex_array).element_buffer
							   : ElementBufferId{ 0 };
}

UniformBufferId GLContext::GetBoundUniformBuffer() const {
	return bound_.uniform_buffer;
}

const State& GLContext::GetBoundState() const {
	return bound_;
}

State& GLContext::GetBoundState() {
	return bound_;
}

ShaderId GLContext::GetBoundShader() const {
	return bound_.shader_program;
}

TextureId GLContext::GetBoundTexture() const {
	PTGN_ASSERT(bound_.active_texture.slot < GetMaxTextureSlots());
	return bound_.texture_units[bound_.active_texture.slot].id;
}

RenderbufferId GLContext::GetBoundRenderbuffer() const {
	return bound_.renderbuffer;
}

FramebufferId GLContext::GetBoundFramebuffer() const {
	return bound_.framebuffer;
}

VertexArrayId GLContext::GetBoundVertexArray() const {
	return bound_.vertex_array;
}

bool GLContext::IsBound(VertexBufferId id) const {
	return GetBoundVertexBuffer() == id;
}

bool GLContext::IsBound(ElementBufferId id) const {
	return GetBoundElementBuffer() == id;
}

bool GLContext::IsBound(UniformBufferId id) const {
	return GetBoundUniformBuffer() == id;
}

bool GLContext::IsBound(ShaderId id) const {
	return GetBoundShader() == id;
}

bool GLContext::IsBound(RenderbufferId id) const {
	return GetBoundRenderbuffer() == id;
}

bool GLContext::IsBound(TextureId id) const {
	return GetBoundTexture() == id;
}

bool GLContext::IsBound(FramebufferId id) const {
	return GetBoundFramebuffer() == id;
}

bool GLContext::IsBound(VertexArrayId id) const {
	return GetBoundVertexArray() == id;
}

void GLContext::Destroy(VertexBufferId id) {
	if (bound_.vertex_buffer == id) {
		bound_.vertex_buffer = {};
	}
	buffers.DestroyVertexBuffer(id);
}

void GLContext::Destroy(ElementBufferId id) {
	vertex_arrays.InvalidateElementBuffer(id);
	buffers.DestroyElementBuffer(id);
}

void GLContext::Destroy(UniformBufferId id) {
	if (bound_.uniform_buffer == id) {
		bound_.uniform_buffer = {};
	}
	buffers.DestroyUniformBuffer(id);
}

void GLContext::Destroy(ShaderId id) {
	if (bound_.shader_program == id) {
		bound_.shader_program = {};
	}
	shaders.DestroyProgram(id);
}

void GLContext::Destroy(TextureId id) {
	for (auto& unit : bound_.texture_units) {
		if (unit.id == id) {
			unit = {};
		}
	}
	framebuffers.InvalidateTexture(id);
	textures.DestroyTexture(id);
}

void GLContext::Destroy(RenderbufferId id) {
	if (bound_.renderbuffer == id) {
		bound_.renderbuffer = {};
	}
	framebuffers.InvalidateRenderbuffer(id);
	renderbuffers.DestroyRenderbuffer(id);
}

void GLContext::Destroy(FramebufferId id) {
	if (bound_.framebuffer == id) {
		bound_.framebuffer = {};
	}
	framebuffers.DestroyFramebuffer(id);
}

void GLContext::Destroy(VertexArrayId id) {
	if (bound_.vertex_array == id) {
		bound_.vertex_array = {};
	}
	vertex_arrays.DestroyVertexArray(id);
}

void GLContext::Destroy(RenderTargetId id) {
	framebuffers.DestroyFramebufferOwning(FramebufferId{ id });
}

void GLContext::SetBlend(const BlendState& blend_state) {
	SetBlending(blend_state.enabled);
	if (blend_state.enabled) {
		SetBlendMode(blend_state.mode);
	}
}

void GLContext::SetBlending(bool enabled) {
	if (enabled) {
		SetDepthTesting(GL_FALSE);
	}
	if (bound_.blend.enabled == enabled) {
		return;
	}
	if (enabled) {
		GLCall(glEnable(GL_BLEND));
	} else {
		GLCall(glDisable(GL_BLEND));
	}
	bound_.blend.enabled = enabled;
}

void GLContext::SetDepth(const DepthState& depth_state) {
	SetDepthMask(depth_state.write);
	SetDepthFunc(depth_state.func);
	SetDepthTesting(depth_state.test);
	SetDepthRange(depth_state.range_near, depth_state.range_far);
}

void GLContext::SetDepthMask(bool enabled) {
	if (bound_.depth.write == enabled) {
		return;
	}
	GLCall(glDepthMask(enabled));
	bound_.depth.write = enabled;
}

void GLContext::SetDepthFunc(CompareFunc depth_func) {
	if (bound_.depth.func == depth_func) {
		return;
	}
	GLCall(glDepthFunc(std::to_underlying(depth_func)));
	bound_.depth.func = depth_func;
}

void GLContext::SetDepthTesting(bool enabled) {
	if (enabled) {
		SetBlending(GL_FALSE);
	}
	if (bound_.depth.test == enabled) {
		return;
	}
	if (enabled) {
		constexpr double value{ 1.0 };
		GLCall(glClearDepth(value));
		GLCall(glEnable(GL_DEPTH_TEST));
	} else {
		GLCall(glDisable(GL_DEPTH_TEST));
	}
	bound_.depth.test = enabled;
}

void GLContext::SetDepthRange(float near_val, float far_val) {
	if (NearlyEqual(bound_.depth.range_near, near_val) &&
		NearlyEqual(bound_.depth.range_far, far_val)) {
		return;
	}
	GLCall(glDepthRange(near_val, far_val));
	bound_.depth.range_near = near_val;
	bound_.depth.range_far	= far_val;
}

void GLContext::SetLineWidth(float width) {
	if (NearlyEqual(bound_.raster.line_width.value, width)) {
		return;
	}
	PTGN_ASSERT(width >= 1.0f, "Only line widths >= 1.0 are supported");
	GLCall(glLineWidth(width));
	bound_.raster.line_width = LineWidth{ width };
}

void GLContext::SetBlendMode(BlendMode mode) {
	SetBlending(GL_TRUE);

	if (bound_.blend.mode == mode) {
		return;
	}

	GLCall(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));

	switch (mode) {
		PTGN_IMPL_BLEND_CASE(
			Blend, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA
		)
		PTGN_IMPL_BLEND_CASE(
			PremultipliedBlend, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA
		)
		PTGN_IMPL_BLEND_CASE(ReplaceRGBA, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO)
		PTGN_IMPL_BLEND_CASE(ReplaceRGB, GL_ONE, GL_ZERO, GL_ZERO, GL_ONE)
		PTGN_IMPL_BLEND_CASE(ReplaceAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ZERO)
		PTGN_IMPL_BLEND_CASE(AddRGB, GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE)
		PTGN_IMPL_BLEND_CASE(AddRGBA, GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE)
		PTGN_IMPL_BLEND_CASE(AddAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ONE)
		PTGN_IMPL_BLEND_CASE(PremultipliedAddRGB, GL_ONE, GL_ONE, GL_ZERO, GL_ONE)
		PTGN_IMPL_BLEND_CASE(PremultipliedAddRGBA, GL_ONE, GL_ONE, GL_ONE, GL_ONE)
		PTGN_IMPL_BLEND_CASE(MultiplyRGB, GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE)
		PTGN_IMPL_BLEND_CASE(MultiplyRGBA, GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO)
		PTGN_IMPL_BLEND_CASE(MultiplyAlpha, GL_ZERO, GL_ONE, GL_DST_ALPHA, GL_ZERO)
		PTGN_IMPL_BLEND_CASE(
			MultiplyRGBWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE
		)
		PTGN_IMPL_BLEND_CASE(
			MultiplyRGBAWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ZERO
		)
		default: PTGN_ERROR("Failed to identify blend mode");
	}

	bound_.blend.mode = mode;
}

void GLContext::SetViewport(Viewport viewport) {
	if (bound_.viewport == viewport) {
		return;
	}
	GLCall(glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y));
	bound_.viewport = viewport;
}

Viewport GLContext::GetViewport() const {
	return bound_.viewport;
}

void GLContext::SetClearColor(Color color) {
	if (bound_.clear_color.value == color) {
		return;
	}
	auto n{ static_cast<V4_float>(color) };
	GLCall(glClearColor(n.x, n.y, n.z, n.w));
	bound_.clear_color = ClearColor{ color };
}

void GLContext::SetClearDepth(double depth) {
	if (NearlyEqual(bound_.clear_depth.value, depth)) {
		return;
	}
	PTGN_ASSERT(depth >= 0.0 && depth <= 1.0, "glClearDepth: depth must be in range [0.0, 1.0]");
	GLCall(glClearDepth(depth));
	bound_.clear_depth = ClearDepth{ depth };
}

void GLContext::SetClearStencil(int stencil) {
	if (bound_.clear_stencil.value == stencil) {
		return;
	}
	PTGN_ASSERT(stencil >= 0, "glClearStencil: stencil value must be non-negative");
	GLCall(glClearStencil(stencil));
	bound_.clear_stencil = ClearStencil{ stencil };
}

void GLContext::SetColorMask(const ColorMaskState& mask) {
	if (bound_.color_mask == mask) {
		return;
	}
	GLCall(glColorMask(mask.red, mask.green, mask.blue, mask.alpha));
	bound_.color_mask = mask;
}

void GLContext::SetScissor(const ScissorState& scissor) {
	if (bound_.scissor == scissor) {
		return;
	}

	if (scissor.enabled) {
		if (!bound_.scissor.enabled) {
			GLCall(glEnable(GL_SCISSOR_TEST));
		}
		if (bound_.scissor.viewport != scissor.viewport) {
			GLCall(glScissor(
				scissor.viewport.position.x, scissor.viewport.position.y, scissor.viewport.size.x,
				scissor.viewport.size.y
			));
		}
	} else {
		if (bound_.scissor.enabled) {
			GLCall(glDisable(GL_SCISSOR_TEST));
		}
	}

	bound_.scissor = scissor;
}

void GLContext::SetCull(const CullState& cull) {
	if (bound_.raster.cull == cull) {
		return;
	}

	if (cull.enabled) {
		GLCall(glEnable(GL_CULL_FACE));
	} else {
		GLCall(glDisable(GL_CULL_FACE));
	}

	GLCall(glCullFace(std::to_underlying(cull.cull_face)));
	GLCall(glFrontFace(std::to_underlying(cull.front_face)));

	bound_.raster.cull = cull;
}

void GLContext::SetRaster(const RasterState& raster) {
	SetLineWidth(raster.line_width.value);
	// SetLineSmoothing(raster.line_smoothing);
	// SetPolygonMode(raster.polygon.front, raster.polygon.back);
	SetCull(raster.cull);
}

void GLContext::SetStencil(const StencilState& stencil) {
	if (bound_.stencil == stencil) {
		return;
	}

	if (stencil.enabled) {
		GLCall(glEnable(GL_STENCIL_TEST));
	} else {
		GLCall(glDisable(GL_STENCIL_TEST));
	}

	GLCall(glStencilFunc(std::to_underlying(stencil.func), stencil.ref, stencil.mask));
	GLCall(glStencilOp(
		std::to_underlying(stencil.fail_op), std::to_underlying(stencil.zfail_op),
		std::to_underlying(stencil.zpass_op)
	));
	GLCall(glStencilMask(stencil.write_mask));

	bound_.stencil = stencil;
}

void GLContext::SetActiveTextureSlot(std::uint32_t slot) {
	if (bound_.active_texture.slot == slot) {
		return;
	}
	PTGN_ASSERT(
		slot < GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
	GLCall(glActiveTexture(GL_TEXTURE0 + slot));

	bound_.active_texture = ActiveTexture{ slot };
}

std::size_t GLContext::GetMaxTextureSlots() const {
	return bound_.texture_units.size();
}

std::uint32_t GLContext::GetActiveTextureSlot() const {
	return bound_.active_texture.slot;
}

int GLContext::GetInteger(std::uint32_t pname) const {
	int value = -1;
	GLCall(glGetIntegerv(pname, &value));
	PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
	return value;
}

} // namespace ptgn::impl::gl