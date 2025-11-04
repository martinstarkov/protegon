#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "debug/core/log.h"
#include "math/vector2.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

class GLManager;

struct BufferResource {
	GLuint id{ 0 };
	GLenum usage{ GL_STATIC_DRAW };
	std::uint32_t count{ 0 };
};

struct RenderBufferResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum format{ GL_RGBA8 };
};

struct TextureResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum format{ GL_RGBA8 };
};

struct FrameBufferResource {
	GLuint id{ 0 };
	GLuint texture{ 0 };
	GLuint render_buffer{ 0 };
};

struct VertexArrayResource {
	GLuint id{ 0 };
	GLuint vertex_buffer{ 0 };
	GLuint element_buffer{ 0 };
};

struct ShaderResource {
	GLuint id{ 0 };

	std::string shader_name;

	// cache needs to be mutable even in const functions.
	mutable std::unordered_map<std::string, std::int32_t> location_cache;
};

enum class GLResource {
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
template <GLResource>
struct ResourceTraits;

// Generate specializations
#define DEFINE_GL_RESOURCE_TRAIT(EnumName, TypeName) \
	template <>                                      \
	struct ResourceTraits<GLResource::EnumName> {    \
		using Type = TypeName;                       \
	};

PTGN_GL_RESOURCE_TYPES(DEFINE_GL_RESOURCE_TRAIT)

#undef DEFINE_GL_RESOURCE_TRAIT
#undef PTGN_GL_RESOURCE_TYPES

template <GLResource T>
class Handle {
public:
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
	friend class GLManager;

	using ResourceType = typename ResourceTraits<T>::Type;

	explicit Handle(std::shared_ptr<ResourceType> resource) : resource_{ std::move(resource) } {}

	std::shared_ptr<ResourceType> resource_;
};

class GLManager {
public:
	template <bool kPersistent>
	Handle<GLResource::VertexBuffer> Create(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	) {
		return CreateBuffer<GLResource::VertexBuffer, kPersistent>(
			GL_ARRAY_BUFFER, data, element_count, element_size, usage,
			kPersistent ? persistent_vertex_buffers_ : vertex_buffers_
		);
	}

	template <bool kPersistent>
	Handle<GLResource::ElementBuffer> Create(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	) {
		return CreateBuffer<GLResource::ElementBuffer, kPersistent>(
			GL_ELEMENT_ARRAY_BUFFER, data, element_count, element_size, usage,
			kPersistent ? persistent_element_buffers_ : element_buffers_
		);
	}

	template <bool kPersistent>
	Handle<GLResource::UniformBuffer> Create(const void* data, std::uint32_t size, GLenum usage) {
		return CreateBuffer<GLResource::UniformBuffer, kPersistent>(
			GL_UNIFORM_BUFFER, data, size, 1, usage,
			kPersistent ? persistent_uniform_buffers_ : uniform_buffers_
		);
	}

	template <bool kPersistent = false>
	Handle<GLResource::Shader> Create(std::string_view shader_name) {
		auto resource		  = MakeGLResource<GLResource::Shader, ShaderResource>();
		resource->id		  = GLCallReturn(glCreateProgram());
		resource->shader_name = shader_name;

		// TODO: Create shader.

		PTGN_ASSERT(resource->id != 0, "Failed to create shader");

		auto& list = kPersistent ? persistent_shaders_ : shaders_;
		list.push_back(resource);

		return Handle<GLResource::Shader>(std::move(resource));
	}

