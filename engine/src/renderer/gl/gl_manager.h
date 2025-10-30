#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "debug/core/log.h"
#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

#define PTGN_DEFINE_GL_RESOURCE_CREATION(ResourceEnum, GenFunc, DeleteFunc, InitLambda) \
	else if constexpr (T == GLResource::ResourceEnum) {                                 \
		resource = std::shared_ptr<GLuint>(new GLuint(0), [](GLuint* id) {              \
			GLCall(DeleteFunc(1, id));                                                  \
			delete id;                                                                  \
		});                                                                             \
		GLCall(GenFunc(1, resource.get()));                                             \
		PTGN_ASSERT(resource && *resource, "Failed to create ", #ResourceEnum);         \
		InitLambda(*resource, std::forward<TArgs>(args)...);                            \
	}

enum class GLResource {
	Shader,
	Buffer,
	RenderBuffer,
	Texture,
	FrameBuffer,
	VertexArray
};

class GLManager;

template <GLResource T>
class Handle {
public:
	bool operator==(const Handle&) const = default;

	bool operator() const {
		return resource_;
	}

private:
	friend class GLManager;

	Handle(std::shared_ptr<GLuint> resource) : resource_{ std::move(resource) } {}

	std::shared_ptr<GLuint> resource_;
};

class GLManager {
public:
	template <GLResource T, bool kPersistent = false, typename... TArgs>
	Handle<T> Create(TArgs&&... args) {
		std::shared_ptr<GLuint> resource;

		if constexpr (T == GLResource::Shader) {
			resource = std::shared_ptr<GLuint>(
				new GLuint(GLCallReturn(glCreateProgram())),
				[](GLuint* id) {
					GLCall(glDeleteProgram(*id));
					delete id;
				}
			);
			PTGN_ASSERT(resource && *resource, "Failed to create Shader");
			const GLuint id{ *resource };
			PTGN_LOG("Initializing shader");
			// TODO: Init shader with TArgs...
		}
		PTGN_DEFINE_GL_RESOURCE_CREATION(Buffer, glGenBuffers, glDeleteBuffers, [](GLuint id) {
			PTGN_LOG("Initializing buffer");
		})
		PTGN_DEFINE_GL_RESOURCE_CREATION(
			RenderBuffer, glGenRenderbuffers, glDeleteRenderbuffers,
			[](GLuint id) { PTGN_LOG("Initializing render buffer"); }
		)
		PTGN_DEFINE_GL_RESOURCE_CREATION(Texture, glGenTextures, glDeleteTextures, [](GLuint id) {
			PTGN_LOG("Initializing texture");
		})
		PTGN_DEFINE_GL_RESOURCE_CREATION(
			FrameBuffer, glGenFramebuffers, glDeleteFramebuffers,
			[](GLuint id) { PTGN_LOG("Initializing frame buffer"); }
		)
		PTGN_DEFINE_GL_RESOURCE_CREATION(
			VertexArray, glGenVertexArrays, glDeleteVertexArrays,
			[](GLuint id) { PTGN_LOG("Initializing vertex array"); }
		)

		PTGN_ASSERT(id && *id, "Failed to create resource");

		GetResources<kPersistent>().emplace_back(resource);
		return resource;
	}

	template <GLResource T>
	void Bind(const Handle<T>& handle) {
		PTGN_ASSERT(handle);

		if constexpr (T == GLResourceType::Shader) {
			GLCall(glUseProgram(*handle.resource_));
		} else if constexpr (T == GLResourceType::Buffer) {
			GLCall(glBindBuffer(GL_ARRAY_BUFFER, *handle.resource_));
		} else if constexpr (T == GLResourceType::RenderBuffer) {
			GLCall(glBindRenderbuffer(GL_RENDERBUFFER, *handle.resource_));
		} else if constexpr (T == GLResourceType::Texture) {
			GLCall(glBindTexture(GL_TEXTURE_2D, *handle.resource_));
		} else if constexpr (T == GLResourceType::FrameBuffer) {
			GLCall(glBindFramebuffer(GL_FRAMEBUFFER, *handle.resource_));
		} else if constexpr (T == GLResourceType::VertexArray) {
			GLCall(glBindVertexArray(*handle.resource_));
		}
	}

	void ClearUnused() {
		std::erase_if(resources_, [](const auto& r) { return r.use_count() == 1; });
	}

private:
	template <bool kPersistent>
	auto& GetResources() {
		return kPersistent ? persistent_resources_ : resources_;
	}

	std::vector<std::shared_ptr<GLuint>> resources_;

	std::vector<std::shared_ptr<GLuint>> persistent_resources_;
};

} // namespace ptgn::impl::gl