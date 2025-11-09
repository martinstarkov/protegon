#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "math/vector2.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

class GLContext;

struct BufferResource {
	GLuint id{ 0 };
	GLenum usage{ GL_STATIC_DRAW };
	std::uint32_t count{ 0 };
};

struct RenderBufferResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct TextureResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
};

struct FrameBufferResource {
	GLuint id{ 0 };
	Handle<Texture> texture;
	Handle<RenderBuffer> render_buffer;
};

struct VertexArrayResource {
	GLuint id{ 0 };
	Handle<VertexBuffer> vertex_buffer;
	Handle<ElementBuffer> element_buffer;
};

struct ShaderResource {
	GLuint id{ 0 };

	std::string shader_name;

	// cache needs to be mutable even in const functions.
	mutable std::unordered_map<std::string, std::int32_t> location_cache;
};

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

#define PTGN_GL_RESOURCE_TYPES(X)         \
	X(Shader, ShaderResource)             \
	X(VertexBuffer, BufferResource)       \
	X(ElementBuffer, BufferResource)      \
	X(UniformBuffer, BufferResource)      \
	X(RenderBuffer, RenderBufferResource) \
	X(Texture, TextureResource)           \
	X(FrameBuffer, FrameBufferResource)   \
	X(VertexArray, VertexArrayResource)

// Primary template
template <Resource>
struct ResourceTraits;

// Generate specializations
#define DEFINE_GL_RESOURCE_TRAIT(EnumName, TypeName) \
	template <>                                      \
	struct ResourceTraits<EnumName> {                \
		using Type = TypeName;                       \
	};

PTGN_GL_RESOURCE_TYPES(DEFINE_GL_RESOURCE_TRAIT)

#undef DEFINE_GL_RESOURCE_TRAIT
#undef PTGN_GL_RESOURCE_TYPES

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