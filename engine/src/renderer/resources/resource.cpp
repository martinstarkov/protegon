#include "renderer/resources/resource.h"

#include <utility>

#include "render_target.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn::impl {

template <typename T>
Resource<T>::Resource(gl::Renderer* renderer, T resource) noexcept :
	renderer_{ renderer }, resource_{ resource } {}

template <typename T>
Resource<T>::Resource(Resource&& other) noexcept :
	renderer_{ std::exchange(other.renderer_, nullptr) },
	resource_{ std::exchange(other.resource_, T{}) } {}

template <typename T>
Resource<T>& Resource<T>::operator=(Resource&& other) noexcept {
	if (this != &other) {
		Reset();
		renderer_ = std::exchange(other.renderer_, nullptr);
		resource_ = std::exchange(other.resource_, T{});
	}
	return *this;
}

template <typename T>
Resource<T>::~Resource() {
	Reset();
}

template <typename T>
Resource<T>::operator T() const noexcept {
	return resource_;
}

template <typename T>
Resource<T>::operator bool() const noexcept {
	return renderer_ != nullptr && resource_ != T{};
}

template <typename T>
void Resource<T>::Reset() noexcept {
	if (*this) {
		renderer_->gl->Destroy(resource_);
		resource_ = T{};
		renderer_ = nullptr;
	}
}

template class Resource<VertexArrayId>;
template class Resource<VertexBufferId>;
template class Resource<ElementBufferId>;
template class Resource<UniformBufferId>;
template class Resource<RenderbufferId>;
template class Resource<FramebufferId>;
template class Resource<RenderTargetData>;
template class Resource<TextureId>;
template class Resource<ShaderId>;

} // namespace ptgn::impl