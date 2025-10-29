#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "renderer/gl/gl.h"

namespace ptgn::impl::gl {

//}
// glGenTextures(1, &id)
// else if constexpr (std::is_same_v<T, RenderBuffer>) {
//	GLCall(glGenRenderbuffers(1, &id));
//}
// else if constexpr (std::is_same_v<T, Shader>) {
//	id = GLCallReturn(glCreateProgram());
//}
// else if constexpr (std::is_same_v<T, Buffer>) {
//	GLCall(glGenBuffers(1, &id));
//}
// else if constexpr (std::is_same_v<T, FrameBuffer>) {
//	GLCall(glGenFramebuffers(1, &id));
//}
// else if constexpr (std::is_same_v<T, VertexArray>) {
//	GLCall(glGenVertexArrays(1, &id));
//}
//
// PTGN_ASSERT(id, "Failed to generate OpenGL resource");
//
// resource_id_ = std::make_shared<GLuint>(id);
//}
//
//~Resource() {
//	auto id{ Get() };
//
//	using namespace GLResourceType;
//
//	if constexpr (std::is_same_v<T, Texture>) {
//		GLCall(glDeleteTextures(1, &id));
//	} else if constexpr (std::is_same_v<T, RenderBuffer>) {
//		GLCall(glDeleteRenderbuffers(1, &id));
//	} else if constexpr (std::is_same_v<T, Shader>) {
//		GLCall(glDeleteProgram(id));
//	} else if constexpr (std::is_same_v<T, Buffer>) {
//		GLCall(glDeleteBuffers(1, &id));
//	} else if constexpr (std::is_same_v<T, FrameBuffer>) {
//		GLCall(glDeleteFramebuffers(1, &id));
//	} else if constexpr (std::is_same_v<T, VertexArray>) {
//		GLCall(glDeleteVertexArrays(1, &id));
//	}
//}
//
// void Bind() {
//	auto id{ Get() };
//
//
//	if constexpr (std::is_same_v<T, GLResourceType::Texture>) {
//		GLCall(glBindTexture(GL_TEXTURE_2D, id));
//	} else if constexpr (std::is_same_v<T, GLResourceType::Buffer>) {
//		GLCall(glBindBuffer(GL_ARRAY_BUFFER, id));
//	} else if constexpr (std::is_same_v<T, GLResourceType::FrameBuffer>) {
//		GLCall(glBindFramebuffer(GL_FRAMEBUFFER, id));
//	} else if constexpr (std::is_same_v<T, GLResourceType::VertexArray>) {
//		GLCall(glBindVertexArray(id));
//	} else if constexpr (std::is_same_v<T, GLResourceType::RenderBuffer>) {
//		GLCall(glBindRenderbuffer(GL_RENDERBUFFER, id));
//	}
//}
//}
//;

enum class GLResource {
	Texture,
};

class GLManager;

template <GLResource T>
class Handle {
public:
	bool operator==(const Handle&) const = default;

private:
	friend class GLManager;

	Handle(std::shared_ptr<int> resource) : resource_{ std::move(resource) } {}

	std::shared_ptr<int> resource_;
};

struct TextureDeleter {
	void operator()(int* texture) {
		std::cout << "destroying texture: " << *texture << std::endl;
		delete texture;
	}
};

class GLManager {
public:
	template <GLResource T, bool kPersistent = false, typename... TArgs>
	Handle<T> Create(TArgs&&... args) {
		std::shared_ptr<int> resource;

		if constexpr (T == GLResource::Texture) {
			// TODO: Replace with actual gl bind.
			resource =
				std::shared_ptr<int>(new int(std::forward<TArgs>(args)...), TextureDeleter{});
			std::cout << "created texture: " << *resource << std::endl;
		}

		// TODO: Assert that resource is not nullptr.

		GetResources<kPersistent>().emplace_back(resource);
		return resource;
	}

	template <GLResource T>
	void Bind(const Handle<T>& handle) {
		if constexpr (T == GLResource::Texture) {
			std::cout << "binding texture: " << *handle.resource_ << std::endl;
		}
	}

	void ClearUnused() {
		std::erase_if(resources_, [](const auto& resource) {
			if (resource.use_count() == 1) {
				std::cout << "clearing non-persistent resource with id: " << *resource << std::endl;
				return true;
			} else {
				return false;
			}
		});
	}

private:
	template <bool kPersistent>
	std::vector<std::shared_ptr<int>>& GetResources() {
		return kPersistent ? persistent_resources_ : resources_;
	}

	std::vector<std::shared_ptr<int>> resources_;

	std::vector<std::shared_ptr<int>> persistent_resources_;
};

} // namespace ptgn::impl::gl