#include "renderer/backend/gl/gl_vertex_array.h"

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_debug.h"
#include "renderer/primitives/buffer.h"
#include "renderer/primitives/buffer_layout.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/vertex_array.h"

namespace ptgn::impl::gl {

VertexArrays::VertexArrays(GLContext& gl) : gl_{ gl } {}

void VertexArrays::SetVertexBuffer(VertexArrayId vertex_array, VertexBufferId vertex_buffer) {
	PTGN_ASSERT(
		gl_.IsBound(vertex_array), "Vertex array must be bound before setting vertex buffer"
	);

	auto _ = gl_.Bind(vertex_buffer, false);
}

void VertexArrays::SetElementBuffer(VertexArrayId vertex_array, ElementBufferId element_buffer) {
	PTGN_ASSERT(
		gl_.IsBound(vertex_array), "Vertex array must be bound before setting element buffer"
	);

	auto _ = gl_.Bind(element_buffer, false);
}

void VertexArrays::SetupVertexAttrib(
	std::uint32_t index, const BufferElement& element, std::int32_t stride
) {
	GLCall(EnableVertexAttribArray(index));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
	PTGN_LOG("glEnableVertexAttribArray(index=", index, ")");
#endif

	if (element.is_integer) {
		GLCall(VertexAttribIPointer(
			index, element.count, std::to_underlying(element.type), stride,
			reinterpret_cast<const void*>(element.offset)
		));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
		PTGN_LOG(
			"glVertexAttribIPointer(index=", index, ",size=", element.count, ",type=", element.type,
			",stride=", stride, ",offset=", element.offset, ")"
		);
#endif
	} else {
		GLCall(VertexAttribPointer(
			index, element.count, std::to_underlying(element.type), element.normalized, stride,
			reinterpret_cast<const void*>(element.offset)
		));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
		PTGN_LOG(
			"glVertexAttribPointer(index=", index, ",size=", element.count, ",type=", element.type,
			",normalized=", element.normalized, ",stride=", stride, ",offset=", element.offset, ")"
		);
#endif
	}
}

void VertexArrays::DrawElements(
	VertexArrayId vertex_array, int index_count, IndexType index_type, PrimitiveMode primitive_mode
) const {
	PTGN_ASSERT(gl_.IsBound(vertex_array));
	PTGN_ASSERT(cache_.Get(vertex_array).layout_set);
	PTGN_ASSERT(gl_.GetBoundElementBuffer());
	GLCall(glDrawElements(
		std::to_underlying(primitive_mode), index_count, std::to_underlying(index_type), nullptr
	));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
	PTGN_LOG(
		"glDrawElements(primitive=", primitive_mode, ",index_count=", index_count,
		",index_type=", index_type, ")"
	);
#endif
}

void VertexArrays::DrawArrays(
	VertexArrayId vertex_array, int vertex_count, PrimitiveMode primitive_mode
) const {
	PTGN_ASSERT(gl_.IsBound(vertex_array));
	PTGN_ASSERT(cache_.Get(vertex_array).layout_set);

	constexpr GLint starting_index{ 0 };
	GLCall(glDrawArrays(std::to_underlying(primitive_mode), starting_index, vertex_count));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
	PTGN_LOG(
		"glDrawArrays(primitive=", primitive_mode, ",first=", starting_index,
		",vertex_count=", vertex_count, ")"
	);
#endif
}

VertexArrayId VertexArrays::CreateVertexArray() {
	VertexArrayId id{ 0 };
	GLCall(GenVertexArrays(1, &id.value));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
	PTGN_LOG("glGenVertexArrays() -> id=", id.value);
#endif
	PTGN_ASSERT(id, "Failed to create vertex array");
	cache_.Add(id, VertexArrayCache{});
	return id;
}

int VertexArrays::GetMaxVertexAttribs() const {
	return gl_.GetInteger(GL_MAX_VERTEX_ATTRIBS);
}

void VertexArrays::InvalidateElementBuffer(ElementBufferId element_buffer) {
	for (auto item : cache_.Items()) {
		VertexArrayId vao{ static_cast<std::uint32_t>(item.id) };
		if (item.value.element_buffer == element_buffer) {
			item.value.element_buffer = {};
			auto _					  = gl_.Bind(vao, true);
			SetElementBuffer(vao, ElementBufferId{ 0 });
		}
	}
}

void VertexArrays::DestroyVertexArray(VertexArrayId id) {
	if (!id) {
		return;
	}
	GLCall(DeleteVertexArrays(1, &id.value));
#ifdef PTGN_GL_DEBUG_VERTEX_ARRAYS
	PTGN_LOG("glDeleteVertexArrays(id=", id.value, ")");
#endif
	cache_.Remove(id);
}

BindGuard<VertexArrayId> VertexArrays::BindVertexArray(
	VertexArrayId vertex_array, bool restore_bind
) {
	return gl_.Bind(vertex_array, restore_bind);
}

[[nodiscard]] bool VertexArrays::IsBound(VertexArrayId vertex_array) const {
	return gl_.IsBound(vertex_array);
}

std::ostream& operator<<(std::ostream& os, PrimitiveMode mode) {
	switch (mode) {
		using enum PrimitiveMode;
		case Points:		return os << "Points";
		case Lines:			return os << "Lines";
		case LineLoop:		return os << "LineLoop";
		case LineStrip:		return os << "LineStrip";
		case Triangles:		return os << "Triangles";
		case TriangleStrip: return os << "TriangleStrip";
		case TriangleFan:	return os << "TriangleFan";
		default:			PTGN_ERROR("Unknown PrimitiveMode: ", std::to_underlying(mode));
	}
}

std::ostream& operator<<(std::ostream& os, IndexType type) {
	switch (type) {
		using enum IndexType;
		case UnsignedByte:	return os << "UnsignedByte";
		case UnsignedShort: return os << "UnsignedShort";
		case UnsignedInt:	return os << "UnsignedInt";
		default:			PTGN_ERROR("Unknown IndexType: ", std::to_underlying(type));
	}
}

} // namespace ptgn::impl::gl