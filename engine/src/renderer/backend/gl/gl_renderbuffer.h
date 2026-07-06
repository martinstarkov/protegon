#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "core/util/id_map.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl::gl {

class GLContext;

struct RenderbufferCache {
	V2_int size;
	TextureFormat format{ TextureFormat::RGBA8 };
};

class Renderbuffers {
public:
	RenderbufferId Create(V2_int size, TextureFormat format, bool restore_bind = true);

	void Destroy(RenderbufferId id);
	void Resize(RenderbufferId renderbuffer, V2_int new_size);

	RenderbufferCache& GetCache(RenderbufferId renderbuffer);

	const RenderbufferCache& GetCache(RenderbufferId renderbuffer) const;

	std::optional<V2_int> GetSize(RenderbufferId renderbuffer) const;
	std::optional<TextureFormat> GetFormat(RenderbufferId renderbuffer) const;

private:
	friend class GLContext;

	explicit Renderbuffers(GLContext& gl);
	~Renderbuffers() noexcept						   = default;
	Renderbuffers(const Renderbuffers&)				   = delete;
	Renderbuffers(Renderbuffers&&) noexcept			   = delete;
	Renderbuffers& operator=(const Renderbuffers&)	   = delete;
	Renderbuffers& operator=(Renderbuffers&&) noexcept = delete;

	void SetStorage(RenderbufferId renderbuffer, V2_int size, TextureFormat format);

	[[nodiscard]] RenderbufferId CreateImpl();

	GLContext& gl_;

	IdMap<RenderbufferCache> cache_;
};

} // namespace ptgn::impl::gl