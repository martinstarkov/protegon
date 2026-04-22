#include "renderer/backend/gl/gl_vertex_array.h"

#include <cstdint>
#include <span>
#include <utility>

#include "core/assert.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/primitive_mode.h"
#include "renderer/resources/id.h"

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
	GLCall(glEnableVertexAttribArray(index));

	if (element.is_integer) {
		GLCall(glVertexAttribIPointer(
			index, element.count, std::to_underlying(element.type), stride,
			reinterpret_cast<const void*>(element.offset)
		));
	} else {
		GLCall(glVertexAttribPointer(
			index, element.count, std::to_underlying(element.type), element.normalized, stride,
			reinterpret_cast<const void*>(element.offset)
		));
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
}

void VertexArrays::DrawArrays(
	VertexArrayId vertex_array, int vertex_count, PrimitiveMode primitive_mode
) const {
	PTGN_ASSERT(gl_.IsBound(vertex_array));
	PTGN_ASSERT(cache_.Get(vertex_array).layout_set);

	constexpr GLint starting_index{ 0 };
	GLCall(glDrawArrays(std::to_underlying(primitive_mode), starting_index, vertex_count));
}

VertexArrayId VertexArrays::CreateVertexArray() {
	VertexArrayId id{ 0 };
	GLCall(glGenVertexArrays(1, &id.value));
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

VertexArrayId VertexArrays::CreateVertexArray(
	VertexBufferId vertex_buffer, const BufferLayoutView& vertex_buffer_layout,
	ElementBufferId element_buffer, bool restore_bind
) {
	auto vertex_array{ CreateVertexArray() };

	auto _ = BindVertexArray(vertex_array, restore_bind);

	SetVertexBuffer(vertex_array, vertex_buffer);
	SetElementBuffer(vertex_array, element_buffer);
	SetBufferLayout(vertex_array, vertex_buffer_layout);

	return vertex_array;
}

void VertexArrays::SetBufferLayout(VertexArrayId vertex_array, const BufferLayoutView& layout) {
	PTGN_ASSERT(
		gl_.IsBound(vertex_array), "Vertex array must be bound before setting its buffer layout"
	);

	PTGN_ASSERT(
		!layout.elements.empty(),
		"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
	);

	const auto& elements{ layout.elements };
	PTGN_ASSERT(
		elements.size() < static_cast<std::uint32_t>(GetMaxVertexAttribs()),
		"Vertex buffer layout cannot exceed maximum number of vertex array attributes"
	);

	auto stride{ layout.stride };

	PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

	for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
		SetupVertexAttrib(i, elements[i], stride);
	}

	cache_.Get(vertex_array).layout_set = true;
}

void VertexArrays::DestroyVertexArray(VertexArrayId id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteVertexArrays(1, &id.value));
	cache_.Remove(id);
}

BindGuard<VertexArrayId> VertexArrays::BindVertexArray(
	VertexArrayId vertex_array, bool restore_bind
) {
	return gl_.Bind(vertex_array, restore_bind);
}

} // namespace ptgn::impl::gl