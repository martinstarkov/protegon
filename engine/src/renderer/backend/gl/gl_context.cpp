#include "renderer/backend/gl/gl_context.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
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

/// @brief Helper around glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)
#define PTGN_IMPL_BLEND_CASE(name, srcRGB, dstRGB, srcA, dstA) \
	case BlendMode::name: GLCall(glBlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)); break;

namespace ptgn::impl::gl {

GLContext::GLContext(Stats& stats) :
	stats{ stats },
	bound_{ static_cast<std::size_t>(GetInteger(GL_MAX_TEXTURE_IMAGE_UNITS)) },
	buffers{ *this },
	shaders{ *this, GetMaxTextureSlots() },
	textures{ *this },
	renderbuffers{ *this },
	framebuffers{ *this, static_cast<std::uint32_t>(GetInteger(GL_MAX_COLOR_ATTACHMENTS)) },
	vertex_arrays{ *this } {}

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

	PTGN_ASSERT(
		bound_.vertex_array.has_value(),
		"Vertex array must be bound before binding an element buffer"
	);

	if (*bound_.vertex_array) {
		vertex_arrays.cache_.Get(*bound_.vertex_array).element_buffer = id;
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

	PTGN_ASSERT(!id || textures.cache_.Has(id), "Texture ", id, " not found in texture cache");

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

const State& GLContext::GetBoundState() const {
	return bound_;
}

State& GLContext::GetBoundState() {
	return bound_;
}

std::optional<VertexBufferId> GLContext::GetBoundVertexBuffer() const {
	return bound_.vertex_buffer;
}

std::optional<ElementBufferId> GLContext::GetBoundElementBuffer() const {
	if (!bound_.vertex_array.has_value() || !*bound_.vertex_array) {
		return std::nullopt;
	}
	return vertex_arrays.cache_.Get(*bound_.vertex_array).element_buffer;
}

std::optional<UniformBufferId> GLContext::GetBoundUniformBuffer() const {
	return bound_.uniform_buffer;
}

std::optional<ShaderId> GLContext::GetBoundShader() const {
	return bound_.shader_program;
}

std::optional<TextureId> GLContext::GetBoundTexture() const {
	PTGN_ASSERT(bound_.active_texture.slot < GetMaxTextureSlots());
	return bound_.texture_units[bound_.active_texture.slot].id;
}

std::optional<RenderbufferId> GLContext::GetBoundRenderbuffer() const {
	return bound_.renderbuffer;
}

std::optional<FramebufferId> GLContext::GetBoundFramebuffer() const {
	return bound_.framebuffer;
}

std::optional<VertexArrayId> GLContext::GetBoundVertexArray() const {
	return bound_.vertex_array;
}

bool GLContext::IsBound(VertexBufferId id) const {
	return bound_.vertex_buffer == id ||
		   !bound_.vertex_buffer.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_ARRAY_BUFFER_BINDING)) == id;
}

bool GLContext::IsBound(ElementBufferId id) const {
	auto bound_id{ GetBoundElementBuffer() };
	return bound_id == id ||
		   !bound_id.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_ELEMENT_ARRAY_BUFFER_BINDING)) == id;
}

bool GLContext::IsBound(UniformBufferId id) const {
	return bound_.uniform_buffer == id ||
		   !bound_.uniform_buffer.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_UNIFORM_BUFFER_BINDING)) == id;
}

bool GLContext::IsBound(ShaderId id) const {
	return bound_.shader_program == id ||
		   !bound_.shader_program.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_CURRENT_PROGRAM)) == id;
}

void GLContext::ForgetId(VertexBufferId id) {
	if (bound_.vertex_buffer == id) {
		GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
		bound_.vertex_buffer = VertexBufferId{ 0 };
	}
}

void GLContext::ForgetId(UniformBufferId id) {
	if (bound_.uniform_buffer == id) {
		GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));
		bound_.uniform_buffer = UniformBufferId{ 0 };
	}
}

void GLContext::ForgetId(ShaderId id) {
	if (bound_.shader_program == id) {
		GLCall(glUseProgram(0));
		bound_.shader_program = ShaderId{ 0 };
	}
}

void GLContext::ForgetId(TextureId id) {
	if (!id) {
		return;
	}

	auto previous_active_texture{ bound_.active_texture };

	for (auto slot{ 0u }; slot < bound_.texture_units.size(); ++slot) {
		auto& unit{ bound_.texture_units[slot] };

		if (unit.id != id) {
			continue;
		}

		if (bound_.active_texture.slot != slot) {
			GLCall(glActiveTexture(GL_TEXTURE0 + slot));
			bound_.active_texture = ActiveTexture{ slot };
		}

		GLCall(glBindTexture(GL_TEXTURE_2D, 0));

		unit.id = TextureId{ 0 };

		// These belong to the deleted texture object, so they are no longer known.
		unit.min_filter.reset();
		unit.mag_filter.reset();
		unit.wrap_s.reset();
		unit.wrap_t.reset();
	}

	if (bound_.active_texture != previous_active_texture) {
		GLCall(glActiveTexture(GL_TEXTURE0 + previous_active_texture.slot));
		bound_.active_texture = previous_active_texture;
	}
}

