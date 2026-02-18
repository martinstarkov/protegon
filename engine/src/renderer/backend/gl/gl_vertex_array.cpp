#include "renderer/backend/gl/gl_vertex_array.h"

#include "core/assert.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_bind_guard.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn::impl::gl {

VertexArrays::VertexArrays(GLContext& gl) : gl_{ gl } {}

void VertexArrays::SetVertexBuffer(VertexArray vertex_array, VertexBuffer vertex_buffer) {
	PTGN_ASSERT(
		gl_.IsBound(vertex_array), "Vertex array must be bound before setting vertex buffer"
	);

	auto _ = gl_.Bind(vertex_buffer, false);
}

void VertexArrays::SetElementBuffer(VertexArray vertex_array, ElementBuffer element_buffer) {
	PTGN_ASSERT(
		gl_.IsBound(vertex_array), "Vertex array must be bound before setting element buffer"
	);

	auto _ = gl_.Bind(element_buffer, false);
}

void VertexArrays::DrawElements(
	VertexArray vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
) const {
	PTGN_ASSERT(gl_.IsBound(vertex_array));
	PTGN_ASSERT(cache_.Get(vertex_array).layout_set);
	PTGN_ASSERT(gl_.GetBoundElementBuffer());

	GLCall(glDrawElements(primitive_mode, element_count, element_type, nullptr));
}

void VertexArrays::DrawArrays(VertexArray vertex_array, GLsizei vertex_count, GLenum primitive_mode)
	const {
	PTGN_ASSERT(gl_.IsBound(vertex_array));
	PTGN_ASSERT(cache_.Get(vertex_array).layout_set);

	constexpr GLint starting_index{ 0 };
	GLCall(glDrawArrays(primitive_mode, starting_index, vertex_count));
}

VertexArray VertexArrays::CreateVertexArray() {
	VertexArray id{ 0 };
	GLCall(GenVertexArrays(1, &id.value));
	PTGN_ASSERT(id, "Failed to create vertex array");
	cache_.Add(id, VertexArrayCache{});
	return id;
}

int VertexArrays::GetMaxVertexAttribs() const {
	return gl_.GetInteger(GL_MAX_VERTEX_ATTRIBS);
}

void VertexArrays::DestroyVertexArray(VertexArray id) {
	if (!id) {
		return;
	}
	GLCall(DeleteVertexArrays(1, &id.value));
	cache_.Remove(id);
}

BindGuard<VertexArray> VertexArrays::BindVertexArray(VertexArray vertex_array, bool restore_bind) {
	return gl_.Bind(vertex_array, restore_bind);
}

[[nodiscard]] bool VertexArrays::IsBound(VertexArray vertex_array) const {
	return gl_.IsBound(vertex_array);
}

} // namespace ptgn::impl::gl