#pragma once

#include <cstdint>
#include <memory>
#include <utility>

namespace ptgn {

template <typename U>
struct IdMap;

namespace impl {

namespace gl {

class GLContext;

};

using Id = std::uint32_t;

enum class Resource {
	Shader,
	VertexBuffer,
	ElementBuffer,
	UniformBuffer,
	Texture,
	Renderbuffer,
	Framebuffer,
	VertexArray
};

template <Resource T>
class WeakHandle {
public:
	constexpr WeakHandle() = default;

	constexpr explicit WeakHandle(Id id) : id_{ id } {}

	bool operator==(const WeakHandle&) const = default;

	[[nodiscard]] constexpr explicit operator bool() const {
		return static_cast<bool>(id_);
	}

	constexpr operator Id() const {
		return id_ ? id_ : 0;
	}

private:
	friend class impl::gl::GLContext;

	template <typename U>
	friend struct IdMap;

	Id id_{ 0 };
};

template <Resource T>
class StrongHandle {
public:
	constexpr StrongHandle() = default;

	bool operator==(const StrongHandle&) const = default;

	[[nodiscard]] constexpr explicit operator bool() const {
		return static_cast<bool>(id_);
	}

	constexpr operator WeakHandle<T>() const {
		return WeakHandle<T>{ id_ ? *id_ : 0 };
	}

private:
	friend class impl::gl::GLContext;

	constexpr StrongHandle(std::shared_ptr<Id> id) : id_{ std::move(id) } {}

	std::shared_ptr<Id> id_;
};

} // namespace impl

using Shader		= impl::StrongHandle<impl::Resource::Shader>;
using VertexBuffer	= impl::StrongHandle<impl::Resource::VertexBuffer>;
using ElementBuffer = impl::StrongHandle<impl::Resource::ElementBuffer>;
using UniformBuffer = impl::StrongHandle<impl::Resource::UniformBuffer>;
using Texture		= impl::StrongHandle<impl::Resource::Texture>;
using Renderbuffer	= impl::StrongHandle<impl::Resource::Renderbuffer>;
using Framebuffer	= impl::StrongHandle<impl::Resource::Framebuffer>;
using VertexArray	= impl::StrongHandle<impl::Resource::VertexArray>;

using ShaderId		  = impl::WeakHandle<impl::Resource::Shader>;
using VertexBufferId  = impl::WeakHandle<impl::Resource::VertexBuffer>;
using ElementBufferId = impl::WeakHandle<impl::Resource::ElementBuffer>;
using UniformBufferId = impl::WeakHandle<impl::Resource::UniformBuffer>;
using TextureId		  = impl::WeakHandle<impl::Resource::Texture>;
using RenderbufferId  = impl::WeakHandle<impl::Resource::Renderbuffer>;
using FramebufferId	  = impl::WeakHandle<impl::Resource::Framebuffer>;
using VertexArrayId	  = impl::WeakHandle<impl::Resource::VertexArray>;

} // namespace ptgn