#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"
#include "protegon/vertex_array.h"

namespace ptgn {

class GLRenderer {
public:
	static void EnableLineSmoothing();
	static void DisableLineSmoothing();
	static void EnableBlending();
	static void DisableBlending();
	static void EnableDepthTesting();
	static void DisableDepthTesting();
	static void SetViewport(const V2_int& position, const V2_int& size);
	static void Clear();
	static void SetClearColor(const Color& color);
	static void DrawElements(const VertexArray& va, std::size_t index_count = 0);
	static void DrawArrays(const VertexArray& va, std::size_t vertex_count);
	// @param width Line width in pixels.
	static void SetLineWidth(float width);
	static std::int32_t GetMaxTextureSlots();
};

} // namespace ptgn