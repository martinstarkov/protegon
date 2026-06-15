#include "renderer/backend/gl/gl_context.h"

#include <array>
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
#include "renderer/resources/texture.h"

/// @brief Apply X macro with args: mode, src_rgb, dst_rgb, src_alpha, dst_alpha
#define PTGN_BLEND_MODE_TABLE(X)                                                          \
	X(Blend, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)        \
	X(PremultipliedBlend, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA) \
	X(ReplaceRGBA, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO)                                      \
	X(ReplaceRGB, GL_ONE, GL_ZERO, GL_ZERO, GL_ONE)                                       \
	X(ReplaceAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ZERO)                                     \
	X(AddRGB, GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE)                                      \
	X(AddRGBA, GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE)                                      \
	X(AddAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ONE)                                          \
	X(PremultipliedAddRGB, GL_ONE, GL_ONE, GL_ZERO, GL_ONE)                               \
	X(PremultipliedAddRGBA, GL_ONE, GL_ONE, GL_ONE, GL_ONE)                               \
	X(MultiplyRGB, GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE)                                \
	X(MultiplyRGBA, GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO)                         \
	X(MultiplyAlpha, GL_ZERO, GL_ONE, GL_DST_ALPHA, GL_ZERO)                              \
	X(MultiplyRGBWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE)   \
	X(MultiplyRGBAWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ZERO)

namespace ptgn::impl::gl {

namespace {

template <typename T>
T GetInteger(GLenum name) {
	GLint value{ 0 };
	GLCall(glGetIntegerv(name, &value));
	PTGN_ASSERT(value >= 0, "Failed to query parameter: ", name);
	return T{ static_cast<std::uint32_t>(value) };
}

bool GetBoolean(GLenum name) {
	GLboolean value{ GL_FALSE };
	GLCall(glGetBooleanv(name, &value));
	return value == GL_TRUE;
}

float GetFloat(GLenum name) {
	GLfloat value{ 0.0f };
	GLCall(glGetFloatv(name, &value));
	return value;
}

std::uint32_t GetUint(GLenum name) {
	GLint value{ 0 };
	GLCall(glGetIntegerv(name, &value));
	return static_cast<std::uint32_t>(value);
}

CompareFunc GetCompareFunc(GLenum name) {
	GLint value{ 0 };
	GLCall(glGetIntegerv(name, &value));
	return static_cast<CompareFunc>(value);
}

StencilOp GetStencilOp(GLenum name) {
	GLint value{ 0 };
	GLCall(glGetIntegerv(name, &value));
	return static_cast<StencilOp>(value);
}

BlendMode GetCurrentBlendMode() {
	GLint equation_rgb{ GL_FUNC_ADD };
	GLint equation_alpha{ GL_FUNC_ADD };

	GLCall(glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation_rgb));
	GLCall(glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha));

	PTGN_ASSERT(
		equation_rgb == GL_FUNC_ADD && equation_alpha == GL_FUNC_ADD,
		"Unsupported OpenGL blend equation state"
	);

	GLint src_rgb{ GL_ONE };
	GLint dst_rgb{ GL_ZERO };
	GLint src_alpha{ GL_ONE };
	GLint dst_alpha{ GL_ZERO };

	GLCall(glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb));
	GLCall(glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb));
	GLCall(glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha));
	GLCall(glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha));

#define PTGN_BLEND_MATCH_CASE(                                                       \
	mode, expected_src_rgb, expected_dst_rgb, expected_src_alpha, expected_dst_alpha \
)                                                                                    \
	if (src_rgb == expected_src_rgb && dst_rgb == expected_dst_rgb &&                \
		src_alpha == expected_src_alpha && dst_alpha == expected_dst_alpha) {        \
		return BlendMode::mode;                                                      \
	}

	PTGN_BLEND_MODE_TABLE(PTGN_BLEND_MATCH_CASE)

#undef PTGN_BLEND_MATCH_CASE

	PTGN_ERROR(
		"Unknown OpenGL blend mode state: src_rgb=", src_rgb, ", dst_rgb=", dst_rgb,
		", src_alpha=", src_alpha, ", dst_alpha=", dst_alpha
	);
}

