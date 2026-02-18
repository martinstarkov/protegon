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

template class BindGuard<VertexBuffer>;
template class BindGuard<ElementBuffer>;
template class BindGuard<UniformBuffer>;
template class BindGuard<Texture>;
template class BindGuard<Shader>;
template class BindGuard<Renderbuffer>;
template class BindGuard<Framebuffer>;
template class BindGuard<VertexArray>;

} // namespace ptgn::impl::gl