#pragma once

#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/assert.h"
#include "core/log.h"
#include "core/util/concepts.h"
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

struct SDL_Window;

namespace ptgn::impl::gl {

class GLContext;

struct ShaderCache {
	std::unordered_map<std::size_t, GLuint> vertex_shaders;
	std::unordered_map<std::size_t, GLuint> fragment_shaders;
};

struct ShaderTypeSource {
	GLuint type{ GL_FRAGMENT_SHADER };
	ShaderCode source;
	std::string name; // optional name for shader.
};

template <Resource R, bool kRestoreBind>
class BindGuard {
public:
	explicit BindGuard(GLContext& gl, const Handle<R>& handle) : gl_{ gl }, handle_{ handle } {}

	~BindGuard() {
		if constexpr (kRestoreBind) {
			auto _ = gl_.Bind<false>(handle_);
		}
	}

	BindGuard(const BindGuard&)			   = delete;
	BindGuard& operator=(const BindGuard&) = delete;

private:
	GLContext& gl_;
	Handle<R> handle_;
};

class GLContext {
public:
	GLContext() = delete;
	explicit GLContext(SDL_Window* window);
	~GLContext() noexcept;
	GLContext(const GLContext&)				   = delete;
	GLContext(GLContext&&) noexcept			   = delete;
	GLContext& operator=(const GLContext&)	   = delete;
	GLContext& operator=(GLContext&&) noexcept = delete;

	Handle<VertexBuffer> CreateVertexBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	) {
		return CreateBuffer<VertexBuffer>(
			GL_ARRAY_BUFFER, data, element_count, element_size, usage
		);
	}

	Handle<ElementBuffer> CreateElementBuffer(
		const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
	) {
		return CreateBuffer<ElementBuffer>(
			GL_ELEMENT_ARRAY_BUFFER, data, element_count, element_size, usage
		);
	}

	Handle<UniformBuffer> CreateUniformBuffer(const void* data, std::uint32_t size, GLenum usage) {
		return CreateBuffer<UniformBuffer>(GL_UNIFORM_BUFFER, data, size, 1, usage);
	}

	template <bool kRestoreBind = true>
	Handle<Shader> CreateShader(GLuint vertex, GLuint fragment, const std::string& shader_name) {
		auto resource		  = MakeGLResource<Shader, ShaderResource>();
		resource->id		  = GLCallReturn(CreateProgram());
		resource->shader_name = shader_name;

		LinkShader(vertex, fragment);

		PTGN_ASSERT(resource->id != 0, "Failed to create shader");

		return Handle<Shader>(std::move(resource));
	}

