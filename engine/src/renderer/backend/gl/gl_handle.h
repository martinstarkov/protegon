#pragma once

#include <cstdint>
#include <memory>
#include <utility>

namespace ptgn {

template <typename U>
struct IdMap;

namespace impl::gl {

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

private:
	friend class GLContext;

	template <typename U>
	friend struct IdMap;

	constexpr operator Id() const {
		return id_ ? id_ : 0;
	}

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

	// TODO: Move this to private.
	constexpr operator WeakHandle<T>() const {
		return WeakHandle<T>{ id_ ? *id_ : 0 };
	}

private:
	friend class GLContext;

	constexpr StrongHandle(std::shared_ptr<Id> id) : id_{ std::move(id) } {}

	std::shared_ptr<Id> id_;
};

using Shader		  = StrongHandle<Resource::Shader>;
using ShaderId		  = WeakHandle<Resource::Shader>;
using VertexBuffer	  = StrongHandle<Resource::VertexBuffer>;
using VertexBufferId  = WeakHandle<Resource::VertexBuffer>;
using ElementBuffer	  = StrongHandle<Resource::ElementBuffer>;
using ElementBufferId = WeakHandle<Resource::ElementBuffer>;
using UniformBuffer	  = StrongHandle<Resource::UniformBuffer>;
using UniformBufferId = WeakHandle<Resource::UniformBuffer>;
using Texture		  = StrongHandle<Resource::Texture>;
using TextureId		  = WeakHandle<Resource::Texture>;
using Renderbuffer	  = StrongHandle<Resource::Renderbuffer>;
using RenderbufferId  = WeakHandle<Resource::Renderbuffer>;
using Framebuffer	  = StrongHandle<Resource::Framebuffer>;
using FramebufferId	  = WeakHandle<Resource::Framebuffer>;
using VertexArray	  = StrongHandle<Resource::VertexArray>;
using VertexArrayId	  = WeakHandle<Resource::VertexArray>;

} // namespace impl::gl

} // namespace ptgn