	template <bool kPersistent = false>
	Handle<GLResource::Texture> Create(
		const V2_int& size, GLenum format = GL_RGBA8, GLenum pixel_format = GL_RGBA,
		GLenum pixel_data_type = GL_UNSIGNED_BYTE
	) {
		auto resource = MakeGLResource<GLResource::Texture, TextureResource>();
		GLCall(glGenTextures(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create texture");

		resource->size	 = size;
		resource->format = format;

		auto& list = kPersistent ? persistent_textures_ : textures_;
		list.push_back(resource);

		Handle<GLResource::Texture> handle{ std::move(resource) };

		Bind(handle);

		GLCall(glTexImage2D(
			GL_TEXTURE_2D, 0, format, size.x, size.y, 0, pixel_format, pixel_data_type, nullptr
		));

		// TODO: Make these modifiable.
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
		GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

		return handle;
	}

	template <bool kPersistent = false>
	Handle<GLResource::RenderBuffer> Create(const V2_int& size, GLenum format = GL_RGBA8) {
		auto resource = MakeGLResource<GLResource::RenderBuffer, RenderBufferResource>();
		GLCall(glGenRenderbuffers(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create render buffer");

		resource->size	 = size;
		resource->format = format;

		auto& list = kPersistent ? persistent_render_buffers_ : render_buffers_;
		list.push_back(resource);

		Handle<GLResource::RenderBuffer> handle{ std::move(resource) };

		Bind(handle);

		GLCall(glRenderbufferStorage(GL_RENDERBUFFER, format, size.x, size.y));

		return handle;
	}

	template <bool kPersistent = false>
	Handle<GLResource::FrameBuffer> Create(
		const Handle<GLResource::Texture>& texture,
		const Handle<GLResource::RenderBuffer>& render_buffer
	) {
		auto resource = MakeGLResource<GLResource::FrameBuffer, FrameBufferResource>();
		GLCall(glGenFramebuffers(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create framebuffer");

		resource->texture		= texture->id;
		resource->render_buffer = render_buffer->id;

		auto& list = kPersistent ? persistent_frame_buffers_ : frame_buffers_;
		list.push_back(resource);

		Handle<GLResource::FrameBuffer> handle{ std::move(resource) };

		Bind(handle);

		GLCall(glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0
		));
		GLCall(glFramebufferRenderbuffer(
			GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, render_buffer->id
		));

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		PTGN_ASSERT(status == GL_FRAMEBUFFER_COMPLETE, "Incomplete framebuffer");

		return handle;
	}

	template <bool kPersistent = false>
	Handle<GLResource::VertexArray> Create(
		const Handle<GLResource::VertexBuffer>& vertex_buffer,
		const Handle<GLResource::ElementBuffer>& element_buffer
	) {
		auto resource = MakeGLResource<GLResource::VertexArray, VertexArrayResource>();
		GLCall(glGenVertexArrays(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create vertex array");

		resource->vertex_buffer	 = vertex_buffer->id;
		resource->element_buffer = element_buffer->id;

		auto& list = kPersistent ? persistent_vertex_arrays_ : vertex_arrays_;
		list.push_back(resource);

		Handle<GLResource::VertexArray> handle{ std::move(resource) };

		Bind(handle);
		Bind(vertex_buffer);
		Bind(element_buffer);

		return handle;
	}

	template <GLResource T>
	void BindId(GLuint id) {
		// TODO: Update gl bound state.
		using enum ptgn::impl::gl::GLResource;
		if constexpr (T == VertexBuffer) {
			GLCall(glBindBuffer(GL_ARRAY_BUFFER, id));
		} else if constexpr (T == ElementBuffer) {
			GLCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
		} else if constexpr (T == UniformBuffer) {
			GLCall(glBindBuffer(GL_UNIFORM_BUFFER, id));
		} else if constexpr (T == Shader) {
			GLCall(glUseProgram(id));
		} else if constexpr (T == RenderBuffer) {
			GLCall(glBindRenderbuffer(GL_RENDERBUFFER, id));
		} else if constexpr (T == Texture) {
			GLCall(glBindTexture(GL_TEXTURE_2D, id));
		} else if constexpr (T == FrameBuffer) {
			GLCall(glBindFramebuffer(GL_FRAMEBUFFER, id));
		} else if constexpr (T == VertexArray) {
			GLCall(glBindVertexArray(id));
		} else {
			static_assert(false, "Unsupported GLResource type in BindId()");
		}
	}

	template <GLResource T>
	void Bind(const Handle<T>& handle) {
		BindId<T>(handle.Get().id);
	}

	void ClearUnused() {
		constexpr auto pred = [](const auto& r) {
			return r.use_count() == 1;
		};
		std::erase_if(shaders_, pred);
		std::erase_if(vertex_buffers_, pred);
		std::erase_if(element_buffers_, pred);
		std::erase_if(uniform_buffers_, pred);
		std::erase_if(render_buffers_, pred);
		std::erase_if(textures_, pred);
		std::erase_if(frame_buffers_, pred);
		std::erase_if(vertex_arrays_, pred);
	}

private:
	template <GLResource T, bool kPersistent>
	Handle<T> CreateBuffer(
		GLenum target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
		GLenum usage, std::vector<std::shared_ptr<BufferResource>>& resource_list
	) {
		static_assert(
			T == GLResource::ElementBuffer || T == GLResource::VertexBuffer ||
				T == GLResource::UniformBuffer,
			"Unsupported CreateBuffer resource type"
		);

		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		auto resource = MakeGLResource<T, BufferResource>();
		GLCall(glGenBuffers(1, &resource->id));
		PTGN_ASSERT(resource->id != 0, "Failed to create buffer resource");

		resource->usage = usage;
		resource->count = element_count;

		const std::uint32_t size = element_count * element_size;

		BindId<GLResource::VertexArray>(0);

		GLCall(glBindBuffer(target, resource->id));
		GLCall(glBufferData(target, size, data, usage));

		resource_list.push_back(resource);

		return Handle<T>(std::move(resource));
	}

	template <GLResource T>
	constexpr void DeleteId(GLuint id) {
		if (id == 0) {
			return; // nothing to delete
		}

		if constexpr (T == GLResource::VertexBuffer || T == GLResource::ElementBuffer ||
					  T == GLResource::UniformBuffer) {
			GLCall(glDeleteBuffers(1, &id));
		} else if constexpr (T == GLResource::Texture) {
			GLCall(glDeleteTextures(1, &id));
		} else if constexpr (T == GLResource::RenderBuffer) {
			GLCall(glDeleteRenderbuffers(1, &id));
		} else if constexpr (T == GLResource::FrameBuffer) {
			GLCall(glDeleteFramebuffers(1, &id));
		} else if constexpr (T == GLResource::VertexArray) {
			GLCall(glDeleteVertexArrays(1, &id));
		} else if constexpr (T == GLResource::Shader) {
			GLCall(glDeleteProgram(id));
		} else {
			static_assert(false, "Unsupported GLResource type in DeleteId()");
		}
	}

	template <GLResource T, typename ResourceType>
	std::shared_ptr<ResourceType> MakeGLResource() {
		return std::shared_ptr<ResourceType>(new ResourceType(), [](ResourceType* res) {
			if (res && res->id) {
				DeleteId<T>(res->id);
				res->id = 0;
			}
			delete res;
		});
	}

	std::vector<std::shared_ptr<ShaderResource>> shaders_;
	std::vector<std::shared_ptr<TextureResource>> textures_;
	std::vector<std::shared_ptr<BufferResource>> vertex_buffers_;
	std::vector<std::shared_ptr<BufferResource>> element_buffers_;
	std::vector<std::shared_ptr<BufferResource>> uniform_buffers_;
	std::vector<std::shared_ptr<RenderBufferResource>> render_buffers_;
	std::vector<std::shared_ptr<FrameBufferResource>> frame_buffers_;
	std::vector<std::shared_ptr<VertexArrayResource>> vertex_arrays_;

	std::vector<std::shared_ptr<ShaderResource>> persistent_shaders_;
	std::vector<std::shared_ptr<TextureResource>> persistent_textures_;
	std::vector<std::shared_ptr<BufferResource>> persistent_vertex_buffers_;
	std::vector<std::shared_ptr<BufferResource>> persistent_element_buffers_;
	std::vector<std::shared_ptr<BufferResource>> persistent_uniform_buffers_;
	std::vector<std::shared_ptr<RenderBufferResource>> persistent_render_buffers_;
	std::vector<std::shared_ptr<FrameBufferResource>> persistent_frame_buffers_;
	std::vector<std::shared_ptr<VertexArrayResource>> persistent_vertex_arrays_;
};

} // namespace ptgn::impl::gl