DepthMaskState GetCurrentDepthMaskState() {
	GLboolean depth_write{ GL_TRUE };
	GLCall(glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write));

	std::array<GLfloat, 2> depth_range{ 0.0f, 1.0f };
	GLCall(glGetFloatv(GL_DEPTH_RANGE, depth_range.data()));

	return DepthMaskState{
		.write		= depth_write == GL_TRUE,
		.func		= GetCompareFunc(GL_DEPTH_FUNC),
		.range_near = depth_range[0],
		.range_far	= depth_range[1],
	};
}

ColorMaskState GetCurrentColorMaskState() {
	std::array<GLboolean, 4> mask{ GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLCall(glGetBooleanv(GL_COLOR_WRITEMASK, mask.data()));

	return ColorMaskState{
		.red   = mask[0] == GL_TRUE,
		.green = mask[1] == GL_TRUE,
		.blue  = mask[2] == GL_TRUE,
		.alpha = mask[3] == GL_TRUE,
	};
}

StencilState GetCurrentStencilState() {
	return StencilState{
		.enabled	= GetBoolean(GL_STENCIL_TEST),
		.func		= GetCompareFunc(GL_STENCIL_FUNC),
		.ref		= static_cast<int>(GetUint(GL_STENCIL_REF)),
		.mask		= GetUint(GL_STENCIL_VALUE_MASK),
		.fail_op	= GetStencilOp(GL_STENCIL_FAIL),
		.zfail_op	= GetStencilOp(GL_STENCIL_PASS_DEPTH_FAIL),
		.zpass_op	= GetStencilOp(GL_STENCIL_PASS_DEPTH_PASS),
		.write_mask = GetUint(GL_STENCIL_WRITEMASK),
	};
}

CullState GetCurrentCullState() {
	GLint cull_face{ GL_BACK };
	GLCall(glGetIntegerv(GL_CULL_FACE_MODE, &cull_face));

	GLint front_face{ GL_CCW };
	GLCall(glGetIntegerv(GL_FRONT_FACE, &front_face));

	return CullState{
		.enabled	= GetBoolean(GL_CULL_FACE),
		.cull_face	= static_cast<CullFace>(cull_face),
		.front_face = static_cast<FrontFace>(front_face),
	};
}

RasterState GetCurrentRasterState() {
	GLfloat line_width{ 1.0f };
	GLCall(glGetFloatv(GL_LINE_WIDTH, &line_width));

	return RasterState{
		.cull		= GetCurrentCullState(),
		.line_width = line_width,
	};
}

Viewport GetCurrentViewport() {
	std::array<GLint, 4> viewport{ 0, 0, 0, 0 };
	GLCall(glGetIntegerv(GL_VIEWPORT, viewport.data()));

	return Viewport{
		.position = { viewport[0], viewport[1] },
		.size	  = { viewport[2], viewport[3] },
	};
}

ScissorState GetCurrentScissorState() {
	auto enabled{ GetBoolean(GL_SCISSOR_TEST) };

	if (enabled) {
		std::array<GLint, 4> box{ 0, 0, 0, 0 };
		GLCall(glGetIntegerv(GL_SCISSOR_BOX, box.data()));
		return ScissorState{ Viewport{
			.position = { box[0], box[1] },
			.size	  = { box[2], box[3] },
		} };
	} else {
		return ScissorState{ false };
	}
}

} // namespace

GLContext::GLContext(Stats& stats) :
	stats{ stats },
	bound_{ ptgn::impl::gl::GetInteger<std::size_t>(GL_MAX_TEXTURE_IMAGE_UNITS) },
	buffers{ *this },
	shaders{ *this, GetMaxTextureSlots() },
	textures{ *this },
	renderbuffers{ *this },
	framebuffers{ *this },
	vertex_arrays{ *this, ptgn::impl::gl::GetInteger<std::size_t>(GL_MAX_VERTEX_ATTRIBS) } {}

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

	GLCall(glBindRenderbuffer(GL_RENDERBUFFER, id));

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

	PTGN_ASSERT(!id || textures.cache_.Has(id), "Texture ", id, " not found in texture cache");

	GLCall(glBindTexture(GL_TEXTURE_2D, id));
	bound_.texture_units[slot].id = id;

	return BindGuard<TextureId>{ *this, previous, restore_bind };
}

