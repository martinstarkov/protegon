#pragma once

#include <cstdint>

#include "core/util/id_map.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/resources/id.h"

namespace ptgn::impl::gl {

class GLContext;

struct VertexArrayCache {
	ElementBufferId element_buffer{ 0 };
	bool layout_set{ false };
};

enum class IndexType : std::uint32_t {
	UnsignedByte  = 0x1401, // GL_UNSIGNED_BYTE
	UnsignedShort = 0x1403, // GL_UNSIGNED_SHORT
	UnsignedInt	  = 0x1405	// GL_UNSIGNED_INT
};

class VertexArrays {
public:
	[[nodiscard]] VertexArrayId CreateVertexArray(
		VertexBufferId vertex_buffer, const BufferLayoutView& vertex_buffer_layout,
		ElementBufferId element_buffer, bool restore_bind = true
	);

	void DestroyVertexArray(VertexArrayId id);

	void SetVertexBuffer(VertexArrayId vertex_array, VertexBufferId vertex_buffer);

	void SetElementBuffer(VertexArrayId vertex_array, ElementBufferId element_buffer);

	void SetBufferLayout(VertexArrayId vertex_array, const BufferLayoutView& layout);

	void DrawElements(
		VertexArrayId vertex_array, int index_count, IndexType index_type,
		PrimitiveMode primitive_mode
	) const;

	void DrawArrays(VertexArrayId vertex_array, int vertex_count, PrimitiveMode primitive_mode)
		const;

private:
	friend class GLContext;

	explicit VertexArrays(GLContext& gl);
	~VertexArrays() noexcept						 = default;
	VertexArrays(const VertexArrays&)				 = delete;
	VertexArrays(VertexArrays&&) noexcept			 = delete;
	VertexArrays& operator=(const VertexArrays&)	 = delete;
	VertexArrays& operator=(VertexArrays&&) noexcept = delete;

	[[nodiscard]] VertexArrayId CreateVertexArray();

	BindGuard<VertexArrayId> BindVertexArray(VertexArrayId vertex_array, bool restore_bind);

	static void SetupVertexAttrib(
		std::uint32_t index, const BufferElement& element, std::int32_t stride
	);

	int GetMaxVertexAttribs() const;

	void InvalidateElementBuffer(ElementBufferId element_buffer);

	GLContext& gl_;

	IdMap<VertexArrayCache> cache_;
};

} // namespace ptgn::impl::gl