#pragma once

#include <cstdint>

#include "core/assert.h"
#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/resources/buffer_layout.h"

namespace ptgn::impl::gl {

class GLContext;

using VertexArray = std::uint32_t;

struct VertexArrayCache {
	ElementBuffer element_buffer{ 0 };
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
	VertexArray CreateVertexArray(
		VertexBuffer vertex_buffer, const BufferLayout<Ts...>& vertex_buffer_layout,
		ElementBuffer element_buffer, bool restore_bind = true
	) {
		auto vertex_array{ CreateVertexArray() };

		auto _ = gl_.Bind(vertex_array, restore_bind);

		SetVertexBuffer(vertex_array, vertex_buffer);
		SetElementBuffer(vertex_array, element_buffer);
		SetBufferLayout(vertex_array, vertex_buffer_layout);

		return vertex_array;
	}

	void DestroyVertexArray(VertexArray id);

	void SetVertexBuffer(VertexArray vertex_array, VertexBuffer vertex_buffer);

	void SetElementBuffer(VertexArray vertex_array, ElementBuffer element_buffer);

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetBufferLayout(VertexArray vertex_array, const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			gl_.IsBound(vertex_array), "Vertex array must be bound before setting its buffer layout"
		);

		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements{ layout.GetElements() };

		PTGN_ASSERT(
			elements.size() < static_cast<std::uint32_t>(gl_.GetInteger(GL_MAX_VERTEX_ATTRIBS)),
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
		VertexArray vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
	) const;

	void DrawArrays(VertexArray vertex_array, GLsizei vertex_count, GLenum primitive_mode) const;

private:
	friend class GLContext;

	explicit VertexArrays(GLContext& gl);
	~VertexArrays() noexcept						 = default;
	VertexArrays(const VertexArrays&)				 = delete;
	VertexArrays(VertexArrays&&) noexcept			 = delete;
	VertexArrays& operator=(const VertexArrays&)	 = delete;
	VertexArrays& operator=(VertexArrays&&) noexcept = delete;

	[[nodiscard]] VertexArray CreateVertexArray();

	GLContext& gl_;

	IdMap<VertexArrayCache> cache_;
};

} // namespace ptgn::impl::gl