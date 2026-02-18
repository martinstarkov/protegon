#include "renderer/backend/gl/gl_context.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"

/// 0 for immediate updates, 1 for updates synchronized with the vertical retrace, -1 for adaptive
/// vsync.
#define PTGN_VSYNC_MODE -1

namespace ptgn::impl::gl {

struct GLVersion {
	GLVersion() {
		bool r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
		PTGN_ASSERT(r, SDL_GetError());
		r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
		PTGN_ASSERT(r, SDL_GetError());
	}

	friend std::ostream& operator<<(std::ostream& os, const GLVersion& v) {
		os << v.major << "." << v.minor;
		return os;
	}

	int major{ 0 };
	int minor{ 0 };
};

GLContext::GLContext(const Window& window) : buffers{ *this }, shaders{ *this }, textures{ *this } {
	if (context_ != nullptr) {
		int result = SDL_GL_MakeCurrent(window, context_);
		PTGN_ASSERT(!result, SDL_GetError());
		return;
	}

	context_ = SDL_GL_CreateContext(window);
	PTGN_ASSERT(context_, SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	if (!SDL_GL_SetSwapInterval(PTGN_VSYNC_MODE)) {
		// If no adaptive VSYNC available, fallback to VSYNC.
		SDL_GL_SetSwapInterval(1);
	}

	LoadGLFunctions();

	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

	auto max_texture_slots{ static_cast<std::size_t>(GetInteger(GL_MAX_TEXTURE_IMAGE_UNITS)) };
	PTGN_ASSERT(max_texture_slots > 0);
	bound_.texture_units.resize(max_texture_slots, {});

	max_color_attachments_ = static_cast<std::uint32_t>(GetInteger(GL_MAX_COLOR_ATTACHMENTS));
	PTGN_ASSERT(max_color_attachments_ > 0);

	shaders.Populate(max_texture_slots);
}

GLContext::~GLContext() {
	if (context_) {
		SDL_GL_DestroyContext(context_);
		context_ = nullptr;
		PTGN_INFO("Destroyed OpenGL context");
		// Note: If this is the last message you see and the window does not close, it is likely
		// that a GL asset is destructed after the GL context has been deleted.
	}
}

Renderbuffer GLContext::CreateRenderbuffer(V2_int size, GLenum internal_format, bool restore_bind) {
	auto renderbuffer{ CreateRenderbufferImpl() };

	auto _ = Bind(renderbuffer, restore_bind);

	SetRenderbufferStorage(renderbuffer, size, internal_format);

	return renderbuffer;
}

Framebuffer GLContext::CreateFramebuffer(
	std::optional<Texture> texture, GLenum texture_attachment,
	std::optional<Renderbuffer> renderbuffer, GLenum renderbuffer_attachment, bool restore_bind
) {
	PTGN_ASSERT(
		texture.has_value() || renderbuffer.has_value(),
		"Must provide at least one valid image attachment when creating a framebuffer"
	);

	auto framebuffer{ CreateFramebufferImpl() };
	auto _ = Bind(framebuffer, restore_bind);

	if (texture.has_value()) {
		AttachTexture(framebuffer, *texture, texture_attachment);
	}

	if (renderbuffer.has_value()) {
		AttachRenderbuffer(framebuffer, *renderbuffer, renderbuffer_attachment);
	}

	PTGN_ASSERT(FramebufferIsComplete(framebuffer));

	return framebuffer;
}

BindGuard<VertexBuffer> GLContext::Bind(VertexBuffer id, bool restore_bind) {
	auto previous{ GetBoundVertexBuffer() };

	if (id == previous) {
		return BindGuard<VertexBuffer>{ *this, VertexBuffer{}, false };
	}

	GLCall(BindBuffer(GL_ARRAY_BUFFER, id));
	bound_.vertex_buffer = id;

	return BindGuard<VertexBuffer>{ *this, previous, restore_bind };
}

BindGuard<ElementBuffer> GLContext::Bind(ElementBuffer id, bool restore_bind) {
	auto previous{ GetBoundElementBuffer() };

	if (id == previous) {
		return BindGuard<ElementBuffer>{ *this, ElementBuffer{}, false };
	}

	GLCall(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));

	if (bound_.vertex_array) {
		vertex_array_cache_.Get(bound_.vertex_array).element_buffer = id;
	}

	return BindGuard<ElementBuffer>{ *this, previous, restore_bind };
}

BindGuard<UniformBuffer> GLContext::Bind(UniformBuffer id, bool restore_bind) {
	auto previous{ GetBoundUniformBuffer() };

	if (id == previous) {
		return BindGuard<UniformBuffer>{ *this, UniformBuffer{}, false };
	}

	GLCall(BindBuffer(GL_UNIFORM_BUFFER, id));
	bound_.uniform_buffer = id;

	return BindGuard<UniformBuffer>{ *this, previous, restore_bind };
}

BindGuard<Program> GLContext::Bind(Program id, bool restore_bind) {
	auto previous{ GetBoundProgram() };

	if (id == previous) {
		return BindGuard<Program>{ *this, Program{}, false };
	}

	GLCall(UseProgram(id));
	bound_.shader_program = id;

	return BindGuard<Program>{ *this, previous, restore_bind };
}

BindGuard<Renderbuffer> GLContext::Bind(Renderbuffer id, bool restore_bind) {
	auto previous{ GetBoundRenderbuffer() };

	if (id == previous) {
		return BindGuard<Renderbuffer>{ *this, Renderbuffer{}, false };
	}

	GLCall(BindRenderbuffer(GL_RENDERBUFFER, id));
	bound_.renderbuffer = id;

	return BindGuard<Renderbuffer>{ *this, previous, restore_bind };
}

BindGuard<Texture> GLContext::Bind(Texture id, bool restore_bind) {
	auto previous{ GetBoundTexture() };

	if (id == previous) {
		return BindGuard<Texture>{ *this, Texture{}, false };
	}

	auto slot{ GetActiveTextureSlot() };
	PTGN_ASSERT(slot < GetMaxTextureSlots(), "Slot out of range of max slots");
	PTGN_ASSERT(bound_.texture_units[slot].id != id);

	GLCall(glBindTexture(GL_TEXTURE_2D, id));
	bound_.texture_units[slot].id = id;

	return BindGuard<Texture>{ *this, previous, restore_bind };
}

BindGuard<Framebuffer> GLContext::Bind(Framebuffer id, bool restore_bind) {
	auto previous{ GetBoundFramebuffer() };

	if (id == previous) {
		return BindGuard<Framebuffer>{ *this, Framebuffer{}, false };
	}

	GLCall(BindFramebuffer(GL_FRAMEBUFFER, id));
	bound_.framebuffer = id;

	return BindGuard<Framebuffer>{ *this, previous, restore_bind };
}

BindGuard<VertexArray> GLContext::Bind(VertexArray id, bool restore_bind) {
	auto previous{ GetBoundVertexArray() };

	if (id == previous) {
		return BindGuard<VertexArray>{ *this, VertexArray{}, false };
	}

#ifdef PTGN_PLATFORM_MACOS
	if (id) {
		GLCall(BindVertexArray(id));
	}
#else
	GLCall(BindVertexArray(id));
#endif

	bound_.vertex_array = id;

	return BindGuard<VertexArray>{ *this, previous, restore_bind };
}

VertexBuffer GLContext::GetBoundVertexBuffer() const {
	return bound_.vertex_buffer;
}

ElementBuffer GLContext::GetBoundElementBuffer() const {
	return bound_.vertex_array ? vertex_array_cache_.Get(bound_.vertex_array).element_buffer
							   : ElementBuffer{ 0 };
}

UniformBuffer GLContext::GetBoundUniformBuffer() const {
	return bound_.uniform_buffer;
}

const State& GLContext::GetBoundState() const {
	return bound_;
}

State& GLContext::GetBoundState() {
	return bound_;
}

Program GLContext::GetBoundProgram() const {
	return bound_.shader_program;
}

Texture GLContext::GetBoundTexture() const {
	PTGN_ASSERT(bound_.active_texture.slot < GetMaxTextureSlots());
	return bound_.texture_units[bound_.active_texture.slot].id;
}

Renderbuffer GLContext::GetBoundRenderbuffer() const {
	return bound_.renderbuffer;
}

Framebuffer GLContext::GetBoundFramebuffer() const {
	return bound_.framebuffer;
}

VertexArray GLContext::GetBoundVertexArray() const {
	return bound_.vertex_array;
}

bool GLContext::IsBound(VertexBuffer id) const {
	return GetBoundVertexBuffer() == id;
}

bool GLContext::IsBound(ElementBuffer id) const {
	return GetBoundElementBuffer() == id;
}

bool GLContext::IsBound(UniformBuffer id) const {
	return GetBoundUniformBuffer() == id;
}

bool GLContext::IsBound(Program id) const {
	return GetBoundProgram() == id;
}

bool GLContext::IsBound(Renderbuffer id) const {
	return GetBoundRenderbuffer() == id;
}

bool GLContext::IsBound(Texture id) const {
	return GetBoundTexture() == id;
}

bool GLContext::IsBound(Framebuffer id) const {
	return GetBoundFramebuffer() == id;
}

bool GLContext::IsBound(VertexArray id) const {
	return GetBoundVertexArray() == id;
}

void GLContext::AttachTexture(Framebuffer framebuffer, Texture texture, GLenum texture_attachment) {
	PTGN_ASSERT(IsBound(framebuffer), "Framebuffer must be bound before attaching a texture");

	if (texture) {
		PTGN_ASSERT(textures.cache_.Has(texture), "Texture not in cache");
		PTGN_ASSERT(
			textures.cache_.Get(texture).size.BothAboveZero(),
			"Cannot attach a texture with no size"
		);
	}

	GLCall(FramebufferTexture2D(GL_FRAMEBUFFER, texture_attachment, GL_TEXTURE_2D, texture, 0));

	UpdateFramebufferCache(framebuffer, texture, texture_attachment, GL_TEXTURE_2D);
}

void GLContext::AttachRenderbuffer(
	Framebuffer framebuffer, Renderbuffer renderbuffer, GLenum renderbuffer_attachment
) {
	PTGN_ASSERT(IsBound(framebuffer), "Framebuffer must be bound before attaching a renderbuffer");

	if (renderbuffer) {
		PTGN_ASSERT(renderbuffer_cache_.Has(renderbuffer), "Renderbuffer not in cache");
		PTGN_ASSERT(
			renderbuffer_cache_.Get(renderbuffer).size.BothAboveZero(),
			"Cannot attach a renderbuffer with no size"
		);
	}

	GLCall(FramebufferRenderbuffer(
		GL_FRAMEBUFFER, renderbuffer_attachment, GL_RENDERBUFFER, renderbuffer
	));

	UpdateFramebufferCache(framebuffer, renderbuffer, renderbuffer_attachment, GL_RENDERBUFFER);
}

void GLContext::SetVertexBuffer(VertexArray vertex_array, VertexBuffer vertex_buffer) {
	PTGN_ASSERT(IsBound(vertex_array), "Vertex array must be bound before setting vertex buffer");

	auto _ = Bind(vertex_buffer, false);
}

void GLContext::SetElementBuffer(VertexArray vertex_array, ElementBuffer element_buffer) {
	PTGN_ASSERT(IsBound(vertex_array), "Vertex array must be bound before setting element buffer");

	auto _ = Bind(element_buffer, false);
}

void GLContext::EnableGammaCorrection() const {
#ifndef __EMSCRIPTEN__
	GLCall(glEnable(GL_FRAMEBUFFER_SRGB));
#else
	PTGN_WARN("glEnable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
}

void GLContext::DisableGammaCorrection() const {
#ifndef __EMSCRIPTEN__
	GLCall(glDisable(GL_FRAMEBUFFER_SRGB));
#else
	PTGN_WARN("glDisable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
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
		GLCall(glClearDepth(1.0));
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

void GLContext::SetLineSmoothing(bool enabled) {
	if (bound_.raster.line_smoothing == enabled) {
		return;
	}
#ifndef __EMSCRIPTEN__
	if (enabled) {
		SetBlending(GL_TRUE);
		GLCall(glEnable(GL_LINE_SMOOTH));
	} else {
		GLCall(glDisable(GL_LINE_SMOOTH));
	}
#else
	if (enabled) {
		PTGN_WARN("GL_LINE_SMOOTH not supported by Emscripten");
	}
#endif
	bound_.raster.line_smoothing = enabled;
}

void GLContext::SetPolygonMode(PolygonMode front_mode, PolygonMode back_mode) {
#ifndef __EMSCRIPTEN__
	if (bound_.raster.polygon.front == front_mode && bound_.raster.polygon.back == back_mode) {
		return;
	}

	if (front_mode == back_mode) {
		GLCall(glPolygonMode(GL_FRONT_AND_BACK, std::to_underlying(front_mode)));
	} else {
		GLCall(glPolygonMode(GL_FRONT, std::to_underlying(front_mode)));
		GLCall(glPolygonMode(GL_BACK, std::to_underlying(back_mode)));
	}

	bound_.raster.polygon.front = front_mode;
	bound_.raster.polygon.back	= back_mode;
#else
	PTGN_WARN("glPolygonMode not supported by Emscripten");
#endif
}

void GLContext::SetBlendMode(BlendMode mode) {
	SetBlending(GL_TRUE);

	if (bound_.blend.mode == mode) {
		return;
	}

	GLCall(BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));

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

void GLContext::DrawElements(
	VertexArray vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
) const {
	PTGN_ASSERT(IsBound(vertex_array));
	PTGN_ASSERT(vertex_array_cache_.Get(vertex_array).layout_set);
	PTGN_ASSERT(GetBoundElementBuffer());

	GLCall(glDrawElements(primitive_mode, element_count, element_type, nullptr));
}

void GLContext::DrawArrays(VertexArray vertex_array, GLsizei vertex_count, GLenum primitive_mode)
	const {
	PTGN_ASSERT(IsBound(vertex_array));
	PTGN_ASSERT(vertex_array_cache_.Get(vertex_array).layout_set);

	constexpr GLint starting_index{ 0 };
	GLCall(glDrawArrays(primitive_mode, starting_index, vertex_count));
}

void GLContext::SetViewport(const Viewport& viewport) {
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

void GLContext::Clear(GLbitfield buffer_bits) const {
	GLCall(glClear(buffer_bits));
}

void GLContext::ClearToColor(Framebuffer framebuffer, Color color, GLenum buffer, GLint drawbuffer)
	const {
	PTGN_ASSERT(IsBound(framebuffer));
	PTGN_ASSERT(drawbuffer >= 0, "Drawbuffer cannot be negative");
	PTGN_ASSERT(
		buffer == GL_COLOR && static_cast<GLuint>(drawbuffer) < max_color_attachments_ ||
			buffer != GL_COLOR && drawbuffer == 0,
		"Drawbuffer must be 0 for depth and stencil buffers and within max color attachments for "
		"color buffers"
	);
	auto c{ static_cast<V4_float>(color) };
	GLCall(ClearBufferfv(buffer, drawbuffer, c.Data()));
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
		GLCall(glEnable(GL_SCISSOR_TEST));
		GLCall(glScissor(scissor.position.x, scissor.position.y, scissor.size.x, scissor.size.y));
	} else {
		GLCall(glDisable(GL_SCISSOR_TEST));
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
	SetLineSmoothing(raster.line_smoothing);
	SetPolygonMode(raster.polygon.front, raster.polygon.back);
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
	GLCall(::ActiveTexture(GL_TEXTURE0 + slot));
	bound_.active_texture = ActiveTexture{ slot };
}

std::size_t GLContext::GetMaxTextureSlots() const {
	return bound_.texture_units.size();
}

GLContext::PixelValue GLContext::ReadPixel(
	Framebuffer framebuffer, V2_int coordinate, GLenum attachment
) {
	auto _1 = Bind(framebuffer, true);

	auto type		 = GetAttachmentDataType(attachment);
	const auto& info = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(info.id != 0, "No image attached to that attachment");

	V2_int size;
	if (info.type == GL_TEXTURE_2D) {
		size = textures.cache_.Get(info.id).size;
	} else {
		size = renderbuffer_cache_.Get(info.id).size;
	}

	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer size"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer size"
	);

	int read_y = size.y - 1 - coordinate.y;

	if (type == AttachmentDataType::Color) {
		const auto& tex = textures.cache_.Get(info.id);

		int components = GetColorComponentCount(tex.internal_format);
		PTGN_ASSERT(components >= 3 && components <= 4);

		std::array<std::uint8_t, 4> v{ 0, 0, 0, 255 };

		GLCall(glReadPixels(
			coordinate.x, read_y, 1, 1, tex.internal_format, GL_UNSIGNED_BYTE, v.data()
		));

		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}

	if (type == AttachmentDataType::Depth) {
		float depth = 0.0f;
		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth));
		return depth;
	}

	if (type == AttachmentDataType::Stencil) {
		std::uint8_t stencil = 0;
		GLCall(
			glReadPixels(coordinate.x, read_y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil)
		);
		return stencil;
	}

	if (type == AttachmentDataType::DepthStencil) {
		// GL_DEPTH_STENCIL returns two integers: depth + stencil packed.
		struct {
			std::uint32_t depth;
			std::uint8_t stencil;
		} ds{};

		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &ds)
		);

		float depth = (ds.depth & 0xFFFFFF) / float(0xFFFFFF);
		return std::make_pair(depth, ds.stencil);
	}

