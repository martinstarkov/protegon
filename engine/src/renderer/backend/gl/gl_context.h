#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "core/util/id_map.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
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

	Buffers buffers;
	Shaders shaders;
	Textures textures;

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

	void SetRenderbufferStorage(Renderbuffer renderbuffer, V2_int size, GLenum internal_format);

	[[nodiscard]] std::uint32_t GetActiveTextureSlot() const;

	[[nodiscard]] VertexArray CreateVertexArrayImpl();

	[[nodiscard]] Framebuffer CreateFramebufferImpl();

	[[nodiscard]] Renderbuffer CreateRenderbufferImpl();

	int GetInteger(GLenum pname) const;

	// TODO: Make sure to update the cache when the parameters change. I.e. when resizing a
	// texture.
	// TODO: Store resource cached values here by GLuint key and erase them in the resource
	// deleter. e.g. std::unordered_map<GLuint, location_cache> location_caches_;
	// UnsafeDelete(); location_caches_.erase(id);

	// equivalent to GL_MAX_COLOR_ATTACHMENTS, or the number of color attachments a framebuffer can
	// have. This is set by the constructor and should not be modified afterward.
	std::uint32_t max_color_attachments_{ 0 };

	IdMap<FramebufferCache> framebuffer_cache_;
	IdMap<RenderbufferCache> renderbuffer_cache_;
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