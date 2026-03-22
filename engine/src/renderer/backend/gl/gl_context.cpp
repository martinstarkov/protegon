#include "renderer/backend/gl/gl_context.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

#include <cstdint>
#include <ostream>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/id_map.h"
#include "platform/window/window.h"
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
#include "SDL3/SDL_opengl_glext.h"

namespace ptgn::impl::gl {

enum class VSyncMode : int {
	Immediate = 0, /// No sync
	VSync	  = 1, /// Sync with vertical retrace
	Adaptive  = -1 /// Adaptive vsync (late swap tearing)
};

constexpr VSyncMode kVSyncMode{ VSyncMode::Adaptive };

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

SDLGLContext::SDLGLContext(const Window& window) {
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
	if (!SDL_GL_SetSwapInterval(std::to_underlying(kVSyncMode))) {
		// If no adaptive VSYNC available, fallback to VSYNC.
		SDL_GL_SetSwapInterval(std::to_underlying(VSyncMode::VSync));
	}

	LoadGLFunctions();
}

SDLGLContext::~SDLGLContext() noexcept {
	if (context_) {
		SDL_GL_DestroyContext(context_);
		context_ = nullptr;
		PTGN_INFO("Destroyed OpenGL context");
		// Note: If this is the last message you see and the window does not close, it is likely
		// that a GL asset is destructed after the GL context has been deleted.
	}
}

GLContext::GLContext(const Window& window) :
	context_{ window },
	buffers{ *this },
	shaders{ *this },
	textures{ *this },
	renderbuffers{ *this },
	framebuffers{ *this },
	vertex_arrays{ *this } {
	// NOSONAR
	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

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

	GLCall(BindBuffer(std::to_underlying(target), id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindBuffer(target=", target, ",id=", id, ") (previous=", previous, ")");
#endif
	bound_.vertex_buffer = id;

	return BindGuard<VertexBufferId>{ *this, previous, restore_bind };
}

BindGuard<ElementBufferId> GLContext::Bind(ElementBufferId id, bool restore_bind) {
	auto previous{ GetBoundElementBuffer() };

	if (id == previous) {
		return BindGuard<ElementBufferId>{ *this, ElementBufferId{}, false };
	}

	constexpr BufferTarget target{ BufferTarget::ElementArrayBuffer };

	GLCall(BindBuffer(std::to_underlying(target), id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindBuffer(target=", target, ",id=", id, ") (previous=", previous, ")");
#endif

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

	GLCall(BindBuffer(std::to_underlying(target), id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindBuffer(target=", target, ",id=", id, ") (previous=", previous, ")");
#endif

	bound_.uniform_buffer = id;

	return BindGuard<UniformBufferId>{ *this, previous, restore_bind };
}

BindGuard<ShaderId> GLContext::Bind(ShaderId id, bool restore_bind) {
	auto previous{ GetBoundShader() };

	if (id == previous) {
		return BindGuard<ShaderId>{ *this, ShaderId{}, false };
	}

	GLCall(UseProgram(id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	Print("glUseProgram(id=", id, ")");
	if (auto shader{ shaders.cache_.TryGet(id) }) {
		Print(" (name=", shader->program_name, ")");
	}
	PrintLine(" (previous=", previous, ")");
#endif

	bound_.shader_program = id;

	return BindGuard<ShaderId>{ *this, previous, restore_bind };
}

BindGuard<RenderbufferId> GLContext::Bind(RenderbufferId id, bool restore_bind) {
	auto previous{ GetBoundRenderbuffer() };

	if (id == previous) {
		return BindGuard<RenderbufferId>{ *this, RenderbufferId{}, false };
	}

	constexpr AttachmentObject target{ AttachmentObject::Renderbuffer };

	GLCall(BindRenderbuffer(std::to_underlying(target), id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindRenderbuffer(id=", id, ") (previous=", previous, ")");
#endif

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
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindTexture(id=", id, ") (previous=", previous, ")");
#endif
	bound_.texture_units[slot].id = id;

	return BindGuard<TextureId>{ *this, previous, restore_bind };
}

BindGuard<FramebufferId> GLContext::Bind(FramebufferId id, bool restore_bind) {
	auto previous{ GetBoundFramebuffer() };

	if (id == previous) {
		return BindGuard<FramebufferId>{ *this, FramebufferId{}, false };
	}

	GLCall(BindFramebuffer(kFrameBufferTarget, id));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBindFramebuffer(id=", id, ") (previous=", previous, ")");
#endif
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

		GLCall(BindVertexArray(id));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glBindVertexArray(id=", id, ") (previous=", previous, ")");
#endif

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

void GLContext::EnableGammaCorrection() const {
#ifndef __EMSCRIPTEN__
	GLCall(glEnable(GL_FRAMEBUFFER_SRGB));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glEnable(GL_FRAMEBUFFER_SRGB)");
#endif
#else
	PTGN_WARN("glEnable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
}

void GLContext::DisableGammaCorrection() const {
#ifndef __EMSCRIPTEN__
	GLCall(glDisable(GL_FRAMEBUFFER_SRGB));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glDisable(GL_FRAMEBUFFER_SRGB)");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glEnable(GL_BLEND)");
#endif
	} else {
		GLCall(glDisable(GL_BLEND));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glDisable(GL_BLEND)");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glDepthMask(enabled=", enabled, ")");
#endif
	bound_.depth.write = enabled;
}

void GLContext::SetDepthFunc(CompareFunc depth_func) {
	if (bound_.depth.func == depth_func) {
		return;
	}
	GLCall(glDepthFunc(std::to_underlying(depth_func)));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glDepthFunc(func=", depth_func, ")");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glClearDepth(value=", value, ")");
#endif
		GLCall(glEnable(GL_DEPTH_TEST));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glEnable(GL_DEPTH_TEST)");
#endif
	} else {
		GLCall(glDisable(GL_DEPTH_TEST));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glDisable(GL_DEPTH_TEST)");
#endif
	}
	bound_.depth.test = enabled;
}

void GLContext::SetDepthRange(float near_val, float far_val) {
	if (NearlyEqual(bound_.depth.range_near, near_val) &&
		NearlyEqual(bound_.depth.range_far, far_val)) {
		return;
	}
	GLCall(glDepthRange(near_val, far_val));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glDepthRange(near=", near_val, ",far=", far_val, ")");
#endif
	bound_.depth.range_near = near_val;
	bound_.depth.range_far	= far_val;
}

void GLContext::SetLineWidth(float width) {
	if (NearlyEqual(bound_.raster.line_width.value, width)) {
		return;
	}
	PTGN_ASSERT(width >= 1.0f, "Only line widths >= 1.0 are supported");
	GLCall(glLineWidth(width));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glLineWidth(width=", width, ")");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glEnable(GL_LINE_SMOOTH)");
#endif
	} else {
		GLCall(glDisable(GL_LINE_SMOOTH));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glDisable(GL_LINE_SMOOTH)");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glPolygonMode(face=GL_FRONT_AND_BACK,mode=", front_mode, ")");
#endif
	} else {
		GLCall(glPolygonMode(GL_FRONT, std::to_underlying(front_mode)));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glPolygonMode(face=GL_FRONT,mode=", front_mode, ")");
#endif
		GLCall(glPolygonMode(GL_BACK, std::to_underlying(back_mode)));
#ifdef PTGN_GL_DEBUG_CONTEXT
		PTGN_LOG("glPolygonMode(face=GL_BACK,mode=", back_mode, ")");
#endif
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

#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glBlendMode(", mode, ")");
#endif

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

void GLContext::SetViewport(Viewport viewport) {
	if (bound_.viewport == viewport) {
		return;
	}
	GLCall(glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glViewport(", viewport, ")");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glClearColor(", color, ")");
#endif
	GLCall(glClearColor(n.x, n.y, n.z, n.w));
	bound_.clear_color = ClearColor{ color };
}

void GLContext::SetClearDepth(double depth) {
	if (NearlyEqual(bound_.clear_depth.value, depth)) {
		return;
	}
	PTGN_ASSERT(depth >= 0.0 && depth <= 1.0, "glClearDepth: depth must be in range [0.0, 1.0]");
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glClearDepth(", depth, ")");
#endif
	GLCall(glClearDepth(depth));
	bound_.clear_depth = ClearDepth{ depth };
}

void GLContext::SetClearStencil(int stencil) {
	if (bound_.clear_stencil.value == stencil) {
		return;
	}
	PTGN_ASSERT(stencil >= 0, "glClearStencil: stencil value must be non-negative");
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glClearStencil(", stencil, ")");
#endif
	GLCall(glClearStencil(stencil));
	bound_.clear_stencil = ClearStencil{ stencil };
}

void GLContext::SetColorMask(const ColorMaskState& mask) {
	if (bound_.color_mask == mask) {
		return;
	}
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glColorMask(", mask, ")");
#endif
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
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glScissorState(", scissor, ")");
#endif

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

#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glCullState(", cull, ")");
#endif

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

#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glStencilState(", stencil, ")");
#endif

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

#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glActiveTexture(slot=", slot, ")");
#endif

	bound_.active_texture = ActiveTexture{ slot };
}

std::size_t GLContext::GetMaxTextureSlots() const {
	return bound_.texture_units.size();
}

std::uint32_t GLContext::GetActiveTextureSlot() const {
	return bound_.active_texture.slot;
}

int GLContext::GetInteger(GLenum pname) const {
	int value = -1;
	GLCall(glGetIntegerv(pname, &value));
#ifdef PTGN_GL_DEBUG_CONTEXT
	PTGN_LOG("glGetIntegerv(param=", pname, ") -> value=", value);
#endif
	PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
	return value;
}

} // namespace ptgn::impl::gl