	PTGN_ERROR("Unhandled attachment type");
}

GLContext::PixelBuffer GLContext::ReadPixels(Framebuffer framebuffer, GLenum attachment) {
	auto type = GetAttachmentDataType(attachment);

	const auto& info = GetFramebufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(info.id != 0);

	auto _ = Bind(framebuffer, true);

	V2_int size = (info.type == GL_TEXTURE_2D) ? textures.cache_.Get(info.id).size
											   : renderbuffer_cache_.Get(info.id).size;

	GLenum format	 = GL_RGBA;
	GLenum type_enum = GL_UNSIGNED_BYTE;

	switch (type) {
		using enum AttachmentDataType;

		case Color: {
			const auto& tex = textures.cache_.Get(info.id);
			PTGN_ASSERT(GetColorComponentCount(tex.internal_format) >= 3);
			format	  = tex.internal_format;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		}
		case Depth:
			format	  = GL_DEPTH_COMPONENT;
			type_enum = GL_FLOAT;
			break;
		case Stencil:
			format	  = GL_STENCIL_INDEX;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		case DepthStencil:
			format	  = GL_DEPTH_STENCIL;
			type_enum = GL_UNSIGNED_INT_24_8;
			break;
	}

	// Allocate max possible size (RGBA8 worst case)
	std::vector<std::uint8_t> buffer(size.x * size.y * 4);

	GLCall(glReadPixels(0, 0, size.x, size.y, format, type_enum, buffer.data()));

	return PixelBuffer{ .size = size, .type = type, .data = std::move(buffer) };
}