void GLContext::ForgetId(RenderbufferId id) {
	if (bound_.renderbuffer == id) {
		GLCall(glBindRenderbuffer(GL_RENDERBUFFER, 0));
		bound_.renderbuffer = RenderbufferId{ 0 };
	}
}

void GLContext::ForgetId(FramebufferId id) {
	if (bound_.framebuffer == id) {
		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		bound_.framebuffer = FramebufferId{ 0 };
	}
}

void GLContext::ForgetId(VertexArrayId id) {
	if (bound_.vertex_array == id) {
		GLCall(glBindVertexArray(0));
		bound_.vertex_array = VertexArrayId{ 0 };
	}
}

bool GLContext::IsBound(TextureId id) const {
	auto bound_id{ GetBoundTexture() };
	return bound_id == id ||
		   !bound_id.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_TEXTURE_BINDING_2D)) == id;
}

bool GLContext::IsBound(RenderbufferId id) const {
	return bound_.renderbuffer == id ||
		   !bound_.renderbuffer.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_RENDERBUFFER_BINDING)) == id;
}

bool GLContext::IsBound(FramebufferId id) const {
	return bound_.framebuffer == id ||
		   !bound_.framebuffer.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_FRAMEBUFFER_BINDING)) == id;
}

bool GLContext::IsBound(VertexArrayId id) const {
	return bound_.vertex_array == id ||
		   !bound_.vertex_array.has_value() &&
			   static_cast<std::uint32_t>(GetInteger(GL_VERTEX_ARRAY_BINDING)) == id;
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

void GLContext::SetBlend(bool enabled) {
	if (enabled) {
		SetDepthTesting(false);
	}

	if (bound_.render_state.blending == enabled) {
		return;
	}

	GLCall(enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND));

	bound_.render_state.blending = enabled;
}

void GLContext::SetDepthTesting(bool enabled) {
	if (enabled) {
		SetBlend(false);
	}

	if (bound_.render_state.depth_testing == enabled) {
		return;
	}

	GLCall(enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));

	bound_.render_state.depth_testing = enabled;
}

void GLContext::SetBlendMode(BlendMode blend) {
	SetBlend(true);

	if (bound_.render_state.blend_mode == blend) {
		return;
	}

	GLCall(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));

	switch (blend) {
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
		default: PTGN_ERROR("Unknown BlendMode: ", std::to_underlying(blend));
	}

	bound_.render_state.blend_mode = blend;
}

void GLContext::SetDepthMask(const DepthMaskState& mask) {
	if (bound_.render_state.depth_mask == mask) {
		return;
	}

	if (!bound_.render_state.depth_mask.has_value() ||
		bound_.render_state.depth_mask.has_value() &&
			bound_.render_state.depth_mask->func != mask.func) {
		GLCall(glDepthFunc(std::to_underlying(mask.func)));
	}
	if (!bound_.render_state.depth_mask.has_value() ||
		bound_.render_state.depth_mask.has_value() &&
			bound_.render_state.depth_mask->write != mask.write) {
		GLCall(glDepthMask(mask.write));
	}
	if (!bound_.render_state.depth_mask.has_value() ||
		bound_.render_state.depth_mask.has_value() &&
			(!NearlyEqual(bound_.render_state.depth_mask->range_near, mask.range_near) ||
			 !NearlyEqual(bound_.render_state.depth_mask->range_far, mask.range_far))) {
		GLCall(glDepthRange(mask.range_near, mask.range_far));
	}

	bound_.render_state.depth_mask = mask;
}

void GLContext::SetViewport(Viewport viewport) {
	PTGN_ASSERT(viewport.size.IsPositive(), "Cannot set viewport with non-positive size");

	if (bound_.render_state.viewport == viewport) {
		return;
	}
	GLCall(glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y));
	bound_.render_state.viewport = viewport;
}

std::optional<Viewport> GLContext::GetViewport() const {
	return bound_.render_state.viewport;
}

void GLContext::SetViewProjection(const Matrix4& view_projection) {
	bound_.render_state.view_projection = view_projection;
}

const std::optional<Matrix4>& GLContext::GetViewProjection() const {
	return bound_.render_state.view_projection;
}

void GLContext::SetClearColor(Color color) {
	if (bound_.clear_color == color) {
		return;
	}
	V4_float n{ color };
	GLCall(glClearColor(n.x, n.y, n.z, n.w));
	bound_.clear_color = color;
}

void GLContext::SetClearDepth(double depth) {
	if (bound_.clear_depth == ClearDepth{ depth }) {
		return;
	}
	PTGN_ASSERT(depth >= 0.0 && depth <= 1.0, "Clear depth must be in range [0.0, 1.0]");
	GLCall(glClearDepth(depth));
	bound_.clear_depth = ClearDepth{ depth };
}

void GLContext::SetClearStencil(int stencil) {
	if (bound_.clear_stencil == stencil) {
		return;
	}
	PTGN_ASSERT(stencil >= 0, "glClearStencil: stencil value must be non-negative");
	GLCall(glClearStencil(stencil));
	bound_.clear_stencil = stencil;
}

