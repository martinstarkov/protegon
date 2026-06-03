#include "renderer/resources/resource.h"

#include <utility>

#include "renderer/renderer.h"
#include "renderer/resources/id.h"

namespace ptgn::impl {

template <ResourceType T>
Resource<T>::Resource(Renderer* renderer, T resource) noexcept :
	renderer{ renderer }, resource{ resource } {}

template <ResourceType T>
Resource<T>::Resource(Resource&& other) noexcept :
	renderer{ std::exchange(other.renderer, nullptr) },
	resource{ std::exchange(other.resource, T{}) } {}

template <ResourceType T>
Resource<T>& Resource<T>::operator=(Resource&& other) noexcept {
	if (this != &other) {
		Reset();
		renderer = std::exchange(other.renderer, nullptr);
		resource = std::exchange(other.resource, T{});
	}
	return *this;
}

template <ResourceType T>
Resource<T>::~Resource() noexcept {
	Reset();
}

template <ResourceType T>
Resource<T>::operator T() const {
	return resource;
}

template <ResourceType T>
Resource<T>::operator bool() const {
	return renderer && resource != T{};
}

template <ResourceType T>
void Resource<T>::Reset() noexcept {
	if (*this) {
		renderer->Destroy(resource);
		resource = T{};
		renderer = nullptr;
	}
}

template class Resource<VertexArrayId>;
template class Resource<VertexBufferId>;
template class Resource<ElementBufferId>;
template class Resource<UniformBufferId>;
template class Resource<RenderbufferId>;
template class Resource<FramebufferId>;
template class Resource<TextureId>;
template class Resource<ShaderId>;

} // namespace ptgn::impl