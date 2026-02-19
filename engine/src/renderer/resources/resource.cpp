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
bool Resource<T>::IsValid() const noexcept {
	return renderer_ != nullptr && resource_ != T{};
}

template <typename T>
void Resource<T>::Reset() noexcept {
	if (IsValid()) {
		renderer_->gl->Destroy(resource_);
		resource_ = T{};
		renderer_ = nullptr;
	}
}

template class Resource<VertexArray>;
template class Resource<VertexBuffer>;
template class Resource<ElementBuffer>;
template class Resource<UniformBuffer>;
template class Resource<Renderbuffer>;
template class Resource<Framebuffer>;
template class Resource<impl::RenderTarget>;
template class Resource<Texture>;
template class Resource<Shader>;

} // namespace ptgn::impl