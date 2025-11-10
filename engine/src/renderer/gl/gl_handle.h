#pragma once

#include <memory>

#include "core/assert.h"

namespace ptgn::impl::gl {

enum Resource {
	Shader,
	VertexBuffer,
	ElementBuffer,
	UniformBuffer,
	RenderBuffer,
	Texture,
	FrameBuffer,
	VertexArray
};

struct ShaderResource;
struct BufferResource;
struct RenderBufferResource;
struct TextureResource;
struct FrameBufferResource;
struct VertexArrayResource;

template <Resource>
struct ResourceTraits;

#define PTGN_DEFINE_GL_RESOURCE_TRAIT(EnumName, TypeName) \
	template <>                                           \
	struct ResourceTraits<EnumName> {                     \
		using Type = TypeName;                            \
	};

PTGN_DEFINE_GL_RESOURCE_TRAIT(Shader, ShaderResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(VertexBuffer, BufferResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(ElementBuffer, BufferResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(UniformBuffer, BufferResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(RenderBuffer, RenderBufferResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(Texture, TextureResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(FrameBuffer, FrameBufferResource)
PTGN_DEFINE_GL_RESOURCE_TRAIT(VertexArray, VertexArrayResource)

#undef PTGN_DEFINE_GL_RESOURCE_TRAIT

template <Resource T>
class Handle {
public:
	Handle() = default;

	bool operator==(const Handle&) const = default;

	explicit operator bool() const {
		return static_cast<bool>(resource_);
	}

	auto& Get() {
		PTGN_ASSERT(resource_);
		return *resource_;
	}

	const auto& Get() const {
		PTGN_ASSERT(resource_);
		return *resource_;
	}

private:
	friend class GLContext;

	using ResourceType = typename ResourceTraits<T>::Type;

	explicit Handle(std::shared_ptr<ResourceType> resource) : resource_{ std::move(resource) } {}

	std::shared_ptr<ResourceType> resource_;
};

} // namespace ptgn::impl::gl