BindGuard<FramebufferId> GLContext::Bind(FramebufferId id, bool restore_bind) {
	auto previous{ GetBoundFramebuffer() };

	if (id == previous) {
		return BindGuard<FramebufferId>{ *this, FramebufferId{}, false };
	}

	GLCall(glBindFramebuffer(GL_FRAMEBUFFER, id));
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

VertexBufferId GLContext::GetBoundVertexBuffer() const {
	return bound_.vertex_buffer;
}

ElementBufferId GLContext::GetBoundElementBuffer() const {
	if (!bound_.vertex_array) {
		return ElementBufferId{ 0 };
	}
	return vertex_arrays.cache_.Get(bound_.vertex_array).element_buffer;
}

UniformBufferId GLContext::GetBoundUniformBuffer() const {
	return bound_.uniform_buffer;
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

		unit = {};
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
	return GetBoundTexture() == id;
}

bool GLContext::IsBound(RenderbufferId id) const {
	return GetBoundRenderbuffer() == id;
}

bool GLContext::IsBound(FramebufferId id) const {
	return GetBoundFramebuffer() == id;
}

bool GLContext::IsBound(VertexArrayId id) const {
	return GetBoundVertexArray() == id;
}

void GLContext::Destroy(VertexBufferId id) {
	buffers.DestroyVertexBuffer(id);
}

void GLContext::Destroy(ElementBufferId id) {
	vertex_arrays.InvalidateElementBuffer(id);
	buffers.DestroyElementBuffer(id);
}

void GLContext::Destroy(UniformBufferId id) {
	buffers.DestroyUniformBuffer(id);
}

void GLContext::Destroy(ShaderId id) {
	shaders.DestroyProgram(id);
}

void GLContext::Destroy(TextureId id) {
	framebuffers.InvalidateTexture(id);
	textures.Destroy(id);
}

void GLContext::Destroy(RenderbufferId id) {
	framebuffers.InvalidateRenderbuffer(id);
	renderbuffers.Destroy(id);
}

void GLContext::Destroy(FramebufferId id) {
	framebuffers.Destroy(id);
}

void GLContext::Destroy(VertexArrayId id) {
	vertex_arrays.Destroy(id);
}

void GLContext::SetBlend(bool enabled, bool force) {
	if (enabled) {
		SetDepthTesting(false, force);
	}

	if (bound_.render_state.blending == enabled && !force) {
		return;
	}

	GLCall(enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND));

	bound_.render_state.blending = enabled;
}

void GLContext::SetDepthTesting(bool enabled, bool force) {
	if (enabled) {
		SetBlend(false, force);
	}

	if (bound_.render_state.depth_testing == enabled && !force) {
		return;
	}

	GLCall(enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST));

	bound_.render_state.depth_testing = enabled;
}

void GLContext::SetBlendMode(BlendMode blend, bool force) {
	SetBlend(true, force);

	if (bound_.render_state.blend_mode == blend && !force) {
		return;
	}

	GLCall(glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));

	switch (blend) {
#define PTGN_BLEND_SET_CASE(mode, src_rgb, dst_rgb, src_alpha, dst_alpha)    \
	case BlendMode::mode:                                                    \
		GLCall(glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha)); \
		break;

		PTGN_BLEND_MODE_TABLE(PTGN_BLEND_SET_CASE)

#undef PTGN_BLEND_SET_CASE

		default: PTGN_ERROR("Unknown BlendMode: ", std::to_underlying(blend));
	}

	bound_.render_state.blend_mode = blend;
}

