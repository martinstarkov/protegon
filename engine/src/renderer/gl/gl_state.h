#pragma once

#include <cstdint>

#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"

#ifdef __EMSCRIPTEN__

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 0;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 3;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

namespace ptgn::impl::gl {

class GLRenderer {
public:
	void EnableGammaCorrection();
	void DisableGammaCorrection();
	void EnableDepthWriting();
	void DisableDepthWriting();

	// Sets the blend mode for the currently bound frame buffer.
	void SetBlendMode(BlendMode mode);

	void EnableDepthTesting();
	void DisableDepthTesting();

	// Sets the viewport dimensions.
	void SetViewport(const V2_int& position, const V2_int& size);

	// @return The size of the viewport.
	[[nodiscard]] V2_int GetViewportSize();

	// @return The top left position of the viewport.
	[[nodiscard]] V2_int GetViewportPosition();

	// Clears the currently bound frame buffer's color and depth buffers.
	void Clear();

	// Sets the clear color for all color buffers.
	void SetClearColor(const Color& color);

	// Clears the currently bound frame buffer's color buffer to the specified color.
	void ClearToColor(const Color& color);

	// Clears the currently bound frame buffer's color buffer to the specified color.
	// @param normalized_color All values must be in range [0, 1].
	void ClearToColor(const V4_float& normalized_color);

	void DrawElements(
		const VertexArray& va, std::size_t index_count, bool bind_vertex_array = true
	);
	void DrawArrays(const VertexArray& va, std::size_t vertex_count, bool bind_vertex_array = true);

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] std::uint32_t GetMaxTextureSlots();

	// @return True if depth testing is enabled, false otherwise.
	[[nodiscard]] bool IsDepthTestingEnabled();
};

} // namespace ptgn::impl::gl