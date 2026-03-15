#include "renderer/primitives/resource.h"

#include <utility>

#include "renderer/primitives/id.h"
#include "renderer/primitives/render_target.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

template <typename T>
Resource<T>::Resource(Renderer* renderer, T resource) noexcept :
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
template class Resource<RenderTargetData>;
template class Resource<TextureId>;
template class Resource<ShaderId>;

} // namespace impl

} // namespace ptgn