void GLContext::SetDepthMask(const DepthMaskState& mask) {
	if (bound_.render_state.depth_mask == mask) {
		return;
	}

	if (bound_.render_state.depth_mask.func != mask.func) {
		GLCall(glDepthFunc(std::to_underlying(mask.func)));
	}
	if (bound_.render_state.depth_mask.write != mask.write) {
		GLCall(glDepthMask(mask.write));
	}
	if (!NearlyEqual(bound_.render_state.depth_mask.range_near, mask.range_near) ||
		!NearlyEqual(bound_.render_state.depth_mask.range_far, mask.range_far)) {
		GLCall(glDepthRange(mask.range_near, mask.range_far));
	}

	bound_.render_state.depth_mask = mask;
}

void GLContext::SetViewport(Viewport viewport) {
	PTGN_ASSERT(viewport.size.IsPositive(), "Cannot set viewport with non-positive size");

	if (bound_.render_state.viewport == viewport) {
		return;
	}
	V2_int pos{ Floor(viewport.position) };
	V2_int size{ Ceil(viewport.size) };
	GLCall(glViewport(pos.x, pos.y, size.x, size.y));
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

void GLContext::SetClearDepth(Depth depth) {
	if (bound_.clear_depth == depth) {
		return;
	}
	PTGN_ASSERT(
		depth.value >= 0.0f && depth.value <= 1.0f, "Clear depth must be in range [0.0, 1.0]"
	);
	GLCall(glClearDepth(depth.value));
	bound_.clear_depth = depth;
}

void GLContext::SetClearStencil(Stencil stencil) {
	if (bound_.clear_stencil == stencil) {
		return;
	}
	PTGN_ASSERT(stencil.value >= 0, "glClearStencil: stencil value must be non-negative");
	GLCall(glClearStencil(stencil.value));
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
		if (!bound_.render_state.scissor.enabled) {
			GLCall(glEnable(GL_SCISSOR_TEST));
		}
		if (bound_.render_state.scissor.viewport != scissor.viewport) {
			V2_int pos{ Floor(scissor.viewport.position) };
			V2_int size{ Ceil(scissor.viewport.size) };
			GLCall(glScissor(pos.x, pos.y, size.x, size.y));
		}
	} else {
		if (bound_.render_state.scissor.enabled) {
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

	if (!NearlyEqual(bound_.render_state.raster.line_width, raster.line_width)) {
		GLCall(glLineWidth(raster.line_width));
	}

	if (raster.cull.enabled) {
		if (!bound_.render_state.raster.cull.enabled) {
			GLCall(glEnable(GL_CULL_FACE));
		}
	} else {
		if (bound_.render_state.raster.cull.enabled) {
			GLCall(glDisable(GL_CULL_FACE));
		}
	}

	if (bound_.render_state.raster.cull.cull_face != raster.cull.cull_face) {
		GLCall(glCullFace(std::to_underlying(raster.cull.cull_face)));
	}
	if (bound_.render_state.raster.cull.front_face != raster.cull.front_face) {
		GLCall(glFrontFace(std::to_underlying(raster.cull.front_face)));
	}

	bound_.render_state.raster = raster;
}

void GLContext::SetStencil(const StencilState& stencil) {
	if (bound_.render_state.stencil == stencil) {
		return;
	}

	if (stencil.enabled) {
		if (!bound_.render_state.stencil.enabled) {
			GLCall(glEnable(GL_STENCIL_TEST));
		}
	} else {
		if (bound_.render_state.stencil.enabled) {
			GLCall(glDisable(GL_STENCIL_TEST));
		}
	}

	if (bound_.render_state.stencil.func != stencil.func ||
		bound_.render_state.stencil.ref != stencil.ref ||
		bound_.render_state.stencil.mask != stencil.mask) {
		GLCall(glStencilFunc(std::to_underlying(stencil.func), stencil.ref, stencil.mask));
	}
	if (bound_.render_state.stencil.fail_op != stencil.fail_op ||
		bound_.render_state.stencil.zfail_op != stencil.zfail_op ||
		bound_.render_state.stencil.zpass_op != stencil.zpass_op) {
		GLCall(glStencilOp(
			std::to_underlying(stencil.fail_op), std::to_underlying(stencil.zfail_op),
			std::to_underlying(stencil.zpass_op)
		));
	}
	if (bound_.render_state.stencil.write_mask != stencil.write_mask) {
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

bool GLContext::ViewportCoversFramebuffer(FramebufferId framebuffer) const {
	return bound_.render_state.viewport.position.IsZero() &&
		   bound_.render_state.viewport.size ==
			   V2_float{ textures.GetDesc(framebuffers.GetAttachmentId(framebuffer)).size };
}

bool GLContext::ScissorCoversFramebuffer(FramebufferId framebuffer) const {
	if (!bound_.render_state.scissor.enabled) {
		return true;
	}

	return bound_.render_state.scissor.viewport.position.IsZero() &&
		   bound_.render_state.scissor.viewport.size ==
			   V2_float{ textures.GetDesc(framebuffers.GetAttachmentId(framebuffer)).size };
}

std::uint32_t GLContext::GetActiveTextureSlot() const {
	return bound_.active_texture.slot;
}

void GLContext::ResetState() {
	auto max_texture_slots{ bound_.texture_units.size() };
	auto view_projection{ bound_.render_state.view_projection };

	PTGN_ASSERT(max_texture_slots > 0);

	bound_.render_state = RenderState{
		.viewport		 = GetCurrentViewport(),
		.view_projection = std::move(view_projection),
		.blending		 = GetBoolean(GL_BLEND),
		.blend_mode		 = GetCurrentBlendMode(),
		.depth_testing	 = GetBoolean(GL_DEPTH_TEST),
		.depth_mask		 = GetCurrentDepthMaskState(),
		.color_mask		 = GetCurrentColorMaskState(),
		.stencil		 = GetCurrentStencilState(),
		.scissor		 = GetCurrentScissorState(),
		.raster			 = GetCurrentRasterState(),
	};

	bound_.framebuffer	  = ptgn::impl::gl::GetInteger<FramebufferId>(GL_FRAMEBUFFER_BINDING);
	bound_.renderbuffer	  = ptgn::impl::gl::GetInteger<RenderbufferId>(GL_RENDERBUFFER_BINDING);
	bound_.vertex_buffer  = ptgn::impl::gl::GetInteger<VertexBufferId>(GL_ARRAY_BUFFER_BINDING);
	bound_.uniform_buffer = ptgn::impl::gl::GetInteger<UniformBufferId>(GL_UNIFORM_BUFFER_BINDING);
	bound_.shader_program = ptgn::impl::gl::GetInteger<ShaderId>(GL_CURRENT_PROGRAM);
	bound_.vertex_array	  = ptgn::impl::gl::GetInteger<VertexArrayId>(GL_VERTEX_ARRAY_BINDING);

	GLint active_texture_index{ 0 };
	GLCall(glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture_index));

	bound_.active_texture = ActiveTexture{
		static_cast<std::uint32_t>(active_texture_index - GL_TEXTURE0),
	};

	bound_.texture_units.clear();
	bound_.texture_units.resize(max_texture_slots);

	for (auto i{ 0uz }; i < max_texture_slots; ++i) {
		GLCall(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i)));

		GLint texture_2d{ 0 };
		GLCall(glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_2d));

		bound_.texture_units[i].id = TextureId{ static_cast<std::uint32_t>(texture_2d) };
	}

	GLCall(glActiveTexture(GL_TEXTURE0 + bound_.active_texture.slot));

	GLfloat depth{ 1.0f };
	GLCall(glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth));
	bound_.clear_depth = Depth{ depth };

	GLint stencil{ 0 };
	GLCall(glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &stencil));
	bound_.clear_stencil = Stencil{ stencil };

	std::array<GLfloat, 4> color{ 0.0f, 0.0f, 0.0f, 0.0f };
	GLCall(glGetFloatv(GL_COLOR_CLEAR_VALUE, color.data()));
	bound_.clear_color = Color{ color };
}

} // namespace ptgn::impl::gl