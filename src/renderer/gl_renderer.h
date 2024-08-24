#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"
#include "protegon/vertex_array.h"

#ifdef __EMSCRIPTEN__

#define PTGN_OPENGL_MAJOR_VERSION	3
#define PTGN_OPENGL_MINOR_VERSION	0
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

#define PTGN_OPENGL_MAJOR_VERSION	3
#define PTGN_OPENGL_MINOR_VERSION	3
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

namespace ptgn {

enum class PolygonMode {
	Point = 0x1B00, // GL_POINT
	Line  = 0x1B01, // GL_LINE
	Fill  = 0x1B02, // GL_FILL
};

class GLRenderer {
public:
	static void SetBlendMode(BlendMode mode = BlendMode::Blend);
	static void EnableLineSmoothing();
	static void DisableLineSmoothing();
	static void EnableDepthTesting();
	static void DisableDepthTesting();
	static void SetViewport(const V2_int& position, const V2_int& size);
	static void Clear();
	static void SetPolygonMode(PolygonMode mode);
	static void SetClearColor(const Color& color);
	static void DrawElements(const VertexArray& va, std::size_t index_count);
	static void DrawArrays(const VertexArray& va, std::size_t vertex_count);
	static std::int32_t GetMaxTextureSlots();
};

} // namespace ptgn