bool GLContext::FramebufferIsComplete(Framebuffer framebuffer) const {
	PTGN_ASSERT(IsBound(framebuffer), "Cannot check status of framebuffer until it is bound");
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* GLContext::GetFramebufferStatus() const {
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:  return "Framebuffer is complete.";
		case GL_FRAMEBUFFER_UNDEFINED: return "Framebuffer is undefined (no framebuffer bound).";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			return "Incomplete attachment: One or more framebuffer attachment points are "
				   "incomplete.";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			return "Missing attachment: No images are attached to the framebuffer.";
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			return "Incomplete draw buffer: Draw buffer points to a missing attachment.";
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			return "Incomplete read buffer: Read buffer points to a missing attachment.";
		case GL_FRAMEBUFFER_UNSUPPORTED:
			return "Framebuffer unsupported: Format combination not supported by "
				   "implementation.";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			return "Incomplete multisample: Mismatched sample counts or improper use of "
				   "multisampling.";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			return "Incomplete layer targets: Layered attachments are not all complete or "
				   "not "
				   "matching.";
		default: PTGN_ERROR("Unknown framebuffer status.");
	}
}

GLContext::AttachmentDataType GLContext::GetAttachmentDataType(GLenum attachment) const {
	if (attachment >= GL_COLOR_ATTACHMENT0 &&
		attachment < GL_COLOR_ATTACHMENT0 + max_color_attachments_) {
		return AttachmentDataType::Color;
	}

	if (attachment == GL_DEPTH_ATTACHMENT) {
		return AttachmentDataType::Depth;
	}

	if (attachment == GL_STENCIL_ATTACHMENT) {
		return AttachmentDataType::Stencil;
	}

	if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
		return AttachmentDataType::DepthStencil;
	}

	PTGN_ERROR("Unsupported framebuffer attachment");
}

