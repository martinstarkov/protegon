#include "renderer/gl/gl_renderer.h"

#include <array>
#include <cstdint>

#include "common/assert.h"
#include "core/game.h"
#include "debug/config.h"
#include "debug/debug_system.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/color.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/gl/gl_types.h"
#include "renderer/renderer.h"

namespace ptgn::impl {

#ifndef __EMSCRIPTEN__

void GLRenderer::EnableLineSmoothing() {
	GLCall(glEnable(GL_BLEND));
	GLCall(glEnable(GL_LINE_SMOOTH));
	// GLCall(glLineWidth(1.0f));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Enabled line smoothing");
#endif
}

void GLRenderer::DisableLineSmoothing() {
	GLCall(glDisable(GL_LINE_SMOOTH));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Disabled line smoothing");
#endif
}

void GLRenderer::SetPolygonMode(PolygonMode mode) {
	GLCall(glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(mode)));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Set polygon mode");
#endif
}

#endif

void GLRenderer::SetBlendMode(BlendMode mode) {
	if (game.renderer.bound_.blend_mode == mode) {
		return;
	}
	/*
	if (mode == BlendMode::ReplaceRGBA) {
		// GLCall(glDisable(GL_BLEND));
		// GLCall(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE));
		// TODO: If re-enabling, put the print before this.
		return;
	}
	*/
	DisableDepthTesting();
	GLCall(glEnable(GL_BLEND));
	// GLCall(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE));

	GLCall(BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));
	switch (mode) {
		case BlendMode::Blend:
			GLCall(BlendFuncSeparate(
				GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA
			));
			break;
		case BlendMode::PremultipliedBlend:
			GLCall(BlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
			);
			break;
		case BlendMode::ReplaceRGBA:
			GLCall(BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO));
			break;
		case BlendMode::ReplaceRGB:
			GLCall(BlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE));
			break;
		case BlendMode::ReplaceAlpha:
			GLCall(BlendFuncSeparate(GL_ZERO, GL_ONE, GL_ONE, GL_ZERO));
			break;
		case BlendMode::AddRGB:
			GLCall(BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE));
			break;
		case BlendMode::AddRGBA:
			GLCall(BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE));
			break;
		case BlendMode::AddAlpha: GLCall(BlendFuncSeparate(GL_ZERO, GL_ONE, GL_ONE, GL_ONE)); break;
		case BlendMode::PremultipliedAddRGB:
			GLCall(BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE));
			break;
		case BlendMode::PremultipliedAddRGBA:
			GLCall(BlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE));
			break;
		case BlendMode::MultiplyRGB:
			GLCall(BlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE));
			break;
		case BlendMode::MultiplyRGBA:
			GLCall(BlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO));
			break;
		case BlendMode::MultiplyAlpha:
			GLCall(BlendFuncSeparate(GL_ZERO, GL_ONE, GL_DST_ALPHA, GL_ZERO));
			break;
		case BlendMode::MultiplyRGBWithAlphaBlend:
			GLCall(BlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
			break;
		case BlendMode::MultiplyRGBAWithAlphaBlend:
			GLCall(BlendFuncSeparate(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ZERO));
			break;
		default: PTGN_ERROR("Failed to identify blend mode");
	}
	game.renderer.bound_.blend_mode = mode;
#ifdef PTGN_DEBUG
	++game.debug.stats.blend_mode_changes;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Changed blend mode to ", mode);
#endif
}

void GLRenderer::EnableGammaCorrection() {
#ifndef __EMSCRIPTEN__
	GLCall(glEnable(GL_FRAMEBUFFER_SRGB));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Enabled gamma correction");
#endif
#endif
}

void GLRenderer::DisableGammaCorrection() {
#ifndef __EMSCRIPTEN__
	GLCall(glDisable(GL_FRAMEBUFFER_SRGB));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Disabled gamma correction");
#endif
#endif
}

void GLRenderer::EnableDepthWriting() {
	GLCall(glDepthMask(GL_TRUE));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Enabled depth writing");
#endif
}

void GLRenderer::DisableDepthWriting() {
	GLCall(glDepthMask(GL_FALSE));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Disabled depth writing");
#endif
}

bool GLRenderer::IsDepthTestingEnabled() {
	GLboolean enabled{ GL_FALSE };
	GLCall(glGetBooleanv(GL_DEPTH_TEST, &enabled));
	return static_cast<bool>(enabled);
}

void GLRenderer::EnableDepthTesting() {
	GLCall(glClearDepth(1.0)); /* Enables Clearing Of The Depth Buffer */
	GLCall(glEnable(GL_DEPTH_TEST));
	GLCall(glDepthFunc(GL_LESS));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Enabled depth testing");
#endif
}

