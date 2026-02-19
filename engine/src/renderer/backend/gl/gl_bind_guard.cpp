#include "renderer/backend/gl/gl_bind_guard.h"

#include "gl_vertex_array.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

template <typename T>
BindGuard<T>::~BindGuard() noexcept {
	if (restore_bind_) {
		auto _ = gl_.Bind(id_, false);
	}
}

template class BindGuard<VertexBufferId>;
template class BindGuard<ElementBufferId>;
template class BindGuard<UniformBufferId>;
template class BindGuard<TextureId>;
template class BindGuard<ShaderId>;
template class BindGuard<RenderbufferId>;
template class BindGuard<FramebufferId>;
template class BindGuard<VertexArrayId>;

} // namespace ptgn::impl::gl