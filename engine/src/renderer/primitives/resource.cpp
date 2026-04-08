#include "renderer/primitives/resource.h"

#include <utility>

#include "renderer/primitives/id.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

template <ResourceType T>
Resource<T>::Resource(Renderer* renderer, T resource) noexcept :
	renderer_{ renderer }, resource_{ resource } {}

template <ResourceType T>
Resource<T>::Resource(Resource&& other) noexcept :
	renderer_{ std::exchange(other.renderer_, nullptr) },
	resource_{ std::exchange(other.resource_, T{}) } {}

template <ResourceType T>
Resource<T>& Resource<T>::operator=(Resource&& other) noexcept {
	if (this != &other) {
		Reset();
		renderer_ = std::exchange(other.renderer_, nullptr);
		resource_ = std::exchange(other.resource_, T{});
	}
	return *this;
}

template <ResourceType T>
Resource<T>::~Resource() noexcept {
	Reset();
}

template <ResourceType T>
Resource<T>::operator T() const {
	return resource_;
}

template <ResourceType T>
Resource<T>::operator bool() const {
	return renderer_ != nullptr && resource_ != T{};
}

template <ResourceType T>
void Resource<T>::Reset() noexcept {
	if (*this) {
		renderer_->Destroy(resource_);
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
template class Resource<RenderTargetId>;
template class Resource<TextureId>;
template class Resource<ShaderId>;

} // namespace impl

} // namespace ptgn