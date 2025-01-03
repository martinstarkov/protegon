#include "renderer/shader.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <string>
#include <utility>

#include "core/game.h"
#include "core/window.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

std::string_view GetShaderTypeName(std::uint32_t type) {
	switch (type) {
		case GL_VERTEX_SHADER:	 return "vertex";
		case GL_FRAGMENT_SHADER: return "fragment";
		// case GL_COMPUTE_SHADER:			return "compute";
		// case GL_GEOMETRY_SHADER:		return "geometry";
		// case GL_TESS_CONTROL_SHADER:	return "tess control";
		// case GL_TESS_EVALUATION_SHADER: return "tess evaluation";
		default:				 return "invalid";
	}
}

ShaderInstance::ShaderInstance() {
	id_ = GLCallReturn(gl::CreateProgram());
	PTGN_ASSERT(id_ != 0, "Failed to create shader program using OpenGL context");
}

ShaderInstance::~ShaderInstance() {
	GLCall(gl::DeleteProgram(id_));
}

} // namespace impl

Shader::Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader) {
	CompileProgram(vertex_shader.source_, fragment_shader.source_);
}

Shader::Shader(const path& vertex_shader_path, const path& fragment_shader_path) {
	PTGN_ASSERT(
		FileExists(vertex_shader_path),
		"Cannot create shader from nonexistent vertex shader path: ", vertex_shader_path.string()
	);
	PTGN_ASSERT(
		FileExists(fragment_shader_path),
		"Cannot create shader from nonexistent fragment shader path: ",
		fragment_shader_path.string()
	);
	CompileProgram(FileToString(vertex_shader_path), FileToString(fragment_shader_path));
}

std::uint32_t Shader::CompileShader(std::uint32_t type, const std::string& source) {
	std::uint32_t id = GLCallReturn(gl::CreateShader(type));

	auto src{ source.c_str() };

#ifdef PTGN_PLATFORM_MACOS
	GLCall(gl::glShaderSource(id, 1, &src, nullptr));
	GLCall(gl::glCompileShader(id));
#else
	GLCall(gl::ShaderSource(id, 1, &src, nullptr));
	GLCall(gl::CompileShader(id));
#endif

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	GLCall(gl::GetShaderiv(id, GL_COMPILE_STATUS, &result));

	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(gl::GetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(length);
		GLCall(gl::GetShaderInfoLog(id, length, &length, &log[0]));

		GLCall(gl::DeleteShader(id));

		PTGN_ERROR(
			"Failed to compile ", impl::GetShaderTypeName(type), " shader: \n", source, "\n", log
		);
	}

	return id;
}

void Shader::CompileProgram(const std::string& vertex_source, const std::string& fragment_source) {
	bool was_valid{ IsValid() };
	if (!was_valid) {
		Create();
	}

	auto& s{ Get() };

	if (was_valid) {
		s.location_cache_.clear();
	}

	std::uint32_t vertex   = CompileShader(GL_VERTEX_SHADER, vertex_source);
	std::uint32_t fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);

	if (vertex && fragment) {
		GLCall(gl::AttachShader(s.id_, vertex));
		GLCall(gl::AttachShader(s.id_, fragment));
		GLCall(gl::LinkProgram(s.id_));

		// Check for shader link errors.
		std::int32_t linked = GL_FALSE;
		GLCall(gl::GetProgramiv(s.id_, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(gl::GetProgramiv(s.id_, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(length);
			GLCall(gl::GetProgramInfoLog(s.id_, length, &length, &log[0]));

			GLCall(gl::DeleteProgram(s.id_));

			GLCall(gl::DeleteShader(vertex));
			GLCall(gl::DeleteShader(fragment));

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(gl::ValidateProgram(s.id_));
	}

	if (vertex) {
		GLCall(gl::DeleteShader(vertex));
	}

	if (fragment) {
		GLCall(gl::DeleteShader(fragment));
	}
}

void Shader::Bind() const {
	GLCall(gl::UseProgram(Get().id_));
#ifdef PTGN_DEBUG
	++game.stats.shader_binds;
#endif
}

void Shader::Draw(
	const Texture& texture, const Rect& destination, const M4_float& view_projection,
	const TextureInfo& texture_info
) const {
	// TODO: Fix.
	// PTGN_ASSERT(texture.IsValid(), "Cannot draw shader with invalid texture");

	// Rect dest{ destination };

	// if (dest.IsZero()) {
	//	dest = Rect::Fullscreen();
	// } else if (dest.size.IsZero()) {
	//	dest.size = texture.GetSize();
	// }

	// auto tex_coords{ impl::RenderData::GetTextureCoordinates(
	//	texture_info.source_position, texture_info.source_size, dest.size, texture_info.flip
	//) };

	//// Since this engine uses top left as origin, shaders must all be flipped vertically.
	// impl::RenderData::FlipTextureCoordinates(tex_coords, Flip::Vertical);

	// Bind();
	// SetUniform("u_ViewProjection", view_projection);
	// SetUniform("u_Texture", 1);
	// SetUniform("u_Resolution", V2_float{ game.window.GetSize() });
	// texture.Bind(1);

	// VertexArray vertex_array{ impl::TextureVertices(
	//							  dest.GetVertices(texture_info.rotation_center), tex_coords, 0.0f,
	//							  texture_info.tint.Normalized()
	//						  ),
	//						  game.renderer.shader_ib_ };

	// vertex_array.Draw();
}

std::int32_t Shader::GetUniformLocation(const std::string& name) const {
	PTGN_ASSERT(IsValid(), "Cannot get uniform location of invalid shader");
	auto& s{ Get() };
	PTGN_ASSERT(
		GetBoundId() == static_cast<std::int32_t>(s.id_),
		"Cannot get uniform location of shader which is not currently bound"
	);
	auto& location_cache{ s.location_cache_ };
	if (auto it = location_cache.find(name); it != location_cache.end()) {
		return it->second;
	}

#ifdef PTGN_PLATFORM_MACOS
	std::int32_t location = GLCallReturn(gl::glGetUniformLocation(s.id_, name.c_str()));
#else
	std::int32_t location = GLCallReturn(gl::GetUniformLocation(s.id_, name.c_str()));
#endif

	location_cache.emplace(name, location);
	return location;
}

void Shader::SetUniform(const std::string& name, const Vector2<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform2f(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform3f(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform4f(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, const Matrix4<float>& m) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::UniformMatrix4fv(location, 1, GL_FALSE, m.Data()));
	}
}

void Shader::SetUniform(const std::string& name, const std::int32_t* data, std::size_t count)
	const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform1iv(location, static_cast<std::uint32_t>(count), data));
	}
}

