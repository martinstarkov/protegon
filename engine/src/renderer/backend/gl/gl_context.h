#pragma once

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_handle.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/resources/buffer_layout.h"
#include "renderer/resources/shader.h"

CMRC_DECLARE(shader);

#ifdef __EMSCRIPTEN__

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 0;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 3;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

#define PTGN_IMPL_BLEND_CASE(name, srcRGB, dstRGB, srcA, dstA) \
	case BlendMode::name: GLCall(BlendFuncSeparate(srcRGB, dstRGB, srcA, dstA)); break;

struct SDL_GLContextState;

namespace ptgn {

class Window;

} // namespace ptgn

namespace ptgn::impl::gl {

class GLContext;

struct ShaderOptions {
	bool auto_layout{ false };
	bool batchable{ false };
};

struct ShaderTypeSource {
	GLuint type{ GL_FRAGMENT_SHADER };
	ShaderCode source;
	std::string name; // optional name for shader.
	ShaderOptions options;
};

template <GLResource R>
class BindGuard {
public:
	BindGuard(GLContext& gl, GLuint id, bool restore_bind) :
		gl_{ gl }, id_{ id }, restore_bind_{ restore_bind } {}

	~BindGuard() noexcept;

	BindGuard(BindGuard&&) noexcept			   = delete;
	BindGuard& operator=(BindGuard&&) noexcept = delete;
	BindGuard(const BindGuard&)				   = delete;
	BindGuard& operator=(const BindGuard&)	   = delete;

private:
	GLContext& gl_;
	GLuint id_{ 0 };
	bool restore_bind_{ false };
};

class GLContext {
public:
	GLContext() = delete;
	explicit GLContext(Window& window);
	~GLContext() noexcept;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	StrongGLHandle<VertexBuffer> CreateVertexBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	);

