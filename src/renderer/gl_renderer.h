#pragma once

#include "protegon/color.h"
#include "protegon/vector2.h"
#include "protegon/vertex_array.h"

namespace ptgn {

class GLRenderer {
public:
	static void Init();
	static void SetViewport(const V2_int& position, const V2_int& size);
	static void Clear();
	static void SetClearColor(const Color& color);
	static void DrawElements(const VertexArray& va, std::int32_t index_count = 0);
	static void DrawArrays(const VertexArray& va, std::uint32_t vertex_count);
	static void SetLineWidth(float width);
	static std::int32_t GetMaxTextureSlots();
};

} // namespace ptgn