void GLContext::SetColorMask(const ColorMaskState& mask) {
	if (bound_.render_state.color_mask == mask) {
		return;
	}
	GLCall(glColorMask(mask.red, mask.green, mask.blue, mask.alpha));
	bound_.render_state.color_mask = mask;
}

void GLContext::SetScissor(const ScissorState& scissor) {
	if (bound_.render_state.scissor == scissor) {
		return;
	}

	if (scissor.enabled) {
		if (!bound_.render_state.scissor.has_value() ||
			bound_.render_state.scissor.has_value() && !bound_.render_state.scissor->enabled) {
			GLCall(glEnable(GL_SCISSOR_TEST));
		}
		if (!bound_.render_state.scissor.has_value() ||
			bound_.render_state.scissor.has_value() &&
				bound_.render_state.scissor->viewport != scissor.viewport) {
			GLCall(glScissor(
				scissor.viewport.position.x, scissor.viewport.position.y, scissor.viewport.size.x,
				scissor.viewport.size.y
			));
		}
	} else {
		if (!bound_.render_state.scissor.has_value() ||
			bound_.render_state.scissor.has_value() && bound_.render_state.scissor->enabled) {
			GLCall(glDisable(GL_SCISSOR_TEST));
		}
	}

	bound_.render_state.scissor = scissor;
}

void GLContext::SetRaster(const RasterState& raster) {
	if (bound_.render_state.raster == raster) {
		return;
	}

	PTGN_ASSERT(raster.line_width >= 1.0f, "Only line widths >= 1.0 are supported");

	if (!bound_.render_state.raster.has_value() ||
		bound_.render_state.raster.has_value() &&
			!NearlyEqual(bound_.render_state.raster->line_width, raster.line_width)) {
		GLCall(glLineWidth(raster.line_width));
	}

	if (raster.cull.enabled) {
		if (!bound_.render_state.raster.has_value() ||
			bound_.render_state.raster.has_value() && !bound_.render_state.raster->cull.enabled) {
			GLCall(glEnable(GL_CULL_FACE));
		}
	} else {
		if (!bound_.render_state.raster.has_value() ||
			bound_.render_state.raster.has_value() && bound_.render_state.raster->cull.enabled) {
			GLCall(glDisable(GL_CULL_FACE));
		}
	}

	if (!bound_.render_state.raster.has_value() ||
		bound_.render_state.raster.has_value() &&
			bound_.render_state.raster->cull.cull_face != raster.cull.cull_face) {
		GLCall(glCullFace(std::to_underlying(raster.cull.cull_face)));
	}
	if (!bound_.render_state.raster.has_value() ||
		bound_.render_state.raster.has_value() &&
			bound_.render_state.raster->cull.front_face != raster.cull.front_face) {
		GLCall(glFrontFace(std::to_underlying(raster.cull.front_face)));
	}

	bound_.render_state.raster = raster;
}

void GLContext::SetStencil(const StencilState& stencil) {
	if (bound_.render_state.stencil == stencil) {
		return;
	}

	if (stencil.enabled) {
		if (!bound_.render_state.stencil.has_value() ||
			bound_.render_state.stencil.has_value() && !bound_.render_state.stencil->enabled) {
			GLCall(glEnable(GL_STENCIL_TEST));
		}
	} else {
		if (!bound_.render_state.stencil.has_value() ||
			bound_.render_state.stencil.has_value() && bound_.render_state.stencil->enabled) {
			GLCall(glDisable(GL_STENCIL_TEST));
		}
	}

	if (!bound_.render_state.stencil.has_value() ||
		bound_.render_state.stencil.has_value() &&
			(bound_.render_state.stencil->func != stencil.func ||
			 bound_.render_state.stencil->ref != stencil.ref ||
			 bound_.render_state.stencil->mask != stencil.mask)) {
		GLCall(glStencilFunc(std::to_underlying(stencil.func), stencil.ref, stencil.mask));
	}
	if (!bound_.render_state.stencil.has_value() ||
		bound_.render_state.stencil.has_value() &&
			(bound_.render_state.stencil->fail_op != stencil.fail_op ||
			 bound_.render_state.stencil->zfail_op != stencil.zfail_op ||
			 bound_.render_state.stencil->zpass_op != stencil.zpass_op)) {
		GLCall(glStencilOp(
			std::to_underlying(stencil.fail_op), std::to_underlying(stencil.zfail_op),
			std::to_underlying(stencil.zpass_op)
		));
	}
	if (!bound_.render_state.stencil.has_value() ||
		bound_.render_state.stencil.has_value() &&
			bound_.render_state.stencil->write_mask != stencil.write_mask) {
		GLCall(glStencilMask(stencil.write_mask));
	}

	bound_.render_state.stencil = stencil;
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

void GLContext::InvalidateState() {
	bound_.Invalidate();
	GLCall(glActiveTexture(GL_TEXTURE0));
}

} // namespace ptgn::impl::gl