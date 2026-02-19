#pragma once

#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/resources/renderbuffer.h"

namespace ptgn::impl::gl {

class GLContext;

struct RenderbufferCache {
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

class Renderbuffers {
public:
	RenderbufferId CreateRenderbuffer(
		V2_int size, GLenum internal_format, bool restore_bind = true
	);

	void DestroyRenderbuffer(RenderbufferId id);
	void ResizeRenderbuffer(RenderbufferId renderbuffer, V2_int new_size);

	RenderbufferCache& GetCache(RenderbufferId renderbuffer);

	const RenderbufferCache& GetCache(RenderbufferId renderbuffer) const;

private:
	friend class GLContext;

	explicit Renderbuffers(GLContext& gl);
	~Renderbuffers() noexcept						   = default;
	Renderbuffers(const Renderbuffers&)				   = delete;
	Renderbuffers(Renderbuffers&&) noexcept			   = delete;
	Renderbuffers& operator=(const Renderbuffers&)	   = delete;
	Renderbuffers& operator=(Renderbuffers&&) noexcept = delete;

	void SetRenderbufferStorage(RenderbufferId renderbuffer, V2_int size, GLenum internal_format);

	[[nodiscard]] RenderbufferId CreateRenderbuffer();

	GLContext& gl_;

	IdMap<RenderbufferCache> cache_;
};

} // namespace ptgn::impl::gl