#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/buffer_layout.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/texture.h"

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

struct TextureFormatDesc {
	GLenum internal_format{ 0 };
	GLenum pixel_format{ 0 };
	GLenum pixel_type{ 0 };

	bool has_depth{ false };
	bool has_stencil{ false };
	bool is_srgb{ false };
};

constexpr TextureFormatDesc GetTextureFormatDesc(TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;

		// -----------------------------------------------------------------
		// Most common color formats (put first)
		// -----------------------------------------------------------------
		case RGBA8:		 return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false };
		case RGBA8_SRGB: return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false, true };
		case RGBA16F:	 return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false, false, false };
		case RGBA32F:	 return { GL_RGBA32F, GL_RGBA, GL_FLOAT, false, false, false };

		case RG8:		 return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false, false, false };
		case R8:		 return { GL_R8, GL_RED, GL_UNSIGNED_BYTE, false, false, false };

		// -----------------------------------------------------------------
		// Color (16-bit / float) - missing before
		// -----------------------------------------------------------------
		case R16F:		 return { GL_R16F, GL_RED, GL_HALF_FLOAT, false, false, false };
		case RG16F:		 return { GL_RG16F, GL_RG, GL_HALF_FLOAT, false, false, false };

		// -----------------------------------------------------------------
		// Color (32-bit float) - missing before
		// -----------------------------------------------------------------
		case R32F:		 return { GL_R32F, GL_RED, GL_FLOAT, false, false, false };
		case RG32F:		 return { GL_RG32F, GL_RG, GL_FLOAT, false, false, false };

		// -----------------------------------------------------------------
		// HDR / lighting
		// -----------------------------------------------------------------
		case R11G11B10F:
			return {
				GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false, false, false
			};

		case RGB10_A2:
			return { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, false, false, false };

		// -----------------------------------------------------------------
		// Depth / stencil
		// -----------------------------------------------------------------
		case Depth16:
			return {
				GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, true, false, false
			};

		case Depth24:
			return {
				GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, true, false, false
			};

		case Depth32F:
			return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true, false, false };

		case Depth24_Stencil8:
			return {
				GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, true, true, false
			};

		case Depth32F_Stencil8:
			return { GL_DEPTH32F_STENCIL8,
					 GL_DEPTH_STENCIL,
					 GL_FLOAT_32_UNSIGNED_INT_24_8_REV,
					 true,
					 true,
					 false };

		case Stencil8:
			return { GL_STENCIL_INDEX8, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, true, false };

		default:
			// No UB / no exceptions:
			PTGN_ERROR("Unknown TextureFormat");
	}
}

[[nodiscard]] constexpr int GetColorComponentCount(GLenum internal_format) {
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
		case GL_BGRA:			 return 4; // blue + green + red + alpha (different order)

		default:				 PTGN_ERROR("Unknown or unsupported internal GL format: ", internal_format);
	}
}

enum class Attachment : std::uint32_t {
	// Color attachments
	Color0 = 0x8CE0, // GL_COLOR_ATTACHMENT0
	Color1 = 0x8CE1, // GL_COLOR_ATTACHMENT1
	Color2 = 0x8CE2, // GL_COLOR_ATTACHMENT2
	Color3 = 0x8CE3, // GL_COLOR_ATTACHMENT3
	Color4 = 0x8CE4, // GL_COLOR_ATTACHMENT4
	Color5 = 0x8CE5, // GL_COLOR_ATTACHMENT5
	Color6 = 0x8CE6, // GL_COLOR_ATTACHMENT6
	Color7 = 0x8CE7, // GL_COLOR_ATTACHMENT7
	Color8 = 0x8CE8, // GL_COLOR_ATTACHMENT8

	// Depth / Stencil attachments
	Depth		 = 0x8D00, // GL_DEPTH_ATTACHMENT
	Stencil		 = 0x8D20, // GL_STENCIL_ATTACHMENT
	DepthStencil = 0x821A  // GL_DEPTH_STENCIL_ATTACHMENT
};

template <typename T>
class BindGuard {
public:
	BindGuard(GLContext& gl, T id, bool restore_bind) :
		gl_{ gl }, id_{ id }, restore_bind_{ restore_bind } {}

	~BindGuard() noexcept;

	BindGuard(BindGuard&&) noexcept			   = delete;
	BindGuard& operator=(BindGuard&&) noexcept = delete;
	BindGuard(const BindGuard&)				   = delete;
	BindGuard& operator=(const BindGuard&)	   = delete;

private:
	GLContext& gl_;
	T id_;
	bool restore_bind_{ false };
};

