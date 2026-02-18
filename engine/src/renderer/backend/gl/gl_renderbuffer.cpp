#include "renderer/backend/gl/gl_renderbuffer.h"

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/renderbuffer.h"

namespace ptgn::impl::gl {

Renderbuffers::Renderbuffers(GLContext& gl) : gl_{ gl } {}

Renderbuffer Renderbuffers::CreateRenderbuffer(
	V2_int size, GLenum internal_format, bool restore_bind
) {
	auto renderbuffer{ CreateRenderbuffer() };

	auto _ = gl_.Bind(renderbuffer, restore_bind);

	SetRenderbufferStorage(renderbuffer, size, internal_format);

	return renderbuffer;
}

void Renderbuffers::ResizeRenderbuffer(Renderbuffer renderbuffer, V2_int new_size) {
	PTGN_ASSERT(renderbuffer);

	const auto& cache = cache_.Get(renderbuffer);

	if (cache.size == new_size) {
		return;
	}

	auto _ = gl_.Bind(renderbuffer, true);

	SetRenderbufferStorage(renderbuffer, new_size, cache.internal_format);
}

RenderbufferCache& Renderbuffers::GetCache(Renderbuffer renderbuffer) {
	PTGN_ASSERT(cache_.Has(renderbuffer), "No renderbuffer with id ", renderbuffer, " in cache");
	return cache_.Get(renderbuffer);
}

const RenderbufferCache& Renderbuffers::GetCache(Renderbuffer renderbuffer) const {
	PTGN_ASSERT(cache_.Has(renderbuffer), "No renderbuffer with id ", renderbuffer, " in cache");
	return cache_.Get(renderbuffer);
}

void Renderbuffers::SetRenderbufferStorage(
	Renderbuffer renderbuffer, V2_int size, GLenum internal_format
) {
	PTGN_ASSERT(
		gl_.IsBound(renderbuffer), "Renderbuffer must be bound prior to setting its storage"
	);

	GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

	auto& cache			  = cache_.Get(renderbuffer);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

Renderbuffer Renderbuffers::CreateRenderbuffer() {
	Renderbuffer id{ 0 };
	GLCall(GenRenderbuffers(1, &id.value));
	PTGN_ASSERT(id, "Failed to create renderbuffer");
	cache_.Add(id, RenderbufferCache{});
	return id;
}

void Renderbuffers::DestroyRenderbuffer(Renderbuffer id) {
	if (!id) {
		return;
	}
	GLCall(DeleteRenderbuffers(1, &id.value));
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl