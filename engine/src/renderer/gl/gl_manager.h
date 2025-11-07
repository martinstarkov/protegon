#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/util/concepts.h"
#include "debug/core/log.h"
#include "math/vector2.h"
#include "renderer/gl/buffer_layout.h"
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
	GLenum internal_format{ GL_RGBA8 };
};

struct TextureResource {
	GLuint id{ 0 };
	V2_int size;
	GLenum internal_format{ GL_RGBA8 };
	GLenum pixel_format{ GL_RGBA };
};

struct FrameBufferResource {
	GLuint id{ 0 };
	Handle<GLResource::Texture> texture;
	Handle<GLResource::RenderBuffer> render_buffer;
};

struct VertexArrayResource {
	GLuint id{ 0 };
	Handle<GLResource::VertexBuffer> vertex_buffer;
	Handle<GLResource::ElementBuffer> element_buffer;
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
		resource->id		  = GLCallReturn(CreateProgram());
		resource->shader_name = shader_name;

		// TODO: Create shader.

		PTGN_ASSERT(resource->id != 0, "Failed to create shader");

		auto& list = kPersistent ? persistent_shaders_ : shaders_;
		list.push_back(resource);

		return Handle<GLResource::Shader>(std::move(resource));
	}

	template <bool kPersistent = false>
	Handle<GLResource::Texture> Create(
		const V2_int& size, GLenum internal_format = GL_RGBA8, GLenum pixel_format = GL_RGBA,
		GLenum pixel_data_type = GL_UNSIGNED_BYTE
	) {
		auto resource = MakeGLResource<GLResource::Texture, TextureResource>();
		GLCall(glGenTextures(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create texture");

		auto& list = kPersistent ? persistent_textures_ : textures_;
		list.push_back(resource);

		Handle<GLResource::Texture> handle{ std::move(resource) };

		Bind(handle);

#ifdef __EMSCRIPTEN__
		PTGN_ASSERT(
			format != GL_BGRA && format != GL_BGR,
			"OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D"
		);
#endif

		resource->size			  = size;
		resource->internal_format = internal_format;
		resource->pixel_format	  = pixel_format;

		GLCall(glTexImage2D(
			GL_TEXTURE_2D, 0, internal_format, size.x, size.y, 0, pixel_format, pixel_data_type,
			nullptr
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
		GLCall(GenRenderbuffers(1, &resource->id));
		auto restore_render_buffer_id{ GetBoundId<GLResource::RenderBuffer>() };
		PTGN_ASSERT(resource->id, "Failed to create render buffer");

		auto& list = kPersistent ? persistent_render_buffers_ : render_buffers_;
		list.push_back(resource);

		Handle<GLResource::RenderBuffer> handle{ std::move(resource) };

		Bind(handle);

		SetRenderBufferStorage(handle, size, format);

		BindId<GLResource::RenderBuffer>(restore_render_buffer_id);

		return handle;
	}

	template <bool kPersistent = false>
	Handle<GLResource::FrameBuffer> Create(
		const Handle<GLResource::Texture>& texture, bool bind_frame_buffer = false
	) {
		PTGN_ASSERT(
			texture.Get().size.BothAboveZero(),
			"Cannot attach texture with no size to a frame buffer"
		);

		auto resource = MakeGLResource<GLResource::FrameBuffer, FrameBufferResource>();
		GLCall(GenFramebuffers(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create framebuffer");

		resource->texture = texture;
		resource->render_buffer =
			Create<GLResource::RenderBuffer, false>(texture.Get().size, GL_DEPTH24_STENCIL8);

		auto& list = kPersistent ? persistent_frame_buffers_ : frame_buffers_;
		list.push_back(resource);

		Handle<GLResource::FrameBuffer> handle{ std::move(resource) };

		std::optional<GLuint> restore_frame_buffer_id;
		if (!bind_frame_buffer) {
			restore_frame_buffer_id = GetBoundId<GLResource::FrameBuffer>();
		}

		Bind(handle);

		GLCall(FramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0
		));

		GLCall(FramebufferRenderbuffer(
			GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer->id
		));

		PTGN_ASSERT(FrameBufferIsComplete(handle));

		if (restore_frame_buffer_id.has_value()) {
			BindId<GLResource::FrameBuffer>(*restore_frame_buffer_id);
		}

		return handle;
	}

	template <bool kPersistent = false, typename... Ts>
	Handle<GLResource::VertexArray> Create(
		const Handle<GLResource::VertexBuffer>& vertex_buffer,
		const BufferLayout<Ts...>& vertex_buffer_layout,
		const Handle<GLResource::ElementBuffer>& element_buffer
	) {
		auto resource = MakeGLResource<GLResource::VertexArray, VertexArrayResource>();
		GLCall(GenVertexArrays(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create vertex array");

		resource->vertex_buffer	 = vertex_buffer;
		resource->element_buffer = element_buffer;

		auto& list = kPersistent ? persistent_vertex_arrays_ : vertex_arrays_;
		list.push_back(resource);

		Handle<GLResource::VertexArray> handle{ std::move(resource) };

		Bind(handle);
		SetVertexBuffer(vertex_buffer);
		SetElementBuffer(element_buffer);
		SetVertexArrayLayout(vertex_buffer_layout);

		return handle;
	}

	template <GLResource T>
	void BindId(GLuint id) {
		// TODO: Update gl bound state.
		using enum ptgn::impl::gl::GLResource;
		if constexpr (T == VertexBuffer) {
			GLCall(BindBuffer(GL_ARRAY_BUFFER, id));
		} else if constexpr (T == ElementBuffer) {
			GLCall(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
		} else if constexpr (T == UniformBuffer) {
			GLCall(BindBuffer(GL_UNIFORM_BUFFER, id));
		} else if constexpr (T == Shader) {
			GLCall(UseProgram(id));
		} else if constexpr (T == RenderBuffer) {
			GLCall(BindRenderbuffer(GL_RENDERBUFFER, id));
		} else if constexpr (T == Texture) {
			GLCall(glBindTexture(GL_TEXTURE_2D, id));
		} else if constexpr (T == FrameBuffer) {
			GLCall(BindFramebuffer(GL_FRAMEBUFFER, id));
		} else if constexpr (T == VertexArray) {
#ifdef PTGN_PLATFORM_MACOS
			// MacOS complains about binding 0 id vertex array.
			if (id == 0) {
				return;
			}
#endif
			GLCall(BindVertexArray(id));
		} else {
			static_assert(false, "Unsupported GLResource type in BindId()");
		}
	}

	template <GLResource T>
	void Bind(const Handle<T>& handle) {
		BindId<T>(handle.Get().id);
	}

	template <GLResource T>
	[[nodiscard]] GLuint GetBoundId() const {
		using enum ptgn::impl::gl::GLResource;
		if constexpr (T == VertexBuffer) {
			return GetInteger<GLuint>(GL_ARRAY_BUFFER_BINDING);
		} else if constexpr (T == ElementBuffer) {
			return GetInteger<GLuint>(GL_ELEMENT_ARRAY_BUFFER_BINDING);
		} else if constexpr (T == UniformBuffer) {
			return GetInteger<GLuint>(GL_UNIFORM_BUFFER_BINDING);
		} else if constexpr (T == Texture) {
			return GetInteger<GLuint>(GL_TEXTURE_BINDING_2D);
		} else if constexpr (T == RenderBuffer) {
			return GetInteger<GLuint>(GL_RENDERBUFFER_BINDING);
		} else if constexpr (T == FrameBuffer) {
			return GetInteger<GLuint>(GL_FRAMEBUFFER_BINDING);
		} else if constexpr (T == VertexArray) {
			return GetInteger<GLuint>(GL_VERTEX_ARRAY_BINDING);
		} else if constexpr (T == Shader) {
			return GetInteger<GLuint>(GL_CURRENT_PROGRAM);
		} else {
			static_assert(false, "Unsupported GLResource in GetBoundId()");
		}
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

	template <GLResource T>
	[[nodiscard]] bool IsBound(const Handle<T>& handle) const {
		return GetBoundId<T>() == handle.Get().id;
	}

	void SetVertexBuffer(
		Handle<GLResource::VertexArray>& vertex_array,
		const Handle<GLResource::VertexBuffer>& vertex_buffer
	) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting vertex buffer"
		);
		vertex_array.Get().vertex_buffer = vertex_buffer;
		Bind(vertex_buffer);
	}

	void SetElementBuffer(
		Handle<GLResource::VertexArray>& vertex_array,
		const Handle<GLResource::ElementBuffer>& element_buffer
	) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting element buffer"
		);
		vertex_array.Get().element_buffer = element_buffer;
		Bind(element_buffer);
	}

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetVertexArrayLayout(const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			!layout.IsEmpty(),
			"Cannot add a vertex buffer with an empty (unset) layout to a vertex array"
		);

		const auto& elements{ layout.GetElements() };
		PTGN_ASSERT(
			elements.size() < GetInteger<GLuint>(GL_MAX_VERTEX_ATTRIBS),
			"Vertex buffer layout cannot exceed maximum number of vertex array attributes"
		);

		auto stride{ layout.GetStride() };
		PTGN_ASSERT(stride > 0, "Failed to calculate buffer layout stride");

		for (std::uint32_t i{ 0 }; i < elements.size(); ++i) {
			const auto& element{ elements[i] };
			GLCall(EnableVertexAttribArray(i));
			if (element.is_integer) {
				GLCall(VertexAttribIPointer(
					i, element.count, static_cast<GLenum>(element.type), stride,
					reinterpret_cast<const void*>(element.offset)
				));
				return;
			}
			GLCall(VertexAttribPointer(
				i, element.count, static_cast<GLenum>(element.type),
				element.normalized ? static_cast<GLboolean>(GL_TRUE)
								   : static_cast<GLboolean>(GL_FALSE),
				stride, reinterpret_cast<const void*>(element.offset)
			));
		}
	}

private:
	template <typename T = GLint>
	static T GetInteger(GLenum pname) {
		GLint value = -1;
		GLCall(glGetIntegerv(pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
		return static_cast<T>(value);
	}

	// WARNING: This function is slow and should be
	// primarily used for debugging frame buffers.
	// @param coordinate Pixel coordinate from [0, size).
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture
	// ids.
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	[[nodiscard]] Color GetFrameBufferPixel(
		const Handle<GLResource::FrameBuffer>& handle, const V2_int& coordinate,
		bool restore_bind_state = true
	) {
		// TODO: Allow reading pixels from stencil or depth buffers.

		V2_int size{ handle.Get().texture.Get().size };
		PTGN_ASSERT(
			coordinate.x >= 0 && coordinate.x < size.x,
			"Cannot get pixel out of range of frame buffer texture"
		);
		PTGN_ASSERT(
			coordinate.y >= 0 && coordinate.y < size.y,
			"Cannot get pixel out of range of frame buffer texture"
		);
		std::optional<GLuint> restore_texture_id;
		std::optional<GLuint> restore_frame_buffer_id;
		if (restore_bind_state) {
			restore_texture_id		= GetBoundId<GLResource::Texture>();
			restore_frame_buffer_id = GetBoundId<GLResource::FrameBuffer>();
		}
		Bind(handle.Get().texture);
		auto components{ GetColorComponentCount(handle.Get().texture.Get().internal_format) };
		PTGN_ASSERT(
			components >= 3,
			"Textures with less than 3 pixel components cannot currently be queried"
		);
		std::vector<std::uint8_t> v(static_cast<std::size_t>(components * 1 * 1));
		int y{ size.y - 1 - coordinate.y };
		PTGN_ASSERT(y >= 0);
		Bind(handle);
		GLCall(glReadPixels(
			coordinate.x, y, 1, 1, handle.Get().texture.Get().pixel_format, GL_UNSIGNED_BYTE,
			static_cast<void*>(v.data())
		));
		if (restore_texture_id.has_value()) {
			BindId<GLResource::Texture>(*restore_texture_id);
		}
		if (restore_frame_buffer_id.has_value()) {
			BindId<GLResource::FrameBuffer>(*restore_frame_buffer_id);
		}
		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}

	// WARNING: This function is slow and should be
	// primarily used for debugging frame buffers.
	// @param callback Function to be called for each pixel.
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture
	// ids. Note: Only RGB/RGBA format textures supported.
	void ForEachFrameBufferPixel(
		const Handle<GLResource::FrameBuffer>& handle,
		const std::function<void(V2_int, Color)>& func, bool restore_bind_state = true
	) {
		// TODO: Allow reading pixels from stencil or depth buffers.

		V2_int size{ handle.Get().texture.Get().size };

		std::optional<GLuint> restore_texture_id;
		std::optional<GLuint> restore_frame_buffer_id;
		if (restore_bind_state) {
			restore_texture_id		= GetBoundId<GLResource::Texture>();
			restore_frame_buffer_id = GetBoundId<GLResource::FrameBuffer>();
		}

		Bind(handle.Get().texture);
		auto components{ GetColorComponentCount(handle.Get().texture.Get().internal_format) };
		PTGN_ASSERT(
			components >= 3,
			"Textures with less than 3 pixel components cannot currently be queried"
		);

		std::vector<std::uint8_t> v(static_cast<std::size_t>(components * size.x * size.y));
		Bind(handle);
		GLCall(glReadPixels(
			0, 0, size.x, size.y, handle.Get().texture.Get().pixel_format, GL_UNSIGNED_BYTE,
			static_cast<void*>(v.data())
		));
		for (int j{ 0 }; j < size.y; j++) {
			// Ensure left-to-right and top-to-bottom iteration.
			int row{ (size.y - 1 - j) * size.x * components };
			for (int i{ 0 }; i < size.x; i++) {
				int idx{ row + i * components };
				PTGN_ASSERT(static_cast<std::size_t>(idx) < v.size());
				Color color{ v[static_cast<std::size_t>(idx)], v[static_cast<std::size_t>(idx + 1)],
							 v[static_cast<std::size_t>(idx + 2)],
							 components == 4 ? v[static_cast<std::size_t>(idx + 3)]
											 : static_cast<std::uint8_t>(255) };
				func(V2_int{ i, j }, color);
			}
		}
		if (restore_texture_id.has_value()) {
			BindId<GLResource::Texture>(*restore_texture_id);
		}
		if (restore_frame_buffer_id.has_value()) {
			BindId<GLResource::FrameBuffer>(*restore_frame_buffer_id);
		}
	}

	[[nodiscard]] constexpr static int GetColorComponentCount(GLenum internal_format) {
		switch (internal_format) {
			case GL_STENCIL_INDEX:	 return 1; // stencil only
			case GL_DEPTH_COMPONENT: return 1; // depth only
			case GL_DEPTH_STENCIL:	 return 2; // depth + stencil

			case GL_RED:			 return 1;
			case GL_GREEN:			 return 1;
			case GL_BLUE:			 return 1;

			case GL_RG:				 return 2; // red + green
			case GL_RGB:			 return 3; // red + green + blue
			case GL_BGR:			 return 3; // blue + green + red (different order)
			case GL_RGBA:			 return 4; // red + green + blue + alpha
			case GL_BGRA:			 return 4;			   // blue + green + red + alpha (different order)

			default:
				PTGN_ASSERT(false, "Unknown or unsupported internal GL format: ", internal_format);
				return 0;
		}
	}

	[[nodiscard]] bool FrameBufferIsComplete(const Handle<GLResource::FrameBuffer>& handle) const {
		PTGN_ASSERT(IsBound(handle), "Cannot check status of frame buffer until it is bound");
		auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
		return status == GL_FRAMEBUFFER_COMPLETE;
	}

	[[nodiscard]] const char* GetFrameBufferStatus() const {
		auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
		switch (status) {
			case GL_FRAMEBUFFER_COMPLETE: return "Framebuffer is complete.";
			case GL_FRAMEBUFFER_UNDEFINED:
				return "Framebuffer is undefined (no framebuffer bound).";
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				return "Incomplete attachment: One or more framebuffer attachment points are "
					   "incomplete.";
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				return "Missing attachment: No images are attached to the framebuffer.";
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				return "Incomplete draw buffer: Draw buffer points to a missing attachment.";
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				return "Incomplete read buffer: Read buffer points to a missing attachment.";
			case GL_FRAMEBUFFER_UNSUPPORTED:
				return "Framebuffer unsupported: Format combination not supported by "
					   "implementation.";
			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
				return "Incomplete multisample: Mismatched sample counts or improper use of "
					   "multisampling.";
			case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
				return "Incomplete layer targets: Layered attachments are not all complete or not "
					   "matching.";
			default: return "Unknown framebuffer status.";
		}
	}

	void Resize(Handle<GLResource::FrameBuffer>& handle, const V2_int& new_size) {
		Resize(handle.Get().texture, new_size);
		Resize(handle.Get().render_buffer, new_size);
	}

	void Resize(Handle<GLResource::RenderBuffer>& handle, const V2_int& new_size) {
		if (handle && handle.Get().size == new_size) {
			return;
		}

		auto restore_render_buffer_id{ GetBoundId<GLResource::RenderBuffer>() };

		Bind(handle);

		SetRenderBufferStorage(handle, new_size, handle.Get().internal_format);

		BindId<GLResource::RenderBuffer>(restore_render_buffer_id);
	}

	void SetRenderBufferStorage(
		Handle<GLResource::RenderBuffer>& handle, const V2_int& size, GLenum internal_format
	) const {
		PTGN_ASSERT(IsBound(handle), "Render buffer must be bound prior to setting its storage");

		GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

		handle.Get().size			 = size;
		handle.Get().internal_format = internal_format;
	}

	template <typename T = GLint>
	T GetBufferParameter(GLenum target, GLenum pname) {
		GLint value = -1;
		GLCall(GetBufferParameteriv(target, pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
		return static_cast<T>(value);
	}

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

		resource_list.push_back(resource);

		Handle<T> handle{ std::move(resource) };

		BindId<GLResource::VertexArray>(0);
		Bind(handle);

		GLCall(glBufferData(target, size, data, usage));

		return handle;
	}

	template <GLResource T>
	void SetBufferSubData(
		const Handle<T>& handle, GLenum target, const void* data, std::int32_t byte_offset,
		std::uint32_t element_count, std::uint32_t element_size, bool unbind_vertex_array,
		bool buffer_orphaning
	) {
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		PTGN_ASSERT(data != nullptr);

		if (unbind_vertex_array) {
			// Ensure that this buffer does not get bound to any currently bound vertex array.
			BindId<GLResource::VertexArray>(0);
		}

		Bind(handle);

		std::uint32_t size{ element_count * element_size };

		// This buffer size check must be done after the buffer is bound.
		PTGN_ASSERT(
			size <= GetBufferParameter(GL_ARRAY_BUFFER, GL_BUFFER_SIZE),
			"Attempting to bind data outside of allocated buffer size"
		);

		auto usage{ handle.Get().usage };
		auto count{ handle.Get().count };

		if (buffer_orphaning && (usage == GL_DYNAMIC_DRAW || usage == GL_STREAM_DRAW)) {
			std::uint32_t buffer_size{ count * element_size };
			PTGN_ASSERT(
				buffer_size <= GetBufferParameter(GL_ARRAY_BUFFER, GL_BUFFER_SIZE),
				"Buffer element size does not appear to match the "
				"originally allocated buffer element size"
			);
			GLCall(BufferData(target, buffer_size, nullptr, usage));
		}

		GLCall(BufferSubData(target, byte_offset, size, data));
	}

	template <GLResource T>
	static constexpr void DeleteId(GLuint id) {
		if (id == 0) {
			return; // nothing to delete
		}

		using enum ptgn::impl::gl::GLResource;

		if constexpr (T == VertexBuffer || T == ElementBuffer || T == UniformBuffer) {
			GLCall(DeleteBuffers(1, &id));
		} else if constexpr (T == Texture) {
			GLCall(glDeleteTextures(1, &id));
		} else if constexpr (T == RenderBuffer) {
			GLCall(DeleteRenderbuffers(1, &id));
		} else if constexpr (T == FrameBuffer) {
			GLCall(DeleteFramebuffers(1, &id));
		} else if constexpr (T == VertexArray) {
			GLCall(DeleteVertexArrays(1, &id));
		} else if constexpr (T == Shader) {
			GLCall(DeleteProgram(id));
		} else {
			static_assert(false, "Unsupported GLResource type in DeleteId()");
		}
	}

	template <GLResource T, typename ResourceType>
	static std::shared_ptr<ResourceType> MakeGLResource() {
		return std::shared_ptr<ResourceType>(new ResourceType{}, [](ResourceType* res) {
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