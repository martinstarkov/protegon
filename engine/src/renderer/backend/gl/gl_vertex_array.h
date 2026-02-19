#pragma once

#include <cstdint>

#include "core/assert.h"
#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/buffer_layout.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn::impl::gl {

class GLContext;

struct VertexArrayCache {
	ElementBufferId element_buffer{ 0 };
	bool layout_set{ false };
};

constexpr GLenum ToGLType(BufferElementType type) noexcept {
	switch (type) {
		using enum BufferElementType;

		case Float:	 return GL_FLOAT;
		case Double: return GL_DOUBLE;
		case Int:	 return GL_INT;
		case UInt:	 return GL_UNSIGNED_INT;
		case Short:	 return GL_SHORT;
		case UShort: return GL_UNSIGNED_SHORT;
		case Byte:	 return GL_BYTE;
		case UByte:	 return GL_UNSIGNED_BYTE;
		case Bool:	 return GL_BOOL;
	}
	return GL_FLOAT;
}

class VertexArrays {
public:
	template <typename... Ts>
	VertexArrayId CreateVertexArray(
		VertexBufferId vertex_buffer, const BufferLayout<Ts...>& vertex_buffer_layout,
		ElementBufferId element_buffer, bool restore_bind = true
	) {
		auto vertex_array{ CreateVertexArray() };

		auto _ = BindVertexArray(vertex_array, restore_bind);

		SetVertexBuffer(vertex_array, vertex_buffer);
		SetElementBuffer(vertex_array, element_buffer);
		SetBufferLayout(vertex_array, vertex_buffer_layout);

		return vertex_array;
	}

	void DestroyVertexArray(VertexArrayId id);

	void SetVertexBuffer(VertexArrayId vertex_array, VertexBufferId vertex_buffer);

	void SetElementBuffer(VertexArrayId vertex_array, ElementBufferId element_buffer);

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetBufferLayout(VertexArrayId vertex_array, const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting its buffer layout"
		);

		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements{ layout.GetElements() };

		PTGN_ASSERT(
			elements.size() < static_cast<std::uint32_t>(GetMaxVertexAttribs()),
			"Vertex buffer layout cannot exceed maximum number of vertex array attributes"
		);

		auto stride{ layout.GetStride() };

		PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

		for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
			const auto& element{ elements[i] };
			GLCall(EnableVertexAttribArray(i));
			if (element.is_integer) {
				GLCall(VertexAttribIPointer(
					i, element.count, ToGLType(element.type), stride,
					reinterpret_cast<const void*>(element.offset)
				));
			} else {
				GLCall(VertexAttribPointer(
					i, element.count, ToGLType(element.type),
					element.normalized ? GL_TRUE : GL_FALSE, stride,
					reinterpret_cast<const void*>(element.offset)
				));
			}
		}

		cache_.Get(vertex_array).layout_set = true;
	}

	void DrawElements(
		VertexArrayId vertex_array, GLsizei element_count, GLenum element_type,
		GLenum primitive_mode
	) const;

	void DrawArrays(VertexArrayId vertex_array, GLsizei vertex_count, GLenum primitive_mode) const;

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

	[[nodiscard]] bool IsBound(VertexArrayId vertex_array) const;

	[[nodiscard]] int GetMaxVertexAttribs() const;

	GLContext& gl_;

	IdMap<VertexArrayCache> cache_;
};

} // namespace ptgn::impl::gl