#include "renderer/backend/gl/gl_renderbuffer.h"

#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/texture_format.h"

namespace ptgn::impl::gl {

Renderbuffers::Renderbuffers(GLContext& gl) : gl_{ gl } {}

RenderbufferId Renderbuffers::CreateRenderbuffer(
	V2_int size, TextureFormat format, bool restore_bind
) {
	auto renderbuffer{ CreateRenderbuffer() };

	auto _ = gl_.Bind(renderbuffer, restore_bind);

	SetRenderbufferStorage(renderbuffer, size, format);

	return renderbuffer;
}

void Renderbuffers::ResizeRenderbuffer(RenderbufferId renderbuffer, V2_int new_size) {
	PTGN_ASSERT(renderbuffer);

	const auto& cache = cache_.Get(renderbuffer);

	if (cache.size == new_size) {
		return;
	}

	auto _ = gl_.Bind(renderbuffer, true);

	SetRenderbufferStorage(renderbuffer, new_size, cache.format);
}

RenderbufferCache& Renderbuffers::GetCache(RenderbufferId renderbuffer) {
	PTGN_ASSERT(cache_.Has(renderbuffer), "No renderbuffer with id ", renderbuffer, " in cache");
	return cache_.Get(renderbuffer);
}

const RenderbufferCache& Renderbuffers::GetCache(RenderbufferId renderbuffer) const {
	PTGN_ASSERT(cache_.Has(renderbuffer), "No renderbuffer with id ", renderbuffer, " in cache");
	return cache_.Get(renderbuffer);
}

void Renderbuffers::SetRenderbufferStorage(
	RenderbufferId renderbuffer, V2_int size, TextureFormat format
) {
	PTGN_ASSERT(
		gl_.IsBound(renderbuffer), "RenderbufferId must be bound prior to setting its storage"
	);

	constexpr AttachmentObject target{ AttachmentObject::Renderbuffer };

	GLCall(glRenderbufferStorage(
		std::to_underlying(target), std::to_underlying(format), size.x, size.y
	));

	auto& cache	 = cache_.Get(renderbuffer);
	cache.size	 = size;
	cache.format = format;
}

RenderbufferId Renderbuffers::CreateRenderbuffer() {
	RenderbufferId id{ 0 };
	GLCall(glGenRenderbuffers(1, &id.value));
	PTGN_ASSERT(id, "Failed to create renderbuffer");
	cache_.Add(id, RenderbufferCache{});
	return id;
}

void Renderbuffers::DestroyRenderbuffer(RenderbufferId id) {
	if (!id) {
		return;
	}
	GLCall(glDeleteRenderbuffers(1, &id.value));
	cache_.Remove(id);
}

} // namespace ptgn::impl::gl