AttachmentInfo& GLContext::GetFramebufferAttachment(Framebuffer framebuffer, GLenum attachment) {
	return const_cast<AttachmentInfo&>(
		std::as_const(*this).GetFramebufferAttachment(framebuffer, attachment)
	);
}

const AttachmentInfo& GLContext::GetFramebufferAttachment(
	Framebuffer framebuffer, GLenum attachment
) const {
	const auto& cache = framebuffer_cache_.Get(framebuffer);

	if (attachment >= GL_COLOR_ATTACHMENT0 &&
		attachment < GL_COLOR_ATTACHMENT0 + max_color_attachments_) {
		PTGN_ASSERT(
			attachment >= GL_COLOR_ATTACHMENT0 &&
				attachment < GL_COLOR_ATTACHMENT0 + cache.color.size(),
			"Color attachment out of valid range"
		);
		auto idx{ attachment - GL_COLOR_ATTACHMENT0 };
		return cache.color[idx];
	} else if (attachment == GL_DEPTH_ATTACHMENT) {
		return cache.depth;
	} else if (attachment == GL_STENCIL_ATTACHMENT) {
		return cache.stencil;
	} else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
		return cache.depth_stencil;
	} else {
		PTGN_ERROR("Unsupported framebuffer attachment enum");
	}
}

void GLContext::UpdateFramebufferCache(
	Framebuffer framebuffer, GLuint image_id, GLenum attachment, GLenum image_type
) {
	PTGN_ASSERT(image_type == GL_TEXTURE_2D || image_type == GL_RENDERBUFFER, "Invalid image type");
	auto& info{ GetFramebufferAttachment(framebuffer, attachment) };
	info.id	  = image_id;
	info.type = image_id ? image_type : 0;
}

void GLContext::ResizeFramebuffer(Framebuffer framebuffer, V2_int new_size) {
	const auto& cache = framebuffer_cache_.Get(framebuffer);

	auto resize_attachment = [&](const AttachmentInfo& info) {
		if (info.id == 0) {
			return;
		}

		if (info.type == GL_TEXTURE_2D) {
			textures.ResizeTexture(Texture{ info.id }, new_size);
		} else if (info.type == GL_RENDERBUFFER) {
			ResizeRenderbuffer(Renderbuffer{ info.id }, new_size);
		} else {
			PTGN_ERROR("Unknown framebuffer attachment type");
		}
	};

	for (const auto& color : cache.color) {
		resize_attachment(color);
	}

	resize_attachment(cache.depth);
	resize_attachment(cache.stencil);
	resize_attachment(cache.depth_stencil);
}