void GLRenderer::DisableDepthTesting() {
	GLCall(glDisable(GL_DEPTH_TEST));
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Disabled depth testing");
#endif
}

void GLRenderer::DrawElements(
	const VertexArray& vao, std::size_t index_count, bool bind_vertex_array
) {
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
	GLCall(glDrawElements(
		static_cast<GLenum>(vao.GetPrimitiveMode()), static_cast<std::int32_t>(index_count),
		static_cast<GLenum>(impl::GetType<std::uint32_t>()), nullptr
	));
#ifdef PTGN_DEBUG
	++game.debug.stats.draw_calls;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Draw elements");
#endif
}

void GLRenderer::DrawArrays(
	const VertexArray& vao, std::size_t vertex_count, bool bind_vertex_array
) {
	PTGN_ASSERT(
		vao.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	if (bind_vertex_array) {
		vao.Bind();
	}
	PTGN_ASSERT(vao.IsBound(), "Cannot glDrawArrays unless the VertexArray is bound");
	GLCall(glDrawArrays(
		static_cast<GLenum>(vao.GetPrimitiveMode()), 0, static_cast<std::int32_t>(vertex_count)
	));
#ifdef PTGN_DEBUG
	++game.debug.stats.draw_calls;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Draw arrays");
#endif
}

std::uint32_t GLRenderer::GetMaxTextureSlots() {
	std::int32_t max_texture_slots{ -1 };
	GLCall(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots));
	PTGN_ASSERT(max_texture_slots >= 0, "Failed to retrieve device maximum texture slots");
	return static_cast<std::uint32_t>(max_texture_slots);
}

void GLRenderer::SetClearColor(const Color& color) {
	auto c{ color.Normalized() };
	GLCall(glClearColor(c[0], c[1], c[2], c[3]));
#ifdef PTGN_DEBUG
	++game.debug.stats.clear_colors;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Changed clear color to ", color);
#endif
}

void GLRenderer::SetViewport(const V2_int& position, const V2_int& size) {
	if (game.renderer.bound_.viewport_position == position &&
		game.renderer.bound_.viewport_size == size) {
		return;
	}
	GLCall(glViewport(position.x, position.y, size.x, size.y));
	game.renderer.bound_.viewport_position = position;
	game.renderer.bound_.viewport_size	   = size;
#ifdef PTGN_DEBUG
	++game.debug.stats.viewport_changes;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Set viewport [position: ", position, ", size: ", size, "]");
#endif
}

V2_int GLRenderer::GetViewportSize() {
	std::array<std::int32_t, 4> values{};
	GLCall(glGetIntegerv(GL_VIEWPORT, values.data()));
	return { values[2], values[3] };
}

V2_int GLRenderer::GetViewportPosition() {
	std::array<std::int32_t, 4> values{};
	GLCall(glGetIntegerv(GL_VIEWPORT, values.data()));
	return { values[0], values[1] };
}

void GLRenderer::Clear() {
	// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GLCall(glClear(GL_COLOR_BUFFER_BIT));
#ifdef PTGN_DEBUG
	++game.debug.stats.clears;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Cleared color and depth buffers");
#endif
}

void GLRenderer::ClearToColor(const V4_float& normalized_color) {
	PTGN_ASSERT(normalized_color.x >= 0.0f && normalized_color.x <= 1.0f);
	PTGN_ASSERT(normalized_color.y >= 0.0f && normalized_color.y <= 1.0f);
	PTGN_ASSERT(normalized_color.z >= 0.0f && normalized_color.z <= 1.0f);
	PTGN_ASSERT(normalized_color.w >= 0.0f && normalized_color.w <= 1.0f);

	std::array<float, 4> color_array{ normalized_color.x, normalized_color.y, normalized_color.z,
									  normalized_color.w };

	// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	GLCall(ClearBufferfv(static_cast<GLenum>(BufferCategory::Color), 0, color_array.data()));
	/*
	// TODO: Check image format of bound texture and potentially use glClearBufferuiv instead of
	ClearBufferfv. std::array<std::uint32_t, 4> color_array{ color.r, color.g, color.b, color.a };
	GLCall(ClearBufferuiv(static_cast<GLenum>(BufferCategory::Color), 0,
	color_array.data()));
	*/
#ifdef PTGN_DEBUG
	++game.debug.stats.clears;
#endif
#ifdef GL_ANNOUNCE_RENDERER_CALLS
	PTGN_LOG("GL: Cleared to color ", normalized_color);
#endif
}

void GLRenderer::ClearToColor(const Color& color) {
	ClearToColor(color.Normalized());
}

} // namespace ptgn::impl