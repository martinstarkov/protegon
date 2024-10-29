#include "renderer/gl_renderer.h"

#include <cstdint>

#include "core/game.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/color.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/vertex_array.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

void GLRenderer::EnableLineSmoothing() {
#ifndef __EMSCRIPTEN__
	GLCall(gl::glEnable(GL_BLEND));
	GLCall(gl::glEnable(GL_LINE_SMOOTH));
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
		default: PTGN_ERROR("Failed to identify blend mode");
	}
}

void GLRenderer::EnableDepthWriting() {
	GLCall(gl::glDepthMask(GL_TRUE));
}

void GLRenderer::DisableDepthWriting() {
	GLCall(gl::glDepthMask(GL_FALSE));
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

void GLRenderer::DrawElements(const VertexArray& va, std::size_t index_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		va.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	PTGN_ASSERT(
		va.HasIndexBuffer(), "Cannot draw vertex array with uninitialized or destroyed index buffer"
	);
	va.Bind();
	PTGN_ASSERT(
		VertexArray::GetBoundId() == static_cast<std::int32_t>(va.Get().id_),
		"Failed to bind vertex array id"
	);
	GLCall(gl::glDrawElements(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), static_cast<std::uint32_t>(index_count),
		static_cast<gl::GLenum>(impl::GetType<std::uint32_t>()), nullptr
	));
}

void GLRenderer::DrawArrays(const VertexArray& va, std::size_t vertex_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		va.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	va.Bind();
	GLCall(gl::glDrawArrays(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), 0, static_cast<std::uint32_t>(vertex_count)
	));
}

std::int32_t GLRenderer::GetMaxTextureSlots() {
	std::int32_t max_texture_slots{ -1 };
	GLCall(gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots));
	PTGN_ASSERT(max_texture_slots >= 0, "Failed to retrieve device maximum texture slots");
	return max_texture_slots;
}

void GLRenderer::ClearColor(const Color& color) {
	auto c = color.Normalized();
	GLCall(gl::glClearColor(c[0], c[1], c[2], c[3]));
}

void GLRenderer::SetViewport(const V2_int& position, const V2_int& size) {
	GLCall(gl::glViewport(position.x, position.y, size.x, size.y));
}

void GLRenderer::Clear() {
	GLCall(gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

} // namespace ptgn