void GLContext::ResizeRenderbuffer(Renderbuffer renderbuffer, V2_int new_size) {
	PTGN_ASSERT(renderbuffer);

	const auto& cache = renderbuffer_cache_.Get(renderbuffer);

	if (cache.size == new_size) {
		return;
	}

	auto _ = Bind(renderbuffer, true);

	SetRenderbufferStorage(renderbuffer, new_size, cache.internal_format);
}

void GLContext::SetRenderbufferStorage(
	Renderbuffer renderbuffer, V2_int size, GLenum internal_format
) {
	PTGN_ASSERT(IsBound(renderbuffer), "Renderbuffer must be bound prior to setting its storage");

	GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

	auto& cache			  = renderbuffer_cache_.Get(renderbuffer);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

std::uint32_t GLContext::GetActiveTextureSlot() const {
	return bound_.active_texture.slot;
}

VertexArray GLContext::CreateVertexArrayImpl() {
	VertexArray id{ 0 };
	GLCall(GenVertexArrays(1, &id));
	PTGN_ASSERT(id, "Failed to create vertex array");
	vertex_array_cache_.Add(id, VertexArrayCache{});
	return id;
}

void GLContext::DestroyVertexArray(VertexArray id) {
	if (!id) {
		return;
	}
	GLCall(DeleteVertexArrays(1, &id));
	vertex_array_cache_.Remove(id);
}

Framebuffer GLContext::CreateFramebufferImpl() {
	Framebuffer id{ 0 };
	GLCall(GenFramebuffers(1, &id));
	PTGN_ASSERT(id, "Failed to create framebuffer");
	framebuffer_cache_.Add(id, FramebufferCache{});
	return id;
}

void GLContext::DestroyFramebuffer(Framebuffer id) {
	if (!id) {
		return;
	}
	GLCall(DeleteFramebuffers(1, &id));
	framebuffer_cache_.Remove(id);
}

Renderbuffer GLContext::CreateRenderbufferImpl() {
	Renderbuffer id{ 0 };
	GLCall(GenRenderbuffers(1, &id));
	PTGN_ASSERT(id, "Failed to create renderbuffer");
	renderbuffer_cache_.Add(id, RenderbufferCache{});
	return id;
}

void GLContext::DestroyRenderbuffer(Renderbuffer id) {
	if (!id) {
		return;
	}
	GLCall(DeleteRenderbuffers(1, &id));
	renderbuffer_cache_.Remove(id);
}

void GLContext::SavePNG(const path& path, Framebuffer framebuffer, GLenum attachment) {
	// Ensure output directory exists
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	// Read all pixels from the framebuffer attachment
	PixelBuffer pb = ReadPixels(framebuffer, attachment);

	PTGN_ASSERT(pb.type == AttachmentDataType::Color, "SavePNG only supports color attachments");

	const V2_int size	   = pb.size;
	constexpr int channels = 4;

	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size.x) * size.y * channels);

	// Convert PixelBuffer -> tightly packed RGBA8
	ForEachPixel(pb, [&rgba, size](V2_int pos, const PixelValue& px) {
		const Color* c = std::get_if<Color>(&px);
		PTGN_ASSERT(c != nullptr);

		const std::size_t idx = static_cast<std::size_t>(pos.y * size.x + pos.x) * channels;

		rgba[idx + 0] = c->r;
		rgba[idx + 1] = c->g;
		rgba[idx + 2] = c->b;
		rgba[idx + 3] = c->a;
	});

	SDL_Surface* surface = SDL_CreateSurfaceFrom(
		size.x, size.y, SDL_PIXELFORMAT_RGBA32, rgba.data(), size.x * channels
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	auto saved{ IMG_SavePNG(surface, path.string().c_str()) };

	PTGN_ASSERT(saved, SDL_GetError());

	SDL_DestroySurface(surface);
}

int GLContext::GetInteger(GLenum pname) const {
	int value = -1;
	GLCall(glGetIntegerv(pname, &value));
	PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
	return value;
}

} // namespace ptgn::impl::gl