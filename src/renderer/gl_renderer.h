#pragma once

#include <cstdint>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/vertex_array.h"

#ifdef __EMSCRIPTEN__

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 0;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 3;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

namespace ptgn {

namespace impl {

enum class PolygonMode {
	Point = 0x1B00, // GL_POINT
	Line  = 0x1B01, // GL_LINE
	Fill  = 0x1B02, // GL_FILL
};

class GLRenderer {
public:
	static void EnableDepthWriting();
	static void DisableDepthWriting();

	// Sets the blend mode for the currently bound frame buffer.
	static void SetBlendMode(BlendMode mode);

	static void EnableLineSmoothing();
	static void DisableLineSmoothing();

	static void SetPolygonMode(PolygonMode mode);

	static void EnableDepthTesting();
	static void DisableDepthTesting();

	// Sets the viewport dimensions.
	static void SetViewport(const V2_int& position, const V2_int& size);

	// @return The size of the viewport.
	[[nodiscard]] static V2_int GetViewportSize();

	// @return The top left position of the viewport.
	[[nodiscard]] static V2_int GetViewportPosition();

	// Clears the currently bound frame buffer's color and depth buffers.
	static void Clear();

	// Sets the clear color for all color buffers.
	static void SetClearColor(const Color& color);

	// Clears the currently bound frame buffer's color buffer to the specified color.
	static void ClearToColor(const Color& color);

	static void DrawElements(
		const VertexArray& va, std::size_t index_count, bool bind_vertex_array = true
	);
	static void DrawArrays(
		const VertexArray& va, std::size_t vertex_count, bool bind_vertex_array = true
	);

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] static std::uint32_t GetMaxTextureSlots();

	// @return True if depth testing is enabled, false otherwise.
	[[nodiscard]] static bool IsDepthTestingEnabled();
};

} // namespace impl

} // namespace ptgn