	StrongGLHandle<ElementBuffer> CreateElementBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	);

	StrongGLHandle<UniformBuffer> CreateUniformBuffer(
		const void* data, std::uint32_t size, GLenum usage
	);

	StrongGLHandle<Shader> CreateShader(
		GLuint vertex, GLuint fragment, const std::string& shader_name
	);

	// String can be path to shader or the name of a pre-existing shader of the respective type.
	StrongGLHandle<Shader> CreateShader(
		std::variant<ShaderCode, std::string> vertex,
		std::variant<ShaderCode, std::string> fragment, const std::string& shader_name
	);

	StrongGLHandle<Shader> CreateShader(
		std::variant<ShaderCode, path> source, const std::string& shader_name
	);

	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	template <bool kRestoreBind = true>
	StrongGLHandle<Texture> CreateTexture(
		const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type, V2_int size,
		GLenum internal_format
	) {
		auto texture{ CreateTextureImpl() };

		auto _ = Bind<Texture, kRestoreBind>(texture);

		SetTextureData(
			texture, pixel_data, pixel_data_format, pixel_data_type, size, internal_format
		);

		SetTextureParameter(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetTextureParameter(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		SetTextureParameter(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		SetTextureParameter(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return texture;
	}

	template <bool kRestoreBind = true>
	StrongGLHandle<RenderBuffer> CreateRenderBuffer(V2_int size, GLenum internal_format) {
		auto renderbuffer{ CreateRenderBufferImpl() };

		auto _ = Bind<RenderBuffer, kRestoreBind>(renderbuffer);

		SetRenderBufferStorage(renderbuffer, size, internal_format);

		return renderbuffer;
	}

	template <bool kRestoreBind = true>
	StrongGLHandle<FrameBuffer> CreateFrameBuffer(
		GLuint texture, GLenum texture_attachment = GL_COLOR_ATTACHMENT0, GLuint renderbuffer = 0,
		GLenum renderbuffer_attachment = GL_DEPTH_STENCIL_ATTACHMENT
	) {
		PTGN_ASSERT(
			texture || renderbuffer,
			"Must provide at least one valid image attachment when creating a framebuffer"
		);

		auto framebuffer{ CreateFrameBufferImpl() };
		auto _ = Bind<FrameBuffer, kRestoreBind>(framebuffer);

		if (texture) {
			AttachTexture(framebuffer, texture, texture_attachment);
		}

		if (renderbuffer) {
			AttachRenderBuffer(framebuffer, renderbuffer, renderbuffer_attachment);
		}

		PTGN_ASSERT(FrameBufferIsComplete(framebuffer));

		return framebuffer;
	}

	template <bool kRestoreBind = true, typename... Ts>
	StrongGLHandle<VertexArray> CreateVertexArray(
		GLuint vertex_buffer, const BufferLayout<Ts...>& vertex_buffer_layout, GLuint element_buffer
	) {
		auto vertex_array{ CreateVertexArrayImpl() };

		auto _ = Bind<VertexArray, kRestoreBind>(vertex_array);

		SetVertexBuffer(vertex_array, vertex_buffer);
		SetElementBuffer(vertex_array, element_buffer);
		SetBufferLayout(vertex_array, vertex_buffer_layout);

		return vertex_array;
	}

	template <GLResource R, bool kRestoreBind>
	[[nodiscard]] BindGuard<R> Bind(GLuint id) {
		auto previous{ GetBound<R>() };

		if (id == previous) {
			return BindGuard<R>{ *this, GLuint{ 0 }, false };
		}

		if constexpr (R == VertexBuffer) {
			GLCall(BindBuffer(GL_ARRAY_BUFFER, id));
			bound_.vertex_buffer = id;
		} else if constexpr (R == ElementBuffer) {
			GLCall(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
			if (bound_.vertex_array) {
				vertex_array_cache_.Get(bound_.vertex_array).element_buffer = id;
			}
		} else if constexpr (R == UniformBuffer) {
			GLCall(BindBuffer(GL_UNIFORM_BUFFER, id));
			bound_.uniform_buffer = id;
		} else if constexpr (R == Shader) {
			GLCall(UseProgram(id));
			bound_.shader = id;
		} else if constexpr (R == RenderBuffer) {
			GLCall(BindRenderbuffer(GL_RENDERBUFFER, id));
			bound_.renderbuffer = id;
		} else if constexpr (R == Texture) {
			auto slot{ GetActiveTextureSlot() };
			PTGN_ASSERT(slot < GetMaxTextureSlots(), "Slot out of range of max slots");
			PTGN_ASSERT(bound_.texture_units[slot].id != id);
			GLCall(glBindTexture(GL_TEXTURE_2D, id));
			bound_.texture_units[slot].id = id;
		} else if constexpr (R == FrameBuffer) {
			GLCall(BindFramebuffer(GL_FRAMEBUFFER, id));
			bound_.framebuffer = id;
		} else if constexpr (R == VertexArray) {
#ifdef PTGN_PLATFORM_MACOS
			// MacOS complains about binding 0 id vertex array.
			if (id != 0) {
				GLCall(BindVertexArray(id));
			}
#else
			GLCall(BindVertexArray(id));
#endif
			bound_.vertex_array = id;
		} else {
			static_assert(false, "Unsupported GLResource type");
		}

		return BindGuard<R>{ *this, previous, kRestoreBind };
	}

	template <GLResource R>
	[[nodiscard]] GLuint GetBound() const {
		if constexpr (R == VertexBuffer) {
			return bound_.vertex_buffer;
		} else if constexpr (R == ElementBuffer) {
			return bound_.vertex_array ? vertex_array_cache_.Get(bound_.vertex_array).element_buffer
									   : 0;
		} else if constexpr (R == UniformBuffer) {
			return bound_.uniform_buffer;
		} else if constexpr (R == Texture) {
			PTGN_ASSERT(bound_.active_texture_slot < GetMaxTextureSlots());
			return bound_.texture_units[bound_.active_texture_slot].id;
		} else if constexpr (R == RenderBuffer) {
			return bound_.renderbuffer;
		} else if constexpr (R == FrameBuffer) {
			return bound_.framebuffer;
		} else if constexpr (R == VertexArray) {
			return bound_.vertex_array;
		} else if constexpr (R == Shader) {
			return bound_.shader;
		} else {
			static_assert(false, "Unsupported GLResource type");
		}
	}

	template <GLResource R>
	[[nodiscard]] bool IsBound(GLuint id) const {
		return GetBound<R>() == id;
	}

	bool IsBatchableShader(GLuint shader) const;

	void AttachTexture(GLuint framebuffer, GLuint texture, GLenum texture_attachment);

	V2_int GetTextureSize(GLuint texture) const;

	void AttachRenderBuffer(
		GLuint framebuffer, GLuint renderbuffer, GLenum renderbuffer_attachment
	);

	void SetVertexBuffer(GLuint vertex_array, GLuint vertex_buffer);

	void SetElementBuffer(GLuint vertex_array, GLuint element_buffer);

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetBufferLayout(GLuint vertex_array, const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			IsBound<VertexArray>(vertex_array),
			"Vertex array must be bound before setting its buffer layout"
		);

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
					i, element.count, ToGLType(element.type), stride,
					reinterpret_cast<const void*>(element.offset)
				));
			} else {
				GLCall(VertexAttribPointer(
					i, element.count, ToGLType(element.type),
					element.normalized ? GL_TRUE : GL_FALSE, stride,
					reinterpret_cast<const void*>(element.offset)
				));
			}
		}

		vertex_array_cache_.Get(vertex_array).layout_set = true;
	}

	void EnableGammaCorrection();
	void DisableGammaCorrection();

	void SetDepthMask(GLboolean enabled);

	// Enabling blending will disable depth testing.
	void SetBlending(GLboolean enabled);

	void SetDepthFunc(GLenum depth_func);

	// Enabling depth testing will disable blending.
	void SetDepthTesting(GLboolean enabled);

	void SetDepthRange(float near_val, float far_val);

	void SetLineWidth(float width);
	void SetLineSmoothing(bool enabled);

	void SetPolygonMode(GLenum front_mode, GLenum back_mode);

	// Will disable depth testing.
	void SetBlendMode(BlendMode mode);

	void DrawElements(
		GLuint vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
	) const;

	void DrawArrays(GLuint vertex_array, GLsizei vertex_count, GLenum primitive_mode) const;

	void SetViewport(const Viewport& viewport);
	[[nodiscard]] Viewport GetViewport() const;

	void SetClearColor(Color color);
	void Clear();
	void ClearToColor(GLuint framebuffer, Color color) const;

	void SetColorMask(const ColorMaskState& mask);
	void SetScissor(const ScissorState& scissor);
	void SetCull(const CullState& cull);
	void SetStencil(const StencilState& stencil);

	void SetUniform(GLuint shader, const char* uniform_name, V2_float v);
	void SetUniform(GLuint shader, const char* uniform_name, V3_float v);
	void SetUniform(GLuint shader, const char* uniform_name, V4_float v);
	void SetUniform(GLuint shader, const char* uniform_name, const Matrix4& matrix);

	void SetUniform(
		GLuint shader, const char* uniform_name, const std::int32_t* data, std::int32_t count
	);

	void SetUniform(GLuint shader, const char* uniform_name, const float* data, std::int32_t count);

	void SetUniform(GLuint shader, const char* uniform_name, const Vector2<std::int32_t>& v);
	void SetUniform(GLuint shader, const char* uniform_name, const Vector3<std::int32_t>& v);
	void SetUniform(GLuint shader, const char* uniform_name, const Vector4<std::int32_t>& v);

	void SetUniform(GLuint shader, const char* uniform_name, float v0);
	void SetUniform(GLuint shader, const char* uniform_name, float v0, float v1);
	void SetUniform(GLuint shader, const char* uniform_name, float v0, float v1, float v2);
	void SetUniform(
		GLuint shader, const char* uniform_name, float v0, float v1, float v2, float v3
	);

	void SetUniform(GLuint shader, const char* uniform_name, std::int32_t v0);
	void SetUniform(GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1);
	void SetUniform(
		GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
	);
	void SetUniform(
		GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2,
		std::int32_t v3
	);

	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(GLuint shader, const char* uniform_name, bool value);

	[[nodiscard]] StrongGLHandle<Shader> GetShader(std::string_view shader_name) const;

	void SetActiveTextureSlot(GLuint slot);

	/// @param target OpenGL buffer binding point (e.g. GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
	/// GL_UNIFORM_BUFFER)
	template <GLResource R, bool kBufferOrphaning = true>
		requires(R == VertexBuffer || R == ElementBuffer || R == UniformBuffer)
	void SetBufferSubData(
		GLuint id, GLenum target, const void* data, std::int32_t byte_offset,
		std::uint32_t element_count, std::uint32_t element_size
	) const {
		PTGN_ASSERT(IsBound<R>(id), "Buffer must be bound before setting its subdata");
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		PTGN_ASSERT(data != nullptr);

		std::uint32_t size{ element_count * element_size };

		// This buffer size check must be done after the buffer is bound.
		PTGN_ASSERT(
			(size <= GetBufferParameter<GLuint>(GL_ARRAY_BUFFER, GL_BUFFER_SIZE)),
			"Attempting to bind data outside of allocated buffer size"
		);

		if constexpr (kBufferOrphaning) {
			const auto& cache{ buffer_cache_.Get(id) };

			if (cache.usage == GL_DYNAMIC_DRAW || cache.usage == GL_STREAM_DRAW) {
				std::uint32_t buffer_size{ cache.count * element_size };
				PTGN_ASSERT(buffer_size > 0);
				PTGN_ASSERT(
					(buffer_size <= GetBufferParameter<GLuint>(GL_ARRAY_BUFFER, GL_BUFFER_SIZE)),
					"Buffer element size does not appear to match the "
					"originally allocated buffer element size"
				);
				GLCall(BufferData(target, buffer_size, nullptr, cache.usage));
			}
		}

		GLCall(BufferSubData(target, byte_offset, size, data));
	}

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] std::size_t GetMaxTextureSlots() const;

	enum class AttachmentDataType {
		Color,
		Depth,
		Stencil,
		DepthStencil
	};

	// Color
	// Depth -> float
	// Stencil -> uint8_t
	// Depth+Stencil -> {float depth, uint8_t stencil}
	using PixelValue = std::variant<Color, float, std::uint8_t, std::pair<float, std::uint8_t>>;

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param coordinate Pixel coordinate from [0, size).
	PixelValue ReadPixel(
		GLuint framebuffer, V2_int coordinate, GLenum attachment = GL_COLOR_ATTACHMENT0
	);

	struct PixelBuffer {
		V2_int size{};
		AttachmentDataType type{};
		std::vector<std::uint8_t> data;
	};

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	PixelBuffer ReadPixels(GLuint framebuffer, GLenum attachment = GL_COLOR_ATTACHMENT0);

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	template <typename F>
	void ForEachPixel(
		const PixelBuffer& buffer, F&& func /* (V2_int, PixelValue) */
	) {
		const auto& data			  = buffer.data;
		const V2_int size			  = buffer.size;
		const AttachmentDataType type = buffer.type;

		for (int y = 0; y < size.y; ++y) {
			int flipped = size.y - 1 - y;

			for (int x = 0; x < size.x; ++x) {
				const int idx = flipped * size.x + x;
				PixelValue px;

				switch (type) {
					case AttachmentDataType::Color: {
						const std::uint8_t* p = &data[idx * 4];
						px					  = Color{ p[0], p[1], p[2], p[3] };
						break;
					}

					case AttachmentDataType::Depth: {
						const float* p = reinterpret_cast<const float*>(data.data());
						px			   = p[idx];
						break;
					}

					case AttachmentDataType::Stencil: {
						px = data[idx];
						break;
					}

					case AttachmentDataType::DepthStencil: {
						const auto* p = reinterpret_cast<const std::uint32_t*>(data.data());
						const std::uint32_t packed = p[idx];
						float depth				   = float(packed & 0xFFFFFF) / float(0xFFFFFF);
						std::uint8_t stencil	   = (packed >> 24) & 0xFF;
						px						   = std::make_pair(depth, stencil);
						break;
					}
				}

				func(V2_int{ x, y }, px);
			}
		}
	}

	template <typename F>
	void ForEachPixel(GLuint framebuffer, F&& func, GLenum attachment = GL_COLOR_ATTACHMENT0) {
		PixelBuffer buffer = ReadPixels(framebuffer, attachment);
		ForEachPixel(buffer, std::forward<F>(func));
	}

	void SavePNG(const path& path, GLuint framebuffer, GLenum attachment = GL_COLOR_ATTACHMENT0);

private:
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

	[[nodiscard]] bool FrameBufferIsComplete(GLuint framebuffer) const;

	[[nodiscard]] const char* GetFrameBufferStatus();

	AttachmentDataType GetAttachmentDataType(GLenum attachment) const;

	AttachmentInfo& GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment);

	const AttachmentInfo& GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment) const;

	void UpdateFrameBufferCache(
		GLenum framebuffer, GLuint image_id, GLenum attachment, GLenum image_type
	);

	[[nodiscard]] std::vector<ShaderTypeSource> ParseShaderSourceFile(
		const std::string& source, const std::string& name, std::size_t max_texture_slots
	) const;

	void PopulateShadersFromCache(const json& manifest);

	void CompileShaders(
		const std::vector<ShaderTypeSource>& sources,
		std::unordered_map<std::size_t, GLuint>& vertex_shaders,
		std::unordered_map<std::size_t, GLuint>& fragment_shaders,
		std::vector<GLuint>& batchable_shaders
	) const;

	void PopulateShaderCache(
		const cmrc::embedded_filesystem& filesystem,
		std::unordered_map<std::size_t, GLuint>& vertex_shaders,
		std::unordered_map<std::size_t, GLuint>& fragment_shaders,
		std::vector<GLuint>& batchable_shaders, std::size_t max_texture_slots
	) const;

	[[nodiscard]] GLuint CompileShaderSource(
		const std::string& source, GLenum type, const std::string& name,
		std::size_t max_texture_slots
	) const;

	GLuint CompileShaderPath(
		const path& shader_path, GLenum type, const std::string& name, std::size_t max_texture_slots
	) const;

	void CompileShader(
		GLuint shader, const std::string& vertex_source, const std::string& fragment_source
	) const;

	[[nodiscard]] GLuint CompileShaderFromSource(GLenum type, const std::string& source) const;

	void LinkShader(GLuint shader, GLuint vertex, GLuint fragment);

	[[nodiscard]] std::int32_t GetUniform(GLuint shader, const char* name);

	template <GLResource R>
		requires(R == VertexBuffer || R == ElementBuffer || R == UniformBuffer)
	StrongGLHandle<R> CreateBufferImpl(
		GLenum target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
		GLenum usage
	) {
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		auto id = new GLuint{ 0 };
		GLCall(GenBuffers(1, id));

		PTGN_ASSERT(id && *id, "Failed to create buffer");

		auto _1 = Bind<VertexArray, true>(0);
		auto _2 = Bind<R, false>(*id);

		const std::uint32_t size = element_count * element_size;

		GLCall(BufferData(target, size, data, usage));

		buffer_cache_.Add(*id, BufferCache{ .usage = usage, .count = element_count });

		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(DeleteBuffers(1, id));
				buffer_cache_.Remove(*id);
			}
			delete id;
		});
	}

	template <typename T = GLint>
	T GetInteger(GLenum pname) const {
		GLint value = -1;
		GLCall(glGetIntegerv(pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
		return static_cast<T>(value);
	}

	void ResizeFrameBuffer(GLuint framebuffer, V2_int new_size);

	void ResizeRenderBuffer(GLuint renderbuffer, V2_int new_size);

	void ResizeTexture(GLuint texture, V2_int new_size);

	void SetRenderBufferStorage(GLuint renderbuffer, V2_int size, GLenum internal_format);

	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	void SetTextureData(
		GLuint texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int size, GLenum internal_format
	);

	void SetTextureSubData(
		GLuint texture, const void* pixel_subdata, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int subdata_size, V2_int subdata_offset
	) const;

	void SetTextureClampBorderColor(GLuint texture, Color color) const;

	void SetTextureParameter(GLuint texture, GLenum param, const GLfloat* values) const;
	void SetTextureParameter(GLuint texture, GLenum param, const GLint* values) const;
	void SetTextureParameter(GLuint texture, GLenum param, GLfloat value) const;
	void SetTextureParameter(GLuint texture, GLenum param, GLint value) const;

	[[nodiscard]] GLint GetTextureParameter(GLuint texture, GLenum param) const;

	[[nodiscard]] GLuint GetActiveTextureSlot() const;

	template <typename T = GLint>
	T GetBufferParameter(GLenum target, GLenum pname) const {
		GLint value = -1;
		GLCall(GetBufferParameteriv(target, pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
		return static_cast<T>(value);
	}

	/// Ensure that the texture scaling of the currently bound texture is valid for generating
	/// mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(GLenum texture_min_filter);

	void GenerateMipmaps(GLuint texture) const;

	[[nodiscard]] StrongGLHandle<Shader> CreateShaderImpl(const std::string& shader_name);

	[[nodiscard]] StrongGLHandle<VertexArray> CreateVertexArrayImpl();

	[[nodiscard]] StrongGLHandle<FrameBuffer> CreateFrameBufferImpl();

	[[nodiscard]] StrongGLHandle<Texture> CreateTextureImpl();

	[[nodiscard]] StrongGLHandle<RenderBuffer> CreateRenderBufferImpl();

	// TODO: Make sure to update the cache when the parameters change. I.e. when resizing a
	// texture.
	// TODO: Store resource cached values here by GLuint key and erase them in the resource
	// deleter. e.g. std::unordered_map<GLuint, location_cache> location_caches_;
	// UnsafeDelete(); location_caches_.erase(id);

	std::unordered_map<std::size_t, StrongGLHandle<Shader>> shaders_;

	IdMap<GLuint, ShaderCache> shader_cache_;
	IdMap<GLuint, TextureCache> texture_cache_;
	IdMap<GLuint, FrameBufferCache> framebuffer_cache_;
	IdMap<GLuint, RenderBufferCache> renderbuffer_cache_;
	IdMap<GLuint, BufferCache> buffer_cache_;
	IdMap<GLuint, VertexArrayCache> vertex_array_cache_;

	State bound_;

	std::unordered_map<std::size_t, GLuint> vertex_shaders_;
	std::unordered_map<std::size_t, GLuint> fragment_shaders_;
	std::vector<GLuint> batchable_shaders_;

	SDL_GLContextState* context_{ nullptr };
};

constexpr GLenum ToGLType(BufferElementType type) noexcept {
	switch (type) {
		case BufferElementType::Float:	return GL_FLOAT;
		case BufferElementType::Double: return GL_DOUBLE;
		case BufferElementType::Int:	return GL_INT;
		case BufferElementType::UInt:	return GL_UNSIGNED_INT;
		case BufferElementType::Short:	return GL_SHORT;
		case BufferElementType::UShort: return GL_UNSIGNED_SHORT;
		case BufferElementType::Byte:	return GL_BYTE;
		case BufferElementType::UByte:	return GL_UNSIGNED_BYTE;
		case BufferElementType::Bool:	return GL_BOOL;
	}
	return GL_FLOAT;
}

template <GLResource R>
BindGuard<R>::~BindGuard() noexcept {
	if (restore_bind_) {
		auto _ = gl_.Bind<R, false>(id_);
	}
}

} // namespace ptgn::impl::gl