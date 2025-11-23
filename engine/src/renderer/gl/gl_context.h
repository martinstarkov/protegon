#pragma once

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/concepts.h"
#include "core/util/id_map.h"
#include "core/util/hash.h"
#include "math/matrix4.h"
#include "math/tolerance.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/gl/buffer_layout.h"
#include "renderer/gl/gl.h"
#include "renderer/gl/gl_handle.h"
#include "renderer/gl/gl_resource.h"
#include "renderer/gl/gl_state.h"

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

namespace ptgn {

class Window;

} // namespace ptgn

namespace ptgn::impl::gl {

class GLContext;

struct ShaderTypeSource {
	GLuint type{ GL_FRAGMENT_SHADER };
	ShaderCode source;
	std::string name; // optional name for shader.
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
	) {
		return CreateBufferImpl<VertexBuffer>(
			GL_ARRAY_BUFFER, data, element_count, element_size, usage
		);
	}

	StrongGLHandle<ElementBuffer> CreateElementBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	) {
		return CreateBufferImpl<ElementBuffer>(
			GL_ELEMENT_ARRAY_BUFFER, data, element_count, element_size, usage
		);
	}

	StrongGLHandle<UniformBuffer> CreateUniformBuffer(
		const void* data, std::uint32_t size, GLenum usage
	) {
		return CreateBufferImpl<UniformBuffer>(GL_UNIFORM_BUFFER, data, size, 1, usage);
	}

	template <bool kRestoreBind = true>
	StrongGLHandle<Shader> CreateShader(
		GLuint vertex, GLuint fragment, const std::string& shader_name
	) {
		auto shader{ CreateShaderImpl(shader_name) };

		LinkShader(shader, vertex, fragment);

		return shader;
	}

	// String can be path to shader or the name of a pre-existing shader of the respective type.
	template <bool kRestoreBind = true>
	StrongGLHandle<Shader> CreateShader(
		std::variant<ShaderCode, std::string> vertex,
		std::variant<ShaderCode, std::string> fragment, const std::string& shader_name
	) {
		auto shader{ CreateShaderImpl(shader_name) };

		const auto has = [&](GLuint type) {
			auto hash{ Hash(shader_name) };
			switch (type) {
				case GL_FRAGMENT_SHADER: return fragment_shaders_.contains(hash);
				case GL_VERTEX_SHADER:	 return vertex_shaders_.contains(hash);
				default:				 PTGN_ERROR("Unknown shader type");
			}
		};

		const auto get = [&](GLuint type) {
			auto hash{ Hash(shader_name) };
			PTGN_ASSERT(has(type), "Could not find ", type, " shader with name: ", shader_name);
			switch (type) {
				case GL_FRAGMENT_SHADER: {
					return fragment_shaders_.find(hash)->second;
				}
				case GL_VERTEX_SHADER: {
					return vertex_shaders_.find(hash)->second;
				}
				default: PTGN_ERROR("Unknown shader type");
			}
		};

		auto max_texture_slots{ bound_.texture_units.size() };

		// @return { id, bool }: If true, delete shader id after.
		const auto get_id = [shader_name, max_texture_slots](
								const std::variant<ShaderCode, std::string>& v, GLuint type
							) -> std::pair<GLuint, bool> {
			if (std::holds_alternative<std::string>(v)) {
				const auto& name{ std::get<std::string>(v) };
				path file{ name };
				if (FileExists(file)) {
					PTGN_ASSERT(
						file.extension() == ".glsl",
						"Shader file extension must be .glsl: ", file.string()
					);
					return { CompileShaderPath(file, type, shader_name, max_texture_slots), true };
				} else if (has(type)) {
					return { get(type), false };
				} else {
					PTGN_ERROR(
						name, " is not a valid shader path or loaded ", type, " shader name"
					);
				}
			} else if (std::holds_alternative<ShaderCode>(v)) {
				const auto& src{ std::get<ShaderCode>(v) };
				return { CompileShaderSource(src.source, type, shader_name, max_texture_slots),
						 true };
			} else {
				PTGN_ERROR("Unknown variant type");
			}
		};

		auto [vertex_id, delete_vert_after]	  = get_id(vertex, GL_VERTEX_SHADER);
		auto [fragment_id, delete_frag_after] = get_id(fragment, GL_FRAGMENT_SHADER);

		LinkShader(shader, vertex_id, fragment_id);

		if (delete_vert_after && vertex_id) {
			GLCall(DeleteShader(vertex_id));
		}

		if (delete_frag_after && fragment_id) {
			GLCall(DeleteShader(fragment_id));
		}

		return shader;
	}

	template <bool kRestoreBind = true>
	StrongGLHandle<Shader> CreateShader(
		std::variant<ShaderCode, path> source, const std::string& shader_name
	) {
		auto shader{ CreateShaderImpl(shader_name) };

		std::string source_string;

		if (std::holds_alternative<path>(source)) {
			const auto& p{ std::get<path>(source) };
			source_string = FileToString(p);
		} else if (std::holds_alternative<ShaderCode>(source)) {
			const auto& src{ std::get<ShaderCode>(source) };
			source_string = src.source;
		} else {
			PTGN_ERROR("Unknown variant type");
		}

		const auto max_texture_slots{ bound_.texture_units.size() };

		auto srcs{ ParseShaderSourceFile(source_string, shader_name, max_texture_slots) };

		PTGN_ASSERT(
			srcs.size() == 2, "Shader file must provide a vertex and fragment type: ", shader_name
		);

		const auto& first{ srcs[0] };
		const auto& second{ srcs[1] };

		std::string vertex_source;
		std::string fragment_source;

		if (first.type == GL_VERTEX_SHADER && second.type == GL_FRAGMENT_SHADER) {
			vertex_source	= first.source.source;
			fragment_source = second.source.source;
		} else if (first.type == GL_FRAGMENT_SHADER && second.type == GL_VERTEX_SHADER) {
			fragment_source = first.source.source;
			vertex_source	= second.source.source;
		} else {
			PTGN_ERROR("Shader file must provide a vertex and fragment type: ", shader_name);
		}

		auto vertex_id{ CompileShaderFromSource(GL_VERTEX_SHADER, vertex_source) };
		auto fragment_id{ CompileShaderFromSource(GL_FRAGMENT_SHADER, fragment_source) };

		LinkShader(shader, vertex_id, fragment_id);

		if (vertex_id) {
			GLCall(DeleteShader(vertex_id));
		}

		if (fragment_id) {
			GLCall(DeleteShader(fragment_id));
		}

		return shader;
	}

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
			PTGN_ASSERT(slot < bound_.texture_units.size(), "Slot out of range of max slots");
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
			PTGN_ASSERT(bound_.active_texture_slot < bound_.texture_units.size());
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

	void AttachTexture(GLuint framebuffer, GLuint texture, GLenum texture_attachment) {
		PTGN_ASSERT(
			IsBound<FrameBuffer>(framebuffer),
			"Framebuffer must be bound before attaching a texture"
		);

		if (texture) {
			PTGN_ASSERT(texture_cache_.Has(texture), "Texture not in cache");
			PTGN_ASSERT(
				texture_cache_.Get(texture).size.BothAboveZero(),
				"Cannot attach a texture with no size"
			);
		}

		GLCall(FramebufferTexture2D(GL_FRAMEBUFFER, texture_attachment, GL_TEXTURE_2D, texture, 0));

		UpdateFrameBufferCache(framebuffer, texture, texture_attachment, GL_TEXTURE_2D);
	}

	void AttachRenderBuffer(
		GLuint framebuffer, GLuint renderbuffer, GLenum renderbuffer_attachment
	) {
		PTGN_ASSERT(
			IsBound<FrameBuffer>(framebuffer),
			"Framebuffer must be bound before attaching a renderbuffer"
		);

		if (renderbuffer) {
			PTGN_ASSERT(renderbuffer_cache_.Has(renderbuffer), "Renderbuffer not in cache");
			PTGN_ASSERT(
				renderbuffer_cache_.Get(renderbuffer).size.BothAboveZero(),
				"Cannot attach a renderbuffer with no size"
			);
		}

		GLCall(FramebufferRenderbuffer(
			GL_FRAMEBUFFER, renderbuffer_attachment, GL_RENDERBUFFER, renderbuffer
		));

		UpdateFrameBufferCache(framebuffer, renderbuffer, renderbuffer_attachment, GL_RENDERBUFFER);
	}

	void SetVertexBuffer(GLuint vertex_array, GLuint vertex_buffer) {
		PTGN_ASSERT(
			IsBound<VertexArray>(vertex_array),
			"Vertex array must be bound before setting vertex buffer"
		);

		auto _ = Bind<VertexBuffer, false>(vertex_buffer);
	}

	void SetElementBuffer(GLuint vertex_array, GLuint element_buffer) {
		PTGN_ASSERT(
			IsBound<VertexArray>(vertex_array),
			"Vertex array must be bound before setting element buffer"
		);

		auto _ = Bind<ElementBuffer, false>(element_buffer);
	}

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
					i, element.count, element.type, stride,
					reinterpret_cast<const void*>(element.offset)
				));
			} else {
				GLCall(VertexAttribPointer(
					i, element.count, element.type, element.normalized ? GL_TRUE : GL_FALSE, stride,
					reinterpret_cast<const void*>(element.offset)
				));
			}
		}

		vertex_array_cache_.Get(vertex_array).layout_set = true;
	}

	void EnableGammaCorrection() {
#ifndef __EMSCRIPTEN__
		GLCall(glEnable(GL_FRAMEBUFFER_SRGB));
#else
		PTGN_WARN("glEnable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
	}

	void DisableGammaCorrection() {
#ifndef __EMSCRIPTEN__
		GLCall(glDisable(GL_FRAMEBUFFER_SRGB));
#else
		PTGN_WARN("glDisable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
	}

	void SetDepthMask(GLboolean enabled) {
		if (bound_.depth.write == enabled) {
			return;
		}
		GLCall(glDepthMask(enabled));
		bound_.depth.write = enabled;
	}

	// Enabling blending will disable depth testing.
	void SetBlending(GLboolean enabled) {
		if (enabled) {
			SetDepthTesting(GL_FALSE);
		}
		if (bound_.blending == enabled) {
			return;
		}
		if (enabled) {
			GLCall(glEnable(GL_BLEND));
		} else {
			GLCall(glDisable(GL_BLEND));
		}
		bound_.blending = enabled;
	}

	void SetDepthFunc(GLenum depth_func) {
		if (bound_.depth.func == depth_func) {
			return;
		}
		GLCall(glDepthFunc(depth_func));
		bound_.depth.func = depth_func;
	}

	// Enabling depth testing will disable blending.
	void SetDepthTesting(GLboolean enabled) {
		if (enabled) {
			SetBlending(GL_FALSE);
		}
		if (bound_.depth.test == enabled) {
			return;
		}
		if (enabled) {
			GLCall(glClearDepth(1.0)); /* Enables Clearing Of The Depth Buffer */
			GLCall(glEnable(GL_DEPTH_TEST));
		} else {
			GLCall(glDisable(GL_DEPTH_TEST));
		}
		bound_.depth.test = enabled;
	}

	void SetDepthRange(float near_val, float far_val) {
		if (NearlyEqual(bound_.depth.range_near, near_val) &&
			NearlyEqual(bound_.depth.range_far, far_val)) {
			return;
		}
		GLCall(glDepthRange(near_val, far_val));
		bound_.depth.range_near = near_val;
		bound_.depth.range_far	= far_val;
	}

	void SetLineWidth(float width) {
		if (bound_.line_width == width) {
			return;
		}
		GLCall(glLineWidth(width));
		bound_.line_width = width;
	}

	void SetLineSmoothing(bool enabled) {
#ifndef __EMSCRIPTEN__
		if (enabled) {
			SetBlending(GL_TRUE);
			GLCall(glEnable(GL_LINE_SMOOTH));
		} else {
			GLCall(glDisable(GL_LINE_SMOOTH));
		}
#else
		PTGN_WARN("GL_LINE_SMOOTH not supported by Emscripten");
#endif
	}

	void SetPolygonMode(GLenum front_mode, GLenum back_mode) {
#ifndef __EMSCRIPTEN__
		if (bound_.polygon_mode_front == front_mode && bound_.polygon_mode_back == back_mode) {
			return;
		}

		if (front_mode == back_mode) {
			GLCall(glPolygonMode(GL_FRONT_AND_BACK, front_mode));
		} else {
			GLCall(glPolygonMode(GL_FRONT, front_mode));
			GLCall(glPolygonMode(GL_BACK, back_mode));
		}

		bound_.polygon_mode_front = front_mode;
		bound_.polygon_mode_back  = back_mode;
#else
		PTGN_WARN("glPolygonMode not supported by Emscripten");
#endif
	}

	// Will disable depth testing.
	void SetBlendMode(BlendMode mode) {
		SetBlending(GL_TRUE);

		if (bound_.blend_mode == mode) {
			return;
		}

		GLCall(BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD));

		switch (mode) {
			PTGN_IMPL_BLEND_CASE(
				Blend, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA
			)
			PTGN_IMPL_BLEND_CASE(
				PremultipliedBlend, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA
			)
			PTGN_IMPL_BLEND_CASE(ReplaceRGBA, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO)
			PTGN_IMPL_BLEND_CASE(ReplaceRGB, GL_ONE, GL_ZERO, GL_ZERO, GL_ONE)
			PTGN_IMPL_BLEND_CASE(ReplaceAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ZERO)
			PTGN_IMPL_BLEND_CASE(AddRGB, GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE)
			PTGN_IMPL_BLEND_CASE(AddRGBA, GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE)
			PTGN_IMPL_BLEND_CASE(AddAlpha, GL_ZERO, GL_ONE, GL_ONE, GL_ONE)
			PTGN_IMPL_BLEND_CASE(PremultipliedAddRGB, GL_ONE, GL_ONE, GL_ZERO, GL_ONE)
			PTGN_IMPL_BLEND_CASE(PremultipliedAddRGBA, GL_ONE, GL_ONE, GL_ONE, GL_ONE)
			PTGN_IMPL_BLEND_CASE(MultiplyRGB, GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE)
			PTGN_IMPL_BLEND_CASE(MultiplyRGBA, GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO)
			PTGN_IMPL_BLEND_CASE(MultiplyAlpha, GL_ZERO, GL_ONE, GL_DST_ALPHA, GL_ZERO)
			PTGN_IMPL_BLEND_CASE(
				MultiplyRGBWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE
			)
			PTGN_IMPL_BLEND_CASE(
				MultiplyRGBAWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
				GL_ZERO
			)
			default: PTGN_ERROR("Failed to identify blend mode");
		}

		bound_.blend_mode = mode;
	}

	void DrawElements(GLuint vertex_array, GLsizei element_count, GLenum primitive_mode) const {
		PTGN_ASSERT(
			IsBound<VertexArray>(vertex_array), "Vertex array must be bound before drawing elements"
		);
		PTGN_ASSERT(
			vertex_array_cache_.Get(vertex_array).layout_set,
			"Cannot draw vertex array without a valid vertex buffer layout"
		);
		PTGN_ASSERT(
			GetBound<ElementBuffer>(),
			"Cannot draw vertex array with uninitialized or destroyed element buffer"
		);

		constexpr GLenum element_type{ GL_UNSIGNED_BYTE };

		GLCall(glDrawElements(primitive_mode, element_count, element_type, nullptr));
	}

	void DrawArrays(GLuint vertex_array, GLsizei vertex_count, GLenum primitive_mode) const {
		PTGN_ASSERT(
			IsBound<VertexArray>(vertex_array), "Vertex array must be bound before drawing arrays"
		);
		PTGN_ASSERT(
			vertex_array_cache_.Get(vertex_array).layout_set,
			"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
		);

		constexpr GLint starting_index{ 0 };

		GLCall(glDrawArrays(primitive_mode, starting_index, vertex_count));
	}

	void SetViewport(const Viewport& viewport) {
		if (bound_.viewport == viewport) {
			return;
		}
		GLCall(
			glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y)
		);
		bound_.viewport = viewport;
	}

	[[nodiscard]] Viewport GetViewport() const {
		return bound_.viewport;
	}

	void SetClearColor(Color color) {
		if (bound_.clear_color == color) {
			return;
		}
		auto n{ static_cast<V4_float>(color) };
		GLCall(glClearColor(n.x, n.y, n.z, n.w));
		bound_.clear_color = color;
	}

	// Clears the currently bound framebuffer's buffers.
	void Clear() {
		// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	}

	// Clears the currently bound framebuffer's color buffer to the specified color.
	void ClearToColor(GLuint framebuffer, Color color) const {
		PTGN_ASSERT(
			IsBound<FrameBuffer>(framebuffer),
			"Framebuffer must be bound before clearing it to color"
		);
		// TODO: Update clear color state and add early exit if same.
		// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		auto c{ static_cast<V4_float>(color) };
		GLCall(ClearBufferfv(GL_COLOR_BUFFER_BIT, 0, c.Data()));
		/*
		// TODO: Check image format of bound texture and potentially use glClearBufferuiv
		instead of ClearBufferfv. GLCall(ClearBufferuiv(GL_COLOR, 0, color.Data()));
		*/
	}

	void SetColorMask(const ColorMaskState& mask) {
		if (bound_.color_mask == mask) {
			return;
		}
		GLCall(glColorMask(mask.red, mask.green, mask.blue, mask.alpha));
		bound_.color_mask = mask;
	}

	void SetScissor(const ScissorState& scissor) {
		if (bound_.scissor == scissor) {
			return;
		}

		if (scissor.enabled) {
			GLCall(glEnable(GL_SCISSOR_TEST));
			GLCall(glScissor(scissor.position.x, scissor.position.y, scissor.size.x, scissor.size.y)
			);
		} else {
			GLCall(glDisable(GL_SCISSOR_TEST));
		}

		bound_.scissor = scissor;
	}

	void SetCull(const CullState& cull) {
		if (bound_.cull == cull) {
			return;
		}

		if (cull.enabled) {
			GLCall(glEnable(GL_CULL_FACE));
		} else {
			GLCall(glDisable(GL_CULL_FACE));
		}

		GLCall(glCullFace(cull.face));
		GLCall(glFrontFace(cull.front));

		bound_.cull = cull;
	}

	void SetStencil(const StencilState& stencil) {
		if (bound_.stencil == stencil) {
			return;
		}

		if (stencil.enabled) {
			GLCall(glEnable(GL_STENCIL_TEST));
		} else {
			GLCall(glDisable(GL_STENCIL_TEST));
		}

		GLCall(glStencilFunc(stencil.func, stencil.ref, stencil.mask));
		GLCall(glStencilOp(stencil.fail_op, stencil.zfail_op, stencil.zpass_op));
		GLCall(glStencilMask(stencil.write_mask));

		bound_.stencil = stencil;
	}

	// Sets the uniform value for the specified uniform name. If the uniform does not exist in
	// the shader, nothing happens. Note: Make sure to bind the shader before setting uniforms.

	void SetUniform(GLuint shader, const char* uniform_name, V2_float v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform2f(location, v.x, v.y));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, V3_float v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform3f(location, v.x, v.y, v.z));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, V4_float v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, const Matrix4& matrix) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(UniformMatrix4fv(location, 1, GL_FALSE, matrix.Data()));
		}
	}

	void SetUniform(
		GLuint shader, const char* uniform_name, const std::int32_t* data, std::int32_t count
	) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform1iv(location, count, data));
		}
	}

	void SetUniform(
		GLuint shader, const char* uniform_name, const float* data, std::int32_t count
	) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform1fv(location, count, data));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, const Vector2<std::int32_t>& v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform2i(location, v.x, v.y));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, const Vector3<std::int32_t>& v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform3i(location, v.x, v.y, v.z));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, const Vector4<std::int32_t>& v) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, float v0) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform1f(location, v0));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, float v0, float v1) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform2f(location, v0, v1));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, float v0, float v1, float v2) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform3f(location, v0, v1, v2));
		}
	}

	void SetUniform(
		GLuint shader, const char* uniform_name, float v0, float v1, float v2, float v3
	) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform4f(location, v0, v1, v2, v3));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, std::int32_t v0) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform1i(location, v0));
		}
	}

	void SetUniform(GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform2i(location, v0, v1));
		}
	}

	void SetUniform(
		GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
	) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform3i(location, v0, v1, v2));
		}
	}

	void SetUniform(
		GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2,
		std::int32_t v3
	) {
		std::int32_t location{ GetUniform(shader, uniform_name) };
		if (location != -1) {
			GLCall(Uniform4i(location, v0, v1, v2, v3));
		}
	}

	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(GLuint shader, const char* uniform_name, bool value) {
		SetUniform(shader, uniform_name, static_cast<std::int32_t>(value));
	}

	[[nodiscard]] StrongGLHandle<Shader> GetShader(std::string_view shader_name) const {
		auto key{ Hash(shader_name) };
		PTGN_ASSERT(shaders_.contains(key));
		return shaders_.find(key)->second;
	}

	void SetActiveTextureSlot(GLuint slot) {
		if (bound_.active_texture_slot == slot) {
			return;
		}
		PTGN_ASSERT(
			slot < GetMaxTextureSlots(),
			"Attempting to bind a slot outside of OpenGL texture slot maximum"
		);
		GLCall(glActiveTexture(GL_TEXTURE0 + slot));
		bound_.active_texture_slot = slot;
	}

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

	[[nodiscard]] bool FrameBufferIsComplete(GLuint framebuffer) const {
		PTGN_ASSERT(
			IsBound<FrameBuffer>(framebuffer),
			"Cannot check status of framebuffer until it is bound"
		);
		auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
		return status == GL_FRAMEBUFFER_COMPLETE;
	}

	[[nodiscard]] const char* GetFrameBufferStatus() {
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
				return "Incomplete layer targets: Layered attachments are not all complete or "
					   "not "
					   "matching.";
			default: return "Unknown framebuffer status.";
		}
	}

	enum class AttachmentDataType {
		Color,
		Depth,
		Stencil,
		DepthStencil
	};

	AttachmentDataType GetAttachmentDataType(GLenum attachment) const {
		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + 8) {
			return AttachmentDataType::Color;
		}

		if (attachment == GL_DEPTH_ATTACHMENT) {
			return AttachmentDataType::Depth;
		}

		if (attachment == GL_STENCIL_ATTACHMENT) {
			return AttachmentDataType::Stencil;
		}

		if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
			return AttachmentDataType::DepthStencil;
		}

		PTGN_ERROR("Unsupported framebuffer attachment");
	}

	AttachmentInfo& GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment) {
		return const_cast<AttachmentInfo&>(
			std::as_const(*this).GetFrameBufferAttachment(framebuffer, attachment)
		);
	}

	const AttachmentInfo& GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment) const {
		const auto& cache = framebuffer_cache_.Get(framebuffer);

		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + 8) {
			auto idx{ attachment - GL_COLOR_ATTACHMENT0 };
			PTGN_ASSERT(idx > 0 && idx < cache.color.size(), "Color attachment out of valid range");
			return cache.color[idx];
		} else if (attachment == GL_DEPTH_ATTACHMENT) {
			return cache.depth;
		} else if (attachment == GL_STENCIL_ATTACHMENT) {
			return cache.stencil;
		} else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
			return cache.depth_stencil;
		} else {
			PTGN_ASSERT(false, "Unsupported framebuffer attachment enum");
		}
	}

	void UpdateFrameBufferCache(
		GLenum framebuffer, GLuint image_id, GLenum attachment, GLenum image_type
	) {
		PTGN_ASSERT(
			image_type == GL_TEXTURE_2D || image_type == GL_RENDERBUFFER, "Invalid image type"
		);

		auto& cache = framebuffer_cache_.Get(framebuffer);

		auto& info{ GetFrameBufferAttachment(framebuffer, attachment) };
		info.id	  = image_id;
		info.type = image_id ? image_type : 0;
	}

	[[nodiscard]] std::vector<ShaderTypeSource> ParseShaderSourceFile(
		const std::string& source, const std::string& name, std::size_t max_texture_slots
	) const;

	void PopulateShadersFromCache(const json& manifest);

	void CompileShaders(
		const std::vector<ShaderTypeSource>& sources,
		std::unordered_map<std::size_t, GLuint>& vertex_shaders,
		std::unordered_map<std::size_t, GLuint>& fragment_shaders
	) const;

	void PopulateShaderCache(
		const cmrc::embedded_filesystem& filesystem,
		std::unordered_map<std::size_t, GLuint>& vertex_shaders,
		std::unordered_map<std::size_t, GLuint>& fragment_shaders, std::size_t max_texture_slots
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

	[[nodiscard]] std::int32_t GetUniform(GLuint shader, const char* name) {
		PTGN_ASSERT(
			IsBound<Shader>(shader),
			"Cannot get uniform location of shader which is not currently bound"
		);

		auto& cache{ shader_cache_.Get(shader) };

		auto hash{ Hash(name) };

		if (auto location{ cache.uniform_locations.TryGet(hash) }) {
			return *location;
		}

		std::int32_t location{ GLCallReturn(GetUniformLocation(shader, name)) };

		cache.uniform_locations.Add(hash, location);

		return location;
	}

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

	// Color
	// Depth -> float
	// Stencil -? uint8_t
	// Depth+Stencil -> {float depth, uint8_t stencil}
	using PixelValue = std::variant<Color, float, std::uint8_t, std::pair<float, std::uint8_t>>;

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param coordinate Pixel coordinate from [0, size).
	PixelValue ReadPixel(
		GLuint framebuffer, V2_int coordinate, GLenum attachment = GL_COLOR_ATTACHMENT0
	) {
		auto _1 = Bind<FrameBuffer, true>(framebuffer);

		auto type  = GetAttachmentDataType(attachment);
		auto& info = GetFrameBufferAttachment(framebuffer, attachment);
		PTGN_ASSERT(info.id != 0, "No image attached to that attachment");

		V2_int size;
		if (info.type == GL_TEXTURE_2D) {
			size = texture_cache_.Get(info.id).size;
		} else {
			size = renderbuffer_cache_.Get(info.id).size;
		}

		PTGN_ASSERT(
			coordinate.x >= 0 && coordinate.x < size.x,
			"Cannot get pixel out of range of frame buffer size"
		);
		PTGN_ASSERT(
			coordinate.y >= 0 && coordinate.y < size.y,
			"Cannot get pixel out of range of frame buffer size"
		);

		int read_y = size.y - 1 - coordinate.y;

		if (type == AttachmentDataType::Color) {
			const auto& tex = texture_cache_.Get(info.id);

			int components = GetColorComponentCount(tex.internal_format);
			PTGN_ASSERT(components >= 3 && components <= 4);

			std::array<std::uint8_t, 4> v{ 0, 0, 0, 255 };

			GLCall(glReadPixels(
				coordinate.x, read_y, 1, 1, tex.internal_format, GL_UNSIGNED_BYTE, v.data()
			));

			return Color{ v[0], v[1], v[2],
						  components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
		}

		if (type == AttachmentDataType::Depth) {
			float depth = 0.0f;
			GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth));
			return depth;
		}

		if (type == AttachmentDataType::Stencil) {
			std::uint8_t stencil = 0;
			GLCall(glReadPixels(
				coordinate.x, read_y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil
			));
			return stencil;
		}

		if (type == AttachmentDataType::DepthStencil) {
			// GL_DEPTH_STENCIL returns two integers: depth + stencil packed.
			struct {
				std::uint32_t depth;
				std::uint8_t stencil;
			} ds{};

			GLCall(glReadPixels(
				coordinate.x, read_y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &ds
			));

			float depth = (ds.depth & 0xFFFFFF) / float(0xFFFFFF);
			return std::make_pair(depth, ds.stencil);
		}

		PTGN_ERROR("Unhandled attachment type");
	}

	// WARNING: This function is slow and should be primarily used for debugging framebuffers.
	// @param coordinate Pixel coordinate from [0, size).
	template <typename F>
	void ForEachPixel(
		GLuint framebuffer, F&& func /* takes in (V2_int, Color) */,
		GLenum attachment = GL_COLOR_ATTACHMENT0
	) {
		auto type  = GetAttachmentDataType(attachment);
		auto& info = GetFrameBufferAttachment(framebuffer, attachment);
		PTGN_ASSERT(info.id != 0);

		auto _1 = Bind<FrameBuffer, true>(framebuffer);

		// Determine buffer size
		V2_int size = (info.type == GL_TEXTURE_2D) ? texture_cache_.Get(info.id).size
												   : renderbuffer_cache_.Get(info.id).size;

		// Allocate max possible pixel size (requires different formats)
		std::vector<std::uint8_t> buffer(size.x * size.y * 4);

		GLenum format	 = GL_RGBA;
		GLenum type_enum = GL_UNSIGNED_BYTE;

		switch (type) {
			case AttachmentDataType::Color: {
				const auto& tex = texture_cache_.Get(info.id);
				PTGN_ASSERT(GetColorComponentCount(tex.internal_format) >= 3);
				format	  = tex.internal_format;
				type_enum = GL_UNSIGNED_BYTE;
				break;
			}
			case AttachmentDataType::Depth:
				format	  = GL_DEPTH_COMPONENT;
				type_enum = GL_FLOAT;
				break;
			case AttachmentDataType::Stencil:
				format	  = GL_STENCIL_INDEX;
				type_enum = GL_UNSIGNED_BYTE;
				break;
			case AttachmentDataType::DepthStencil:
				format	  = GL_DEPTH_STENCIL;
				type_enum = GL_UNSIGNED_INT_24_8;
				break;
		}

		GLCall(glReadPixels(0, 0, size.x, size.y, format, type_enum, buffer.data()));

		for (int y = 0; y < size.y; ++y) {
			int flipped = size.y - 1 - y;

			for (int x = 0; x < size.x; ++x) {
				PixelValue px;

				int idx = (flipped * size.x + x);

				switch (type) {
					case AttachmentDataType::Color: {
						std::uint8_t* p = &buffer[idx * 4];
						px				= Color{ p[0], p[1], p[2], p[3] };
						break;
					}

					case AttachmentDataType::Depth: {
						float* p = reinterpret_cast<float*>(buffer.data());
						px		 = p[idx];
						break;
					}

					case AttachmentDataType::Stencil: {
						std::uint8_t* p = buffer.data();
						px				= p[idx];
						break;
					}

					case AttachmentDataType::DepthStencil: {
						std::uint32_t* p	 = reinterpret_cast<std::uint32_t*>(buffer.data());
						std::uint32_t packed = p[idx];
						float depth			 = float(packed & 0xFFFFFF) / float(0xFFFFFF);
						std::uint8_t stencil = (packed >> 24) & 0xFF;
						px					 = std::make_pair(depth, stencil);
						break;
					}
				}

				func(V2_int{ x, y }, px);
			}
		}
	}

	void ResizeFrameBuffer(GLuint framebuffer, V2_int new_size) {
		auto& cache = framebuffer_cache_.Get(framebuffer);

		auto resize_attachment = [&](const AttachmentInfo& info) {
			if (info.id == 0) {
				return; // No attachment, skip resizing.
			}

			if (info.type == GL_TEXTURE_2D) {
				ResizeTexture(info.id, new_size);
			} else if (info.type == GL_RENDERBUFFER) {
				ResizeRenderBuffer(info.id, new_size);
			} else {
				PTGN_ASSERT(false, "Unknown framebuffer attachment type");
			}
		};

		for (auto& color : cache.color) {
			resize_attachment(color);
		}

		resize_attachment(cache.depth);
		resize_attachment(cache.stencil);
		resize_attachment(cache.depth_stencil);
	}

	void ResizeRenderBuffer(GLuint renderbuffer, V2_int new_size) {
		PTGN_ASSERT(renderbuffer);

		const auto& cache = renderbuffer_cache_.Get(renderbuffer);

		if (cache.size == new_size) {
			return;
		}

		auto _ = Bind<RenderBuffer, true>(renderbuffer);

		SetRenderBufferStorage(renderbuffer, new_size, cache.internal_format);
	}

	void ResizeTexture(GLuint texture, V2_int new_size) {
		PTGN_ASSERT(texture);

		const auto& cache = texture_cache_.Get(texture);

		if (cache.size == new_size) {
			return;
		}

		auto _ = Bind<Texture, true>(texture);

		SetTextureData(
			texture, nullptr, GL_RGBA, GL_UNSIGNED_BYTE, new_size, cache.internal_format
		);
	}

	void SetRenderBufferStorage(GLuint renderbuffer, V2_int size, GLenum internal_format) {
		PTGN_ASSERT(
			IsBound<RenderBuffer>(renderbuffer),
			"Renderbuffer must be bound prior to setting its storage"
		);

		GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

		auto& cache = renderbuffer_cache_.Get(renderbuffer);

		cache.size			  = size;
		cache.internal_format = internal_format;
	}

	void SetTextureData(
		GLuint texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int size, GLenum internal_format
	) {
		PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its data");

		constexpr GLint mipmap_level{ 0 };
		constexpr GLint border{ 0 };

#ifdef __EMSCRIPTEN__
		PTGN_ASSERT(
			pixel_data_format != GL_BGRA && pixel_data_format != GL_BGR &&
				internal_format != GL_BGRA && internal_format != GL_BGR,
			"OpenGL ES3.0 does not support BGR(A) formats in glTexImage2D"
		);
#endif

		GLCall(glTexImage2D(
			GL_TEXTURE_2D, mipmap_level, internal_format, size.x, size.y, border, pixel_data_format,
			pixel_data_type, pixel_data
		));

		auto& cache = texture_cache_.Get(texture);

		cache.size			  = size;
		cache.internal_format = internal_format;
	}

	void SetTextureSubData(
		GLuint texture, const void* pixel_subdata, GLenum pixel_data_format, GLenum pixel_data_type,
		V2_int subdata_size, V2_int subdata_offset
	) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to setting its subdata"
		);

		PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

		constexpr GLint mipmap_level{ 0 };

		GLCall(glTexSubImage2D(
			GL_TEXTURE_2D, mipmap_level, subdata_offset.x, subdata_offset.y, subdata_size.x,
			subdata_size.y, pixel_data_format, pixel_data_type, pixel_subdata
		));
	}

	void SetTextureClampBorderColor(GLuint texture, Color color) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture),
			"Texture must be bound prior to setting its clamp border color"
		);

		auto c{ static_cast<V4_float>(color) };
		SetTextureParameter(texture, GL_TEXTURE_BORDER_COLOR, c.Data());
	}

	void SetTextureParameter(GLuint texture, GLenum param, const GLfloat* values) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters"
		);
		PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
		GLCall(glTexParameterfv(GL_TEXTURE_2D, param, values));
	}

	void SetTextureParameter(GLuint texture, GLenum param, const GLint* values) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters"
		);
		PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
		GLCall(glTexParameteriv(GL_TEXTURE_2D, param, values));
	}

	void SetTextureParameter(GLuint texture, GLenum param, GLfloat value) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters"
		);
		PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
		GLCall(glTexParameterf(GL_TEXTURE_2D, param, value));
	}

	void SetTextureParameter(GLuint texture, GLenum param, GLint value) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters"
		);
		PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
		GLCall(glTexParameteri(GL_TEXTURE_2D, param, value));
	}

	[[nodiscard]] GLint GetTextureParameter(GLuint texture, GLenum param) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to getting its parameters"
		);
		GLint value{ -1 };
		GLCall(glGetTexParameteriv(GL_TEXTURE_2D, param, &value));
		PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
		return value;
	}

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] std::size_t GetMaxTextureSlots() const {
		return bound_.texture_units.size();
	}

	[[nodiscard]] GLuint GetActiveTextureSlot() const {
		return bound_.active_texture_slot;
	}

	template <typename T = GLint>
	T GetBufferParameter(GLenum target, GLenum pname) const {
		GLint value = -1;
		GLCall(GetBufferParameteriv(target, pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
		return static_cast<T>(value);
	}

	// Ensure that the texture scaling of the currently bound texture is valid for generating
	// mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(GLenum texture_min_filter) {
		return texture_min_filter == GL_LINEAR_MIPMAP_LINEAR ||
			   texture_min_filter == GL_LINEAR_MIPMAP_NEAREST ||
			   texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
			   texture_min_filter == GL_NEAREST_MIPMAP_NEAREST;
	}

	void GenerateMipmaps(GLuint texture) const {
		PTGN_ASSERT(
			IsBound<Texture>(texture), "Texture must be bound prior to generating mipmaps for it"
		);
#ifndef __EMSCRIPTEN__
		PTGN_ASSERT(
			SupportsMipmaps(GetTextureParameter(texture, GL_TEXTURE_MIN_FILTER)),
			"Set texture minifying scaling to mipmap type before generating mipmaps"
		);
#endif
		GLCall(GenerateMipmap(GL_TEXTURE_2D));
	}

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

	[[nodiscard]] StrongGLHandle<Shader> CreateShaderImpl(const std::string& shader_name) {
		auto id = new GLuint{ GLCallReturn(CreateProgram()) };
		PTGN_ASSERT(id && *id, "Failed to create shader");
		shader_cache_.Add(*id, ShaderCache{ .shader_name = shader_name });
		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(DeleteProgram(*id));
				shader_cache_.Remove(*id);
			}
			delete id;
		});
	}

	[[nodiscard]] StrongGLHandle<VertexArray> CreateVertexArrayImpl() {
		auto id = new GLuint{ 0 };
		GLCall(GenVertexArrays(1, id));
		PTGN_ASSERT(id && *id, "Failed to create vertex array");
		vertex_array_cache_.Add(*id, VertexArrayCache{});
		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(DeleteVertexArrays(1, id));
				vertex_array_cache_.Remove(*id);
			}
			delete id;
		});
	}

	[[nodiscard]] StrongGLHandle<FrameBuffer> CreateFrameBufferImpl() {
		auto id = new GLuint{ 0 };
		GLCall(GenFramebuffers(1, id));
		PTGN_ASSERT(id && *id, "Failed to create framebuffer");
		framebuffer_cache_.Add(*id, FrameBufferCache{});
		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(DeleteFramebuffers(1, id));
				framebuffer_cache_.Remove(*id);
			}
			delete id;
		});
	}

	[[nodiscard]] StrongGLHandle<Texture> CreateTextureImpl() {
		auto id = new GLuint{ 0 };
		GLCall(glGenTextures(1, id));
		PTGN_ASSERT(id && *id, "Failed to create texture");
		texture_cache_.Add(*id, TextureCache{});
		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(glDeleteTextures(1, id));
				texture_cache_.Remove(*id);
			}
			delete id;
		});
	}

	[[nodiscard]] StrongGLHandle<RenderBuffer> CreateRenderBufferImpl() {
		auto id = new GLuint{ 0 };
		GLCall(GenRenderbuffers(1, id));
		PTGN_ASSERT(id && *id, "Failed to create renderbuffer");
		renderbuffer_cache_.Add(*id, RenderBufferCache{});
		return std::shared_ptr<GLuint>(id, [this](GLuint* id) {
			if (id && *id) {
				GLCall(DeleteRenderbuffers(1, id));
				renderbuffer_cache_.Remove(*id);
			}
			delete id;
		});
	}

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

	void* context_{ nullptr };

	State bound_;

	std::unordered_map<std::size_t, GLuint> vertex_shaders_;
	std::unordered_map<std::size_t, GLuint> fragment_shaders_;
};

template <GLResource R>
BindGuard<R>::~BindGuard() noexcept {
	if (restore_bind_) {
		auto _ = gl_.Bind<R, false>(id_);
	}
}

} // namespace ptgn::impl::gl