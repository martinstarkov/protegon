#pragma once

#include "protegon/vertex_array.h"
#include "protegon/color.h"
#include "protegon/vector2.h"

namespace ptgn {

class GLRenderer {
public:
	static void Init();
	static void SetSize(const V2_int& size);
	static void Clear();
	static void SetClearColor(const Color& color);
	static void DrawIndexed(const VertexArray& va, std::int32_t index_count = 0);
	static void DrawLines(const VertexArray& va, std::uint32_t vertex_count);
	static void SetLineWidth(float width);
};

} // namespace ptgn