	template <bool kRestoreBind = true>
	// String can be path to shader or the name of a pre-existing shader of the respective type.
	Handle<Shader> CreateShader(
		std::variant<ShaderCode, std::string> vertex,
		std::variant<ShaderCode, std::string> fragment, const std::string& shader_name
	) {
		auto resource		  = MakeGLResource<Shader, ShaderResource>();
		resource->id		  = GLCallReturn(CreateProgram());
		resource->shader_name = shader_name;

		const auto has = [&](GLuint type) {
			auto hash{ Hash(shader_name) };
			switch (type) {
				case GL_FRAGMENT_SHADER {
					return shader_cache_.fragment_shaders.contains(hash);
				};
					case GL_VERTEX_SHADER: {
					return shader_cache_.vertex_shaders.contains(hash);
				};
				default: PTGN_ERROR("Unknown shader type");
			}
		};

		const auto get = [&](GLuint type) {
			auto hash{ Hash(shader_name) };
			PTGN_ASSERT(has(type), "Could not find ", type, " shader with name: ", shader_name);
			switch (type) {
				case GL_FRAGMENT_SHADER: {
					auto it{ shader_cache_.fragment_shaders.find(hash) };
					return it->second;
				};
				case GL_VERTEX_SHADER: {
					auto it{ shader_cache_.vertex_shaders.find(hash) };
					return it->second;
				};
				default: PTGN_ERROR("Unknown shader type");
			}
		};

		// bool: If true, delete shader id after.
		const auto get_id = [shader_name](
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
					return { CompileShaderPath(file, type, shader_name), true };
				} else if (has(type)) {
					return { get(type), false };
				} else {
					PTGN_ERROR(
						name, " is not a valid shader path or loaded ", type, " shader name"
					);
				}
			} else if (std::holds_alternative<ShaderCode>(v)) {
				const auto& src{ std::get<ShaderCode>(v) };
				return { CompileShaderSource(src.source_, type, shader_name), true };
			} else {
				PTGN_ERROR("Unknown variant type");
			}
		};

		auto [vertex_id, delete_vert_after]	  = get_id(vertex, GL_VERTEX_SHADER);
		auto [fragment_id, delete_frag_after] = get_id(fragment, GL_FRAGMENT_SHADER);

		LinkShader(vertex_id, fragment_id);

		if (delete_vert_after && vertex_id) {
			GLCall(DeleteShader(vertex_id));
		}

		if (delete_frag_after && fragment_id) {
			GLCall(DeleteShader(fragment_id));
		}

		PTGN_ASSERT(resource->id != 0, "Failed to create shader");

		return Handle<Shader>(std::move(resource));
	}

	template <bool kRestoreBind = true>
	Handle<Shader> CreateShader(
		std::variant<ShaderCode, path> source, const std::string& shader_name
	) {
		auto resource		  = MakeGLResource<Shader, ShaderResource>();
		resource->id		  = GLCallReturn(CreateProgram());
		resource->shader_name = shader_name;

		std::string source_string;

		if (std::holds_alternative<path>(source)) {
			const auto& p{ std::get<path>(source) };
			source_string = FileToString(p);
		} else if (std::holds_alternative<ShaderCode>(source)) {
			const auto& src{ std::get<ShaderCode>(source) };
			source_string = src.source_;
		} else {
			PTGN_ERROR("Unknown variant type");
		}

		auto srcs{ impl::ShaderManager::ParseShaderSourceFile(source_string, shader_name) };

		PTGN_ASSERT(
			srcs.size() == 2, "Shader file must provide a vertex and fragment type: ", shader_name
		);

		const auto& first{ srcs[0] };
		const auto& second{ srcs[1] };

		std::string vertex_source;
		std::string fragment_source;

		if (first.type == ShaderType::Vertex && second.type == ShaderType::Fragment) {
			vertex_source	= first.source.source_;
			fragment_source = second.source.source_;
		} else if (first.type == ShaderType::Fragment && second.type == ShaderType::Vertex) {
			fragment_source = first.source.source_;
			vertex_source	= second.source.source_;
		} else {
			PTGN_ERROR("Shader file must provide a vertex and fragment type: ", shader_name);
		}

		ShaderId vertex_id{ Shader::Compile(ShaderType::Vertex, vertex_source) };
		ShaderId fragment_id{ Shader::Compile(ShaderType::Fragment, fragment_source) };

		Link(vertex_id, fragment_id);

		if (vertex_id) {
			GLCall(DeleteShader(vertex_id));
		}

		if (fragment_id) {
			GLCall(DeleteShader(fragment_id));
		}

		PTGN_ASSERT(resource->id != 0, "Failed to create shader");

		return Handle<Shader>(std::move(resource));
	}

	template <bool kRestoreBind = true>
	Handle<Texture> CreateTexture(
		const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
		const V2_int& size, GLenum internal_format

	) {
		auto resource = MakeGLResource<Texture, TextureResource>();
		GLCall(glGenTextures(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create texture");

		Handle<Texture> handle{ std::move(resource) };

		auto _ = Bind<kRestoreBind>(handle);

#ifdef __EMSCRIPTEN__
		PTGN_ASSERT(
			format != GL_BGRA && format != GL_BGR,
			"OpenGL ES3.0 does not support BGR(A) texture formats in glTexImage2D"
		);
#endif

		SetTextureData(
			handle, pixel_data, pixel_data_format, pixel_data_type, size, internal_format
		);

		SetTextureParameter(handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		SetTextureParameter(handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		SetTextureParameter(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		SetTextureParameter(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		return handle;
	}

	template <bool kRestoreBind = true>
	Handle<RenderBuffer> CreateRenderBuffer(const V2_int& size, GLenum internal_format) {
		auto resource = MakeGLResource<RenderBuffer, RenderBufferResource>();
		GLCall(GenRenderbuffers(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create render buffer");

		Handle<RenderBuffer> handle{ std::move(resource) };

		auto _ = Bind<kRestoreBind>(handle);

		SetRenderBufferStorage(handle, size, internal_format);

		return handle;
	}

	template <bool kRestoreBind = true>
	Handle<FrameBuffer> CreateFrameBuffer(const Handle<Texture>& texture) {
		PTGN_ASSERT(
			texture.Get().size.BothAboveZero(),
			"Cannot attach texture with no size to a frame buffer"
		);

		auto resource = MakeGLResource<FrameBuffer, FrameBufferResource>();
		GLCall(GenFramebuffers(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create framebuffer");

		resource->texture = texture;
		// Render buffer is implicitly as persistent as the frame buffer since the frame buffer
		// holds a reference to it.
		resource->render_buffer =
			CreateRenderBuffer<kRestoreBind>(texture.Get().size, GL_DEPTH24_STENCIL8);

		Handle<FrameBuffer> handle{ std::move(resource) };

		auto _ = Bind<kRestoreBind>(handle);

		GLCall(FramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->id, 0
		));

		GLCall(FramebufferRenderbuffer(
			GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, render_buffer->id
		));

		PTGN_ASSERT(FrameBufferIsComplete(handle));

		return handle;
	}

	template <bool kRestoreBind = true, typename... Ts>
	Handle<VertexArray> CreateVertexArray(
		const Handle<VertexBuffer>& vertex_buffer, const BufferLayout<Ts...>& vertex_buffer_layout,
		const Handle<ElementBuffer>& element_buffer
	) {
		auto resource = MakeGLResource<VertexArray, VertexArrayResource>();
		GLCall(GenVertexArrays(1, &resource->id));
		PTGN_ASSERT(resource->id, "Failed to create vertex array");

		resource->vertex_buffer	 = vertex_buffer;
		resource->element_buffer = element_buffer;

		Handle<VertexArray> handle{ std::move(resource) };

		auto _ = Bind<kRestoreBind>(handle);

		SetVertexBuffer(vertex_buffer);
		SetElementBuffer(element_buffer);
		SetBufferLayout(handle, vertex_buffer_layout);

		return handle;
	}

	template <bool kRestoreBind, Resource T>
	[[nodiscard]] std::optional<BindGuard<T, kRestoreBind>> Bind(const Handle<T>& handle) {
		Handle<T> previous{ GetBound<T>() };

		if (handle == previous) {
			return;
		}

		GLuint id{ handle ? handle.Get().id : 0 };

		if constexpr (T == VertexBuffer) {
			GLCall(BindBuffer(GL_ARRAY_BUFFER, id));
			bound_.vertex_array.Get().vertex_buffer = handle;
		} else if constexpr (T == ElementBuffer) {
			GLCall(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
			bound_.vertex_array.Get().element_buffer = handle;
		} else if constexpr (T == UniformBuffer) {
			GLCall(BindBuffer(GL_UNIFORM_BUFFER, id));
			bound_.uniform_buffer = handle;
		} else if constexpr (T == Shader) {
			GLCall(UseProgram(id));
			bound_.shader = handle;
		} else if constexpr (T == RenderBuffer) {
			GLCall(BindRenderbuffer(GL_RENDERBUFFER, id));
			bound_.render_buffer = handle;
		} else if constexpr (T == Texture) {
			auto slot{ GetActiveTextureSlot() };
			PTGN_ASSERT(slot < bound_.texture_units.size(), "Slot out of range of max slots");
			PTGN_ASSERT(bound_.texture_units[slot].texture != handle);
			GLCall(glBindTexture(GL_TEXTURE_2D, id));
			bound_.texture_units[slot].texture = handle;
		} else if constexpr (T == FrameBuffer) {
			GLCall(BindFramebuffer(GL_FRAMEBUFFER, id));
			bound_.frame_buffer = handle;
		} else if constexpr (T == VertexArray) {
#ifdef PTGN_PLATFORM_MACOS
			// MacOS complains about binding 0 id vertex array.
			if (id != 0) {
				GLCall(BindVertexArray(id));
			}
#else
			GLCall(BindVertexArray(id));
#endif
			bound_.vertex_array = handle;
		} else {
			static_assert(false, "Unsupported Resource type");
		}

		if constexpr (kRestoreBind) {
			return BindGuard<T, kRestoreBind>{ *this, previous };
		} else {
			return std::nullopt;
		}
	}

	template <Resource T>
	[[nodiscard]] const Handle<T>& GetBound() const {
		if constexpr (T == VertexBuffer) {
			return bound_.vertex_array.Get().vertex_buffer;
		} else if constexpr (T == ElementBuffer) {
			return bound_.vertex_array.Get().element_buffer;
		} else if constexpr (T == UniformBuffer) {
			return bound_.uniform_buffer;
		} else if constexpr (T == Texture) {
			PTGN_ASSERT(bound_.active_texture_slot < bound_.textures.size());
			return bound_.textures[bound_.active_texture_slot].texture;
		} else if constexpr (T == RenderBuffer) {
			return bound_.render_buffer;
		} else if constexpr (T == FrameBuffer) {
			return bound_.frame_buffer;
		} else if constexpr (T == VertexArray) {
			return bound_.vertex_array;
		} else if constexpr (T == Shader) {
			return bound_.shader;
		} else {
			static_assert(false, "Unsupported Resource type");
		}
	}

	template <Resource T>
	[[nodiscard]] bool IsBound(const Handle<T>& handle) const {
		return GetBound<T>() == handle;
	}

	void SetVertexBuffer(
		Handle<VertexArray>& vertex_array, const Handle<VertexBuffer>& vertex_buffer
	) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting vertex buffer"
		);
		vertex_array.Get().vertex_buffer = vertex_buffer;

		auto _ = Bind<false>(vertex_buffer);
	}

	void SetElementBuffer(
		Handle<VertexArray>& vertex_array, const Handle<ElementBuffer>& element_buffer
	) {
		PTGN_ASSERT(
			IsBound(vertex_array), "Vertex array must be bound before setting element buffer"
		);
		vertex_array.Get().element_buffer = element_buffer;

		auto _ = Bind<false>(element_buffer);
	}

	template <VertexDataType... Ts>
		requires NonEmptyPack<Ts...>
	void SetBufferLayout(const Handle<VertexArray>& vertex_array, const BufferLayout<Ts...>& layout)
		const {
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

	void DrawElements(
		const Handle<VertexArray>& vertex_array, GLsizei element_count, GLenum primitive_mode
	) const {
		PTGN_ASSERT(IsBound(vertex_array), "Vertex array must be bound before drawing elements");
		PTGN_ASSERT(
			vertex_array.Get().vertex_buffer,
			"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
		);
		PTGN_ASSERT(
			vertex_array.Get().element_buffer,
			"Cannot draw vertex array with uninitialized or destroyed element buffer"
		);

		constexpr GLenum element_type{ GL_UNSIGNED_BYTE };

		GLCall(glDrawElements(primitive_mode, element_count, element_type, nullptr));
	}

	void DrawArrays(
		const Handle<VertexArray>& vertex_array, GLsizei vertex_count, GLenum primitive_mode
	) const {
		PTGN_ASSERT(IsBound(vertex_array), "Vertex array must be bound before drawing arrays");
		PTGN_ASSERT(
			vertex_array.Get().vertex_buffer,
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

	void SetClearColor(const Color& color) {
		if (bound_.clear_color == color) {
			return;
		}
		auto n{ static_cast<V4_float>(color) };
		GLCall(glClearColor(n.x, n.y, n.z, n.w));
		bound_.clear_color = color;
	}

	// Clears the currently bound frame buffer's buffers.
	void Clear() {
		// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	}

	// Clears the currently bound frame buffer's color buffer to the specified color.
	void ClearToColor(const Handle<FrameBuffer>& frame_buffer, const Color& color) const {
		PTGN_ASSERT(
			IsBound(frame_buffer), "Frame buffer must be bound before clearing it to color"
		);
		// TODO: Update clear color state and add early exit if same.
		// GLCall(glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		auto c{ static_cast<V4_float>(color) };
		GLCall(ClearBufferfv(GL_COLOR_BUFFER_BIT, 0, c.Data()));
		/*
		// TODO: Check image format of bound texture and potentially use glClearBufferuiv instead of
		ClearBufferfv.
		GLCall(ClearBufferuiv(GL_COLOR, 0, color.Data()));
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

	// Sets the uniform value for the specified uniform name. If the uniform does not exist in the
	// shader, nothing happens.
	// Note: Make sure to bind the shader before setting uniforms.

	void SetUniform(const Handle<Shader>& handle, const std::string& name, const Vector2<float>& v)
		const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform2f(location, v.x, v.y));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, const Vector3<float>& v)
		const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform3f(location, v.x, v.y, v.z));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, const Vector4<float>& v)
		const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, const Matrix4& matrix)
		const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(UniformMatrix4fv(location, 1, GL_FALSE, matrix.Data()));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, const std::int32_t* data,
		std::int32_t count
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform1iv(location, count, data));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, const float* data, std::int32_t count
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform1fv(location, count, data));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, const Vector2<std::int32_t>& v
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform2i(location, v.x, v.y));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, const Vector3<std::int32_t>& v
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform3i(location, v.x, v.y, v.z));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, const Vector4<std::int32_t>& v
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, float v0) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform1f(location, v0));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, float v0, float v1)
		const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform2f(location, v0, v1));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, float v0, float v1, float v2
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform3f(location, v0, v1, v2));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, float v0, float v1, float v2,
		float v3
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform4f(location, v0, v1, v2, v3));
		}
	}

	void SetUniform(const Handle<Shader>& handle, const std::string& name, std::int32_t v0) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform1i(location, v0));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, std::int32_t v0, std::int32_t v1
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform2i(location, v0, v1));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, std::int32_t v0, std::int32_t v1,
		std::int32_t v2
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform3i(location, v0, v1, v2));
		}
	}

	void SetUniform(
		const Handle<Shader>& handle, const std::string& name, std::int32_t v0, std::int32_t v1,
		std::int32_t v2, std::int32_t v3
	) const {
		std::int32_t location{ GetUniform(handle, name) };
		if (location != -1) {
			GLCall(Uniform4i(location, v0, v1, v2, v3));
		}
	}

	// Behaves identically to SetUniform(name, std::int32_t).
	void SetUniform(const Handle<Shader>& handle, const std::string& name, bool value) const {
		SetUniform(handle, name, static_cast<std::int32_t>(value));
	}

private:
	static void LoadGLFunctions();

	[[nodiscard]] std::int32_t GetUniform(const Handle<Shader>& handle, const std::string& name)
		const {
		PTGN_ASSERT(
			IsBound(handle), "Cannot get uniform location of shader which is not currently bound"
		);

		const auto& resource{ handle.Get() };

		if (auto it{ resource.location_cache.find(name) }; it != resource.location_cache.end()) {
			return it->second;
		}

		std::int32_t location{ GLCallReturn(GetUniformLocation(resource.id, name.c_str())) };

		resource.location_cache.try_emplace(name, location);

		return location;
	}

	template <Resource T>
		requires(T == VertexBuffer || T == ElementBuffer || T == UniformBuffer)
	Handle<T> CreateBuffer(
		GLenum target, const void* data, std::uint32_t element_count, std::uint32_t element_size,
		GLenum usage
	) {
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		auto resource = MakeGLResource<T, BufferResource>();
		GLCall(glGenBuffers(1, &resource->id));
		PTGN_ASSERT(resource->id != 0, "Failed to create buffer resource");

		resource->usage = usage;
		resource->count = element_count;

		const std::uint32_t size = element_count * element_size;

		Handle<T> handle{ std::move(resource) };

		auto _1 = Bind<true>(Handle<VertexArray>{});
		auto _2 = Bind<false>(handle);

		GLCall(BufferData(target, size, data, usage));

		return handle;
	}

	template <typename T = GLint>
	T GetInteger(GLenum pname) {
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
	template <bool kRestoreBind>
	[[nodiscard]] Color GetFrameBufferPixel(
		const Handle<FrameBuffer>& handle, const V2_int& coordinate
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
		auto _1 = Bind<kRestoreBind>(handle.Get().texture);
		auto components{ GetColorComponentCount(handle.Get().texture.Get().internal_format) };
		PTGN_ASSERT(
			components >= 3,
			"Textures with less than 3 pixel components cannot currently be queried"
		);
		std::vector<std::uint8_t> v(static_cast<std::size_t>(components * 1 * 1));
		int y{ size.y - 1 - coordinate.y };
		PTGN_ASSERT(y >= 0);
		auto _2 = Bind<kRestoreBind>(handle);
		GLCall(glReadPixels(
			coordinate.x, y, 1, 1, handle.Get().texture.Get().pixel_format, GL_UNSIGNED_BYTE,
			static_cast<void*>(v.data())
		));
		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}

	// WARNING: This function is slow and should be
	// primarily used for debugging frame buffers.
	// @param callback Function to be called for each pixel.
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture
	// ids. Note: Only RGB/RGBA format textures supported.
	template <bool kRestoreBind>
	void ForEachFrameBufferPixel(
		const Handle<FrameBuffer>& handle, const std::function<void(V2_int, Color)>& func
	) {
		// TODO: Allow reading pixels from stencil or depth buffers.

		V2_int size{ handle.Get().texture.Get().size };

		auto _1 = Bind<kRestoreBind>(handle.Get().texture);
		auto components{ GetColorComponentCount(handle.Get().texture.Get().internal_format) };
		PTGN_ASSERT(
			components >= 3,
			"Textures with less than 3 pixel components cannot currently be queried"
		);

		std::vector<std::uint8_t> v(static_cast<std::size_t>(components * size.x * size.y));
		auto _2 = Bind<kRestoreBind>(handle);
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

	[[nodiscard]] bool FrameBufferIsComplete(const Handle<FrameBuffer>& handle) const {
		PTGN_ASSERT(IsBound(handle), "Cannot check status of frame buffer until it is bound");
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
				return "Incomplete layer targets: Layered attachments are not all complete or not "
					   "matching.";
			default: return "Unknown framebuffer status.";
		}
	}

	template <bool kRestoreBind>
	void Resize(Handle<FrameBuffer>& handle, const V2_int& new_size) {
		Resize<kRestoreBind>(handle.Get().texture, new_size);
		Resize<kRestoreBind>(handle.Get().render_buffer, new_size);
	}

	template <bool kRestoreBind>
	void Resize(Handle<RenderBuffer>& handle, const V2_int& new_size) {
		if (handle && handle.Get().size == new_size) {
			return;
		}

		auto _ = Bind<kRestoreBind>(handle);

		SetRenderBufferStorage(handle, new_size, handle.Get().internal_format);
	}

	template <bool kRestoreBind>
	void Resize(Handle<Texture>& handle, const V2_int& new_size) {
		if (handle && handle.Get().size == new_size) {
			return;
		}

		auto _ = Bind<kRestoreBind>(handle);

		SetTextureData(
			handle, nullptr, GL_RGBA, GL_UNSIGNED_BYTE, new_size, handle.Get().internal_format
		);
	}

	void SetRenderBufferStorage(
		Handle<RenderBuffer>& handle, const V2_int& size, GLenum internal_format
	) const {
		PTGN_ASSERT(IsBound(handle), "Render buffer must be bound prior to setting its storage");

		GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

		auto& resource{ handle.Get() };
		resource.size			 = size;
		resource.internal_format = internal_format;
	}

	template <typename T = GLint>
	T GetBufferParameter(GLenum target, GLenum pname) {
		GLint value = -1;
		GLCall(GetBufferParameteriv(target, pname, &value));
		PTGN_ASSERT(value >= 0, "Failed to query buffer parameter");
		return static_cast<T>(value);
	}

	void SetActiveTexture(GLuint slot) {
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

	// @return The maximum number of texture slots available on the current hardware.
	[[nodiscard]] GLuint GetMaxTextureSlots() const {
		return bound_.texture_units.size();
	}

	[[nodiscard]] GLuint GetActiveSlot() const {
		return bound_.active_texture_slot;
	}

	void SetTextureData(
		Handle<Texture>& handle, const void* pixel_data, GLenum pixel_data_format,
		GLenum pixel_data_type, const V2_int& size, GLenum internal_format
	) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its data");

		constexpr GLint mipmap_level{ 0 };
		constexpr GLint border{ 0 };

		GLCall(glTexImage2D(
			GL_TEXTURE_2D, mipmap_level, internal_format, size.x, size.y, border, pixel_data_format,
			pixel_data_type, pixel_data
		));

		auto& resource{ handle.Get() };
		resource.size			 = size;
		resource.internal_format = internal_format;
	}

	void SetTextureSubData(
		const Handle<Texture>& handle, const void* pixel_subdata, GLenum pixel_data_format,
		GLenum pixel_data_type, const V2_int& subdata_size, const V2_int& subdata_offset
	) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its subdata");

		PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

		constexpr GLint mipmap_level{ 0 };

		GLCall(glTexSubImage2D(
			GL_TEXTURE_2D, mipmap_level, subdata_offset.x, subdata_offset.y, subdata_size.x,
			subdata_size.y, pixel_data_format, pixel_data_type, pixel_subdata
		));
	}

	void SetTextureClampBorderColor(const Handle<Texture>& handle, const Color& color) const {
		PTGN_ASSERT(
			IsBound(handle), "Texture must be bound prior to setting its clamp border color"
		);

		auto c{ static_cast<V4_float>(color) };
		SetTextureParameter(handle, GL_TEXTURE_BORDER_COLOR, c.Data());
	}

	void SetTextureParameter(const Handle<Texture>& handle, GLenum param, const GLfloat* values)
		const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its parameters");
		PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
		GLCall(glTexParameterfv(GL_TEXTURE_2D, param, values));
	}

	void SetTextureParameter(const Handle<Texture>& handle, GLenum param, const GLint* values)
		const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its parameters");
		PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
		GLCall(glTexParameteriv(GL_TEXTURE_2D, param, values));
	}

	void SetTextureParameter(const Handle<Texture>& handle, GLenum param, GLfloat value) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its parameters");
		PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
		GLCall(glTexParameterf(GL_TEXTURE_2D, param, value));
	}

	void SetTextureParameter(const Handle<Texture>& handle, GLenum param, GLint value) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to setting its parameters");
		PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
		GLCall(glTexParameteri(GL_TEXTURE_2D, param, value));
	}

	[[nodiscard]] GLint GetTextureParameter(const Handle<Texture>& handle, GLenum param) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to getting its parameters");
		GLint value{ -1 };
		GLCall(glGetTexParameteriv(GL_TEXTURE_2D, param, &value));
		PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
		return value;
	}

	// Ensure that the texture scaling of the currently bound texture is valid for generating
	// mipmaps.
	[[nodiscard]] static bool SupportsMipmaps(GLenum texture_min_filter) {
		return texture_min_filter == GL_LINEAR_MIPMAP_LINEAR ||
			   texture_min_filter == GL_LINEAR_MIPMAP_NEAREST ||
			   texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
			   texture_min_filter == GL_NEAREST_MIPMAP_NEAREST;
	}

	void GenerateMipmaps(const Handle<Texture>& handle) const {
		PTGN_ASSERT(IsBound(handle), "Texture must be bound prior to generating mipmaps for it");
#ifndef __EMSCRIPTEN__
		PTGN_ASSERT(
			SupportsMipmaps(GetTextureParameter(handle, GL_TEXTURE_MIN_FILTER)),
			"Set texture minifying scaling to mipmap type before generating mipmaps"
		);
#endif
		GLCall(GenerateMipmap(GL_TEXTURE_2D));
	}

	template <Resource T, bool kBufferOrphaning = true>
		requires(T == VertexBuffer || T == ElementBuffer || T == UniformBuffer)
	void SetBufferSubData(
		const Handle<T>& handle, GLenum target, const void* data, std::int32_t byte_offset,
		std::uint32_t element_count, std::uint32_t element_size
	) const {
		PTGN_ASSERT(IsBound(handle), "Buffer must be bound before setting its subdata");
		PTGN_ASSERT(element_count > 0, "Number of buffer elements must be greater than 0");
		PTGN_ASSERT(element_size > 0, "Byte size of a buffer element must be greater than 0");

		PTGN_ASSERT(data != nullptr);

		std::uint32_t size{ element_count * element_size };

		// This buffer size check must be done after the buffer is bound.
		PTGN_ASSERT(
			size <= GetBufferParameter(GL_ARRAY_BUFFER, GL_BUFFER_SIZE),
			"Attempting to bind data outside of allocated buffer size"
		);

		auto usage{ handle.Get().usage };
		auto count{ handle.Get().count };

		if constexpr (kBufferOrphaning) {
			if (usage == GL_DYNAMIC_DRAW || usage == GL_STREAM_DRAW) {
				std::uint32_t buffer_size{ count * element_size };
				PTGN_ASSERT(
					buffer_size <= GetBufferParameter(GL_ARRAY_BUFFER, GL_BUFFER_SIZE),
					"Buffer element size does not appear to match the "
					"originally allocated buffer element size"
				);
				GLCall(BufferData(target, buffer_size, nullptr, usage));
			}
		}

		GLCall(BufferSubData(target, byte_offset, size, data));
	}

	template <Resource T>
	constexpr void UnsafeDeleteId(GLuint id) {
		if (id == 0) {
			return;
		}

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
			static_assert(false, "Unsupported Resource type in DeleteId()");
		}
	}

	template <Resource T, typename ResourceType>
	[[nodiscard]] std::shared_ptr<ResourceType> MakeGLResource() {
		return std::shared_ptr<ResourceType>(new ResourceType{}, [](ResourceType* res) {
			if (res && res->id) {
				UnsafeDeleteId<T>(res->id);
			}
			delete res;
		});
	}

	std::unordered_map<std::size_t, ShaderResource> shaders_;

	void* context_{ nullptr };

	State bound_;

	ShaderCache shader_cache_;
};

} // namespace ptgn::impl::gl