#include "renderer/gl_renderer.h"

#include <cstdint>

#include "core/game.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/renderer.h"
#include "renderer/vertex_array.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

void GLRenderer::EnableLineSmoothing() {
#ifndef __EMSCRIPTEN__
	GLCall(gl::glEnable(GL_BLEND));
	GLCall(gl::glEnable(GL_LINE_SMOOTH));
	// GLCall(gl::glLineWidth(1.0f));
#endif
}

void GLRenderer::DisableLineSmoothing() {
#ifndef __EMSCRIPTEN__
	GLCall(gl::glDisable(GL_LINE_SMOOTH));
#endif
}

void GLRenderer::SetPolygonMode(PolygonMode mode) {
#ifndef __EMSCRIPTEN__
	GLCall(gl::glPolygonMode(GL_FRONT_AND_BACK, static_cast<gl::GLenum>(mode)));
#endif
}

void GLRenderer::SetBlendMode(BlendMode mode /* = BlendMode::Blend*/) {
	if (mode == game.renderer.bound_blend_mode_) {
		return;
	}
	game.renderer.bound_blend_mode_ = mode;
#ifdef PTGN_DEBUG
	++game.stats.blend_mode_changes;
#endif
	if (mode == BlendMode::None) {
		GLCall(gl::glDisable(GL_BLEND));
		// GLCall(gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE));
		return;
	}
	DisableDepthTesting();
	GLCall(gl::glEnable(GL_BLEND));
	// GLCall(gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE));

	switch (mode) {
		case BlendMode::Blend:	  GLCall(gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)); break;
		case BlendMode::Add:	  GLCall(gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE)); break;
		case BlendMode::Modulate: GLCall(gl::glBlendFunc(GL_ZERO, GL_SRC_COLOR)); break;
		case BlendMode::Multiply:
			// TODO: Check that this works correctly.
			GLCall(gl::glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
			break;
		case BlendMode::Stencil:
			GLCall(gl::BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
			GLCall(gl::BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE));
			break;
		case BlendMode::None:
			PTGN_ERROR("BlendMode::None should be dealt with before enabling blending");
			[[fallthrough]];
		default: PTGN_ERROR("Failed to identify blend mode");
	}
}

void GLRenderer::EnableDepthWriting() {
	GLCall(gl::glDepthMask(GL_TRUE));
}

void GLRenderer::DisableDepthWriting() {
	GLCall(gl::glDepthMask(GL_FALSE));
}

bool GLRenderer::IsDepthTestingEnabled() {
	gl::GLboolean enabled{ GL_FALSE };
	GLCall(gl::glGetBooleanv(GL_DEPTH_TEST, &enabled));
	return static_cast<bool>(enabled);
}

void GLRenderer::EnableDepthTesting() {
#ifdef __EMSCRIPTEN__
	GLCall(gl::glClearDepthf(1.0));
#else
	GLCall(gl::glClearDepth(1.0)); /* Enables Clearing Of The Depth Buffer */
#endif
	GLCall(gl::glEnable(GL_DEPTH_TEST));
	GLCall(gl::glDepthFunc(GL_LESS));
}

void GLRenderer::DisableDepthTesting() {
	GLCall(gl::glDisable(GL_DEPTH_TEST));
}

void GLRenderer::DrawElements(
	const VertexArray& vao, std::size_t index_count, bool bind_vertex_array
) {
	PTGN_ASSERT(vao.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		vao.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	PTGN_ASSERT(
		vao.HasIndexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed index buffer"
	);
	if (bind_vertex_array) {
		vao.Bind();
	}
	PTGN_ASSERT(vao.IsBound(), "Cannot glDrawElements unless the VertexArray is bound");
	GLCall(gl::glDrawElements(
		static_cast<gl::GLenum>(vao.GetPrimitiveMode()), static_cast<std::int32_t>(index_count),
		static_cast<gl::GLenum>(impl::GetType<std::uint32_t>()), nullptr
	));
#ifdef PTGN_DEBUG
	++game.stats.draw_calls;
#endif
}

void GLRenderer::DrawArrays(
	const VertexArray& vao, std::size_t vertex_count, bool bind_vertex_array
) {
	PTGN_ASSERT(vao.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		vao.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	if (bind_vertex_array) {
		vao.Bind();
	}
	PTGN_ASSERT(vao.IsBound(), "Cannot glDrawArrays unless the VertexArray is bound");
	GLCall(gl::glDrawArrays(
		static_cast<gl::GLenum>(vao.GetPrimitiveMode()), 0, static_cast<std::int32_t>(vertex_count)
	));
#ifdef PTGN_DEBUG
	++game.stats.draw_calls;
#endif
}

std::int32_t GLRenderer::GetMaxTextureSlots() {
	std::int32_t max_texture_slots{ -1 };
	GLCall(gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots));
	PTGN_ASSERT(max_texture_slots >= 0, "Failed to retrieve device maximum texture slots");
	return max_texture_slots;
}

void GLRenderer::ClearColor(const Color& color) {
	if (color == game.renderer.bound_clear_color_) {
		return;
	}
	game.renderer.bound_clear_color_ = color;
	auto c{ color.Normalized() };
	GLCall(gl::glClearColor(c[0], c[1], c[2], c[3]));
#ifdef PTGN_DEBUG
	++game.stats.clear_colors;
#endif
}

void GLRenderer::SetViewport(const V2_int& position, const V2_int& size) {
	GLCall(gl::glViewport(position.x, position.y, size.x, size.y));
	// game.window.Center();
#ifdef PTGN_DEBUG
	++game.stats.viewport_changes;
#endif
}

void GLRenderer::Clear() {
	GLCall(gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
#ifdef PTGN_DEBUG
	++game.stats.clears;
#endif
}

} // namespace ptgn