void Shader::SetUniform(const std::string& name, const float* data, std::size_t count) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform1fv(location, static_cast<std::uint32_t>(count), data));
	}
}

void Shader::SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform2i(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform3i(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform4i(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, float v0) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform1f(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform2f(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform3f(location, v0, v1, v2));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform4f(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform1i(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform2i(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
	const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform3i(location, v0, v1, v2));
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		GLCall(gl::Uniform4i(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, bool value) const {
	SetUniform(name, static_cast<std::int32_t>(value));
}

std::int32_t Shader::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(gl::glGetIntegerv(GL_CURRENT_PROGRAM, &id));
	PTGN_ASSERT(id >= 0);
	return id;
}

namespace impl {

Shader ShaderManager::Get(ScreenShader screen_shader) const {
	switch (screen_shader) {
		case ScreenShader::Default:		  return default_;
		case ScreenShader::Blur:		  return blur_;
		case ScreenShader::GaussianBlur:  return gaussian_blur_;
		case ScreenShader::EdgeDetection: return edge_detection_;
		case ScreenShader::InverseColor:  return inverse_color_;
		case ScreenShader::Grayscale:	  return grayscale_;
		case ScreenShader::Sharpen:		  return sharpen_;
		default:						  PTGN_ERROR("Cannot retrieve unrecognized screen shader");
	}
}

Shader ShaderManager::Get(PresetShader shader) const {
	switch (shader) {
		case PresetShader::Quad:   return quad_;
		case PresetShader::Circle: return circle_;
		case PresetShader::Color:  return color_;
		default:				   PTGN_ERROR("Cannot retrieve unrecognized preset shader");
	}
}

void ShaderManager::Init() {
	std::int32_t max_texture_slots{ GLRenderer::GetMaxTextureSlots() };

	PTGN_ASSERT(max_texture_slots > 0, "Max texture slots must be set before initializing shaders");

	PTGN_INFO("Renderer Texture Slots: ", max_texture_slots);
	// This strange way of including files allows for them to be packed into the library binary.
	ShaderSource quad_frag;

	if (max_texture_slots == 8) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_8.frag)
		};
	} else if (max_texture_slots == 16) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_16.frag)
		};
	} else if (max_texture_slots == 32) {
		quad_frag = ShaderSource{
#include PTGN_SHADER_PATH(quad_32.frag)
		};
	} else {
		PTGN_ERROR("Unsupported Texture Slot Size: ", max_texture_slots);
	}

	quad_ = { ShaderSource{
#include PTGN_SHADER_PATH(quad.vert)
			  },
			  quad_frag };

	circle_ = { ShaderSource{
#include PTGN_SHADER_PATH(circle.vert)
				},
				ShaderSource{
#include PTGN_SHADER_PATH(circle.frag)
				} };

	color_ = { ShaderSource{
#include PTGN_SHADER_PATH(color.vert)
			   },
			   ShaderSource{
#include PTGN_SHADER_PATH(color.frag)
			   } };

	InitScreenShaders();
}

} // namespace impl

} // namespace ptgn