class GLContext {
public:
	GLContext() = delete;
	explicit GLContext(const Window& window);
	~GLContext() noexcept;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	VertexBuffer CreateVertexBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	);

	ElementBuffer CreateElementBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	);

	UniformBuffer CreateUniformBuffer(const void* data, std::uint32_t size, GLenum usage);

	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	Texture CreateTexture(
		const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type, V2_int size,
		GLenum internal_format, bool restore_bind = true
	);

	Renderbuffer CreateRenderbuffer(V2_int size, GLenum internal_format, bool restore_bind = true);

	Framebuffer CreateFramebuffer(
		std::optional<Texture> texture = {}, GLenum texture_attachment = GL_COLOR_ATTACHMENT0,
		std::optional<Renderbuffer> renderbuffer = {},
		GLenum renderbuffer_attachment = GL_DEPTH_STENCIL_ATTACHMENT, bool restore_bind = true
	);

	template <typename... Ts>
	VertexArray CreateVertexArray(
		VertexBuffer vertex_buffer, const BufferLayout<Ts...>& vertex_buffer_layout,
		ElementBuffer element_buffer, bool restore_bind = true
	) {
		auto vertex_array{ CreateVertexArrayImpl() };

		auto _ = Bind(vertex_array, restore_bind);

		SetVertexBuffer(vertex_array, vertex_buffer);
		SetElementBuffer(vertex_array, element_buffer);
		SetBufferLayout(vertex_array, vertex_buffer_layout);

		return vertex_array;
	}

	void DestroyTexture(Texture id);
	void DestroyRenderbuffer(Renderbuffer id);
	void DestroyFramebuffer(Framebuffer id);
	void DestroyVertexArray(VertexArray id);

	[[nodiscard]] BindGuard<VertexBuffer> Bind(VertexBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<ElementBuffer> Bind(ElementBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<UniformBuffer> Bind(UniformBuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Program> Bind(Program id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Texture> Bind(Texture id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Renderbuffer> Bind(Renderbuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<Framebuffer> Bind(Framebuffer id, bool restore_bind = false);
	[[nodiscard]] BindGuard<VertexArray> Bind(VertexArray id, bool restore_bind = false);

	[[nodiscard]] const State& GetBoundState() const;
	[[nodiscard]] State& GetBoundState();
	[[nodiscard]] VertexBuffer GetBoundVertexBuffer() const;
	[[nodiscard]] ElementBuffer GetBoundElementBuffer() const;
	[[nodiscard]] UniformBuffer GetBoundUniformBuffer() const;
	[[nodiscard]] Program GetBoundProgram() const;
	[[nodiscard]] Texture GetBoundTexture() const;
	[[nodiscard]] Renderbuffer GetBoundRenderbuffer() const;
	[[nodiscard]] Framebuffer GetBoundFramebuffer() const;
	[[nodiscard]] VertexArray GetBoundVertexArray() const;

	[[nodiscard]] bool IsBound(VertexBuffer id) const;
	[[nodiscard]] bool IsBound(ElementBuffer id) const;
	[[nodiscard]] bool IsBound(UniformBuffer id) const;
	[[nodiscard]] bool IsBound(Program id) const;
	[[nodiscard]] bool IsBound(Texture id) const;
	[[nodiscard]] bool IsBound(Renderbuffer id) const;
	[[nodiscard]] bool IsBound(Framebuffer id) const;
	[[nodiscard]] bool IsBound(VertexArray id) const;

	void AttachTexture(Framebuffer framebuffer, Texture texture, GLenum texture_attachment);

	V2_int GetTextureSize(Texture texture) const;

	void AttachRenderbuffer(
		Framebuffer framebuffer, Renderbuffer renderbuffer, GLenum renderbuffer_attachment
	);

	void SetVertexBuffer(VertexArray vertex_array, VertexBuffer vertex_buffer);

	void SetElementBuffer(VertexArray vertex_array, ElementBuffer element_buffer);

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetBufferLayout(VertexArray vertex_array, const BufferLayout<Ts...>& layout) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting its buffer layout"
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

	void EnableGammaCorrection() const;
	void DisableGammaCorrection() const;

	// Enabling blending will disable depth testing.
	void SetBlending(bool enabled);
	void SetBlend(const BlendState& blend_state);
	// Will disable depth testing.
	void SetBlendMode(BlendMode mode);

	// Enabling depth testing will disable blending.
	void SetDepthTesting(bool enabled);

	void SetDepth(const DepthState& state);
	void SetDepthMask(bool enabled);
	void SetDepthFunc(CompareFunc depth_func);
	void SetDepthRange(float near_val, float far_val);
	void SetLineWidth(float width);
	void SetLineSmoothing(bool enabled);
	void SetPolygonMode(PolygonMode front_mode, PolygonMode back_mode);
	void SetColorMask(const ColorMaskState& mask);
	void SetScissor(const ScissorState& scissor);
	void SetCull(const CullState& cull);
	void SetRaster(const RasterState& raster);
	void SetStencil(const StencilState& stencil);
	void SetClearColor(Color color);
	void SetClearDepth(double depth);
	void SetClearStencil(int stencil);

	void DrawElements(
		VertexArray vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
	) const;

	void DrawArrays(VertexArray vertex_array, GLsizei vertex_count, GLenum primitive_mode) const;

	void SetViewport(const Viewport& viewport);
	[[nodiscard]] Viewport GetViewport() const;

	/// Clear buffers to preset values
	void Clear(
		GLbitfield buffer_bits = GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
	) const;

	/// Clear individual buffers of a framebuffer.
	/// @param drawbuffer Specify a particular draw buffer to clear.
	/// @param drawbuffer Specify the buffer to clear. Accepted: GL_COLOR, GL_DEPTH, GL_STENCIL.
	void ClearToColor(
		Framebuffer framebuffer, Color color, GLenum buffer = GL_COLOR, GLint drawbuffer = 0
	) const;
	void SetActiveTextureSlot(std::uint32_t slot);

	/// @param target OpenGL buffer binding point (e.g. GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER,
	/// GL_UNIFORM_BUFFER)
	template <typename T, bool kBufferOrphaning = true>
		requires(std::is_same_v<T, VertexBuffer> || std::is_same_v<T, ElementBuffer> || std::is_same_v<T, UniformBuffer>)
	void SetBufferSubData(
		T id, GLenum target, const void* data, std::int32_t byte_offset,
		std::uint32_t element_count, std::uint32_t element_size
	) const {
		PTGN_ASSERT(IsBound(id), "Buffer must be bound before setting its subdata");
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

	// Color
	// Depth -> float
	// Stencil -> uint8_t
	// Depth+Stencil -> {float depth, uint8_t stencil}
	using PixelValue = std::variant<Color, float, std::uint8_t, std::pair<float, std::uint8_t>>;

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param coordinate Pixel coordinate from [0, size).
	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	PixelValue ReadPixel(
		Framebuffer framebuffer, V2_int coordinate, GLenum attachment = GL_COLOR_ATTACHMENT0
	);

	enum class AttachmentDataType {
		Color,
		Depth,
		Stencil,
		DepthStencil
	};

	struct PixelBuffer {
		V2_int size{};
		AttachmentDataType type{};
		std::vector<std::uint8_t> data;
	};

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	PixelBuffer ReadPixels(Framebuffer framebuffer, GLenum attachment = GL_COLOR_ATTACHMENT0);

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	template <typename F>
	void ForEachPixel(
		const PixelBuffer& buffer, F&& func /* (V2_int, PixelValue) */
	) const {
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

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	template <typename F>
	void ForEachPixel(Framebuffer framebuffer, F&& func, GLenum attachment = GL_COLOR_ATTACHMENT0) {
		PixelBuffer buffer = ReadPixels(framebuffer, attachment);
		ForEachPixel(buffer, std::forward<F>(func));
	}

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	void SavePNG(
		const path& path, Framebuffer framebuffer, GLenum attachment = GL_COLOR_ATTACHMENT0
	);

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	AttachmentInfo& GetFramebufferAttachment(Framebuffer framebuffer, GLenum attachment);

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	const AttachmentInfo& GetFramebufferAttachment(Framebuffer framebufferv, GLenum attachment)
		const;

	void ResizeFramebuffer(Framebuffer framebuffer, V2_int new_size);

	void ResizeRenderbuffer(Renderbuffer renderbuffer, V2_int new_size);

	void ResizeTexture(Texture texture, V2_int new_size);

	Shaders shaders;

private:
	[[nodiscard]] bool FramebufferIsComplete(Framebuffer framebuffer) const;

	[[nodiscard]] const char* GetFramebufferStatus() const;

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	AttachmentDataType GetAttachmentDataType(GLenum attachment) const;

	// @param attachment Accepted: GL_COLOR_ATTACHMENT0-8, GL_DEPTH_ATTACHMENT,
	// GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
	void UpdateFramebufferCache(
		Framebuffer framebuffer, GLuint image_id, GLenum attachment, GLenum image_type
	);

	template <typename T>
		requires(std::is_same_v<T, VertexBuffer> || std::is_same_v<T, ElementBuffer> || std::is_same_v<T, UniformBuffer>)
	T CreateBufferImpl(
		GLenum target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
		GLenum usage
	) {
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		T id{ 0 };
		GLCall(GenBuffers(1, &id));

		PTGN_ASSERT(id, "Failed to create buffer");

		auto _1 = Bind(VertexArray{ 0 }, true);
		auto _2 = Bind(id, false);

		const std::uint32_t size = element_count * element_size;

		GLCall(BufferData(target, size, data, usage));

		buffer_cache_.Add(id, BufferCache{ .usage = usage, .count = element_count });

		return id;
	}

	template <typename T>
		requires(std::is_same_v<T, VertexBuffer> || std::is_same_v<T, ElementBuffer> || std::is_same_v<T, UniformBuffer>)
	void DeleteBuffer(T id) {
		if (!id) {
			return;
		}
		GLCall(DeleteBuffers(1, &id));
		buffer_cache_.Remove(id);
	}

	template <typename T = GLint>
	T GetInteger(GLenum pname) const {
		GLint value = -1;
		GLCall(glGetIntegerv(pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query integer parameter");
		return static_cast<T>(value);
	}

	void SetRenderbufferStorage(Renderbuffer renderbuffer, V2_int size, GLenum internal_format);

	/// @param pixel_data_format Accepted: GL_RED, GL_RG, GL_RGB, GL_BGR, GL_RGBA, GL_BGRA,
	/// GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_BGR_INTEGER, GL_RGBA_INTEGER,
	/// GL_BGRA_INTEGER, GL_STENCIL_INDEX, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL
	/// @param internal_format Accepted: GL_RGBA, GL_RGB, GL_RG, GL_RED, GL_DEPTH_STENCIL,
	/// GL_DEPTH_COMPONENT
	void SetTextureData(
		Texture texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int size, GLenum internal_format
	);

	void SetTextureSubData(
		Texture texture, const void* pixel_subdata, GLenum pixel_data_format,
		GLenum pixel_data_type, V2_int subdata_size, V2_int subdata_offset
	) const;

	void SetTextureClampBorderColor(Texture texture, Color color) const;

	void SetTextureParameter(Texture texture, GLenum param, const GLfloat* values) const;
	void SetTextureParameter(Texture texture, GLenum param, const GLint* values) const;
	void SetTextureParameter(Texture texture, GLenum param, GLfloat value) const;
	void SetTextureParameter(Texture texture, GLenum param, GLint value) const;

	[[nodiscard]] GLint GetTextureParameter(Texture texture, GLenum param) const;

	[[nodiscard]] std::uint32_t GetActiveTextureSlot() const;

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

	void GenerateMipmaps(Texture texture) const;

	[[nodiscard]] VertexArray CreateVertexArrayImpl();

	[[nodiscard]] Framebuffer CreateFramebufferImpl();

	[[nodiscard]] Texture CreateTextureImpl();

	[[nodiscard]] Renderbuffer CreateRenderbufferImpl();

	// TODO: Make sure to update the cache when the parameters change. I.e. when resizing a
	// texture.
	// TODO: Store resource cached values here by GLuint key and erase them in the resource
	// deleter. e.g. std::unordered_map<GLuint, location_cache> location_caches_;
	// UnsafeDelete(); location_caches_.erase(id);

	// equivalent to GL_MAX_COLOR_ATTACHMENTS, or the number of color attachments a framebuffer can
	// have. This is set by the constructor and should not be modified afterward.
	GLuint max_color_attachments_{ 0 };

	IdMap<TextureCache> texture_cache_;
	IdMap<FramebufferCache> framebuffer_cache_;
	IdMap<RenderbufferCache> renderbuffer_cache_;
	// TODO: Consider splitting this up into separate buffers.
	IdMap<BufferCache> buffer_cache_;
	IdMap<VertexArrayCache> vertex_array_cache_;

	State bound_;

	SDL_GLContextState* context_{ nullptr };
};

constexpr GLenum ToGLType(BufferElementType type) noexcept {
	switch (type) {
		using enum BufferElementType;

		case Float:	 return GL_FLOAT;
		case Double: return GL_DOUBLE;
		case Int:	 return GL_INT;
		case UInt:	 return GL_UNSIGNED_INT;
		case Short:	 return GL_SHORT;
		case UShort: return GL_UNSIGNED_SHORT;
		case Byte:	 return GL_BYTE;
		case UByte:	 return GL_UNSIGNED_BYTE;
		case Bool:	 return GL_BOOL;
	}
	return GL_FLOAT;
}

template <typename T>
BindGuard<T>::~BindGuard() noexcept {
	if (restore_bind_) {
		auto _ = gl_.Bind(id_, false);
	}
}

} // namespace ptgn::impl::gl