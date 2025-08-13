#include "renderer/shader.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "common/assert.h"
#include "core/game.h"
#include "debug/debugging.h"
#include "debug/log.h"
#include "debug/stats.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/renderer.h"
#include "utility/file.h"

namespace ptgn {

std::string_view GetShaderName(std::uint32_t shader_type) {
	switch (shader_type) {
		case GL_VERTEX_SHADER:	 return "vertex";
		case GL_FRAGMENT_SHADER: return "fragment";
		// case GL_COMPUTE_SHADER:			return "compute";
		// case GL_GEOMETRY_SHADER:		return "geometry";
		// case GL_TESS_CONTROL_SHADER:	return "tess control";
		// case GL_TESS_EVALUATION_SHADER: return "tess evaluation";
		default:				 return "invalid";
	}
}

Shader::Shader(
	const ShaderCode& vertex_shader, const ShaderCode& fragment_shader, std::string_view shader_name
) :
	shader_name_{ shader_name } {
	Create();
	Compile(vertex_shader.source_, fragment_shader.source_);
}

// Extract just the content inside R"( ... )"
void TrimRawStringLiteral(std::string& content) {
	const std::string raw_start = "R\"(";
	const std::string raw_end	= ")\"";

	size_t start = content.find(raw_start);
	size_t end	 = content.rfind(raw_end);

	if (start != std::string::npos && end != std::string::npos &&
		end > start + raw_start.length()) {
		content = content.substr(start + raw_start.length(), end - (start + raw_start.length()));
	}
}

Shader::Shader(
	const ShaderCode& vertex_shader, const path& fragment_shader_path, std::string_view shader_name
) :
	shader_name_{ shader_name } {
	PTGN_ASSERT(
		FileExists(fragment_shader_path),
		"Cannot create shader from nonexistent fragment shader path: ",
		fragment_shader_path.string()
	);
	Create();
	auto fragment_source{ FileToString(fragment_shader_path) };

	TrimRawStringLiteral(fragment_source);

	Compile(vertex_shader.source_, fragment_source);
}

Shader::Shader(
	const path& vertex_shader_path, const path& fragment_shader_path, std::string_view shader_name
) :
	shader_name_{ shader_name } {
	PTGN_ASSERT(
		FileExists(vertex_shader_path),
		"Cannot create shader from nonexistent vertex shader path: ", vertex_shader_path.string()
	);
	PTGN_ASSERT(
		FileExists(fragment_shader_path),
		"Cannot create shader from nonexistent fragment shader path: ",
		fragment_shader_path.string()
	);
	Create();
	Compile(FileToString(vertex_shader_path), FileToString(fragment_shader_path));
}

Shader::Shader(Shader&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	shader_name_{ std::exchange(other.shader_name_, {}) },
	location_cache_{ std::exchange(other.location_cache_, {}) } {}

Shader& Shader::operator=(Shader&& other) noexcept {
	if (this != &other) {
		Delete();
		id_				= std::exchange(other.id_, 0);
		shader_name_	= std::exchange(other.shader_name_, {});
		location_cache_ = std::exchange(other.location_cache_, {});
	}
	return *this;
}

Shader::~Shader() {
	Delete();
}

void Shader::Create() {
	id_ = GLCallReturn(CreateProgram());
	PTGN_ASSERT(IsValid(), "Failed to create shader program using OpenGL context");
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Created shader program with id ", id_);
#endif
}

void Shader::Delete() noexcept {
	if (!IsValid()) {
		return;
	}
	GLCall(DeleteProgram(id_));
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Deleted shader program with id ", id_);
#endif
	id_ = 0;
}

ShaderId Shader::Compile(std::uint32_t type, const std::string& source) {
	ShaderId id{ GLCallReturn(CreateShader(type)) };

	auto src{ source.c_str() };

	GLCall(ShaderSource(id, 1, &src, nullptr));
	GLCall(CompileShader(id));

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	GLCall(GetShaderiv(id, GL_COMPILE_STATUS, &result));

	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetShaderiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetShaderInfoLog(id, length, &length, &log[0]));

		GLCall(DeleteShader(id));

		PTGN_ERROR("Failed to compile ", GetShaderName(type), " shader: \n", source, "\n", log);
	}

	return id;
}

void Shader::Compile(const std::string& vertex_source, const std::string& fragment_source) {
	location_cache_.clear();

	ShaderId vertex{ Compile(GL_VERTEX_SHADER, vertex_source) };
	ShaderId fragment{ Compile(GL_FRAGMENT_SHADER, fragment_source) };

	if (vertex && fragment) {
		GLCall(AttachShader(id_, vertex));
		GLCall(AttachShader(id_, fragment));
		GLCall(LinkProgram(id_));

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(GetProgramiv(id_, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(GetProgramiv(id_, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(GetProgramInfoLog(id_, length, &length, &log[0]));

			GLCall(DeleteProgram(id_));

			GLCall(DeleteShader(vertex));
			GLCall(DeleteShader(fragment));

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(ValidateProgram(id_));
	}

	if (vertex) {
		GLCall(DeleteShader(vertex));
	}

	if (fragment) {
		GLCall(DeleteShader(fragment));
	}
}

void Shader::Bind(ShaderId id) {
	if (game.renderer.bound_.shader_id == id) {
		return;
	}
	GLCall(UseProgram(id));
	game.renderer.bound_.shader_id = id;
#ifdef PTGN_DEBUG
	++game.stats.shader_binds;
#endif
#ifdef GL_ANNOUNCE_SHADER_CALLS
	PTGN_LOG("GL: Bound shader program with id ", id);
#endif
}

void Shader::Bind() const {
	PTGN_ASSERT(IsValid(), "Cannot bind destroyed or uninitialized shader");
	Bind(id_);
}

bool Shader::IsBound() const {
	return GetBoundId() == id_;
}

ShaderId Shader::GetBoundId() {
	std::int32_t id{ -1 };
	GLCall(glGetIntegerv(GL_CURRENT_PROGRAM, &id));
	PTGN_ASSERT(id >= 0, "Failed to retrieve bound shader id");
	return static_cast<ShaderId>(id);
}

std::int32_t Shader::GetUniform(const std::string& name) const {
	PTGN_ASSERT(IsBound(), "Cannot get uniform location of shader which is not currently bound");
	if (auto it{ location_cache_.find(name) }; it != location_cache_.end()) {
		return it->second;
	}

	std::int32_t location{ GLCallReturn(GetUniformLocation(id_, name.c_str())) };

	location_cache_.try_emplace(name, location);
	return location;
}

void Shader::SetUniform(const std::string& name, const Vector2<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<float>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, const Matrix4& matrix) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(UniformMatrix4fv(location, 1, GL_FALSE, matrix.Data()));
	}
}

void Shader::SetUniform(const std::string& name, const std::int32_t* data, std::int32_t count)
	const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1iv(location, count, data));
	}
}

void Shader::SetUniform(const std::string& name, const float* data, std::int32_t count) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1fv(location, count, data));
	}
}

void Shader::SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v.x, v.y));
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v.x, v.y, v.z));
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
	}
}

void Shader::SetUniform(const std::string& name, float v0) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1f(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v0, v1, v2));
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform1i(location, v0));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v0, v1));
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
	const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v0, v1, v2));
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) const {
	std::int32_t location{ GetUniform(name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v0, v1, v2, v3));
	}
}

void Shader::SetUniform(const std::string& name, bool value) const {
	SetUniform(name, static_cast<std::int32_t>(value));
}

bool Shader::IsValid() const {
	return id_;
}

ShaderId Shader::GetId() const {
	return id_;
}

std::string_view Shader::GetName() const {
	return shader_name_;
}

namespace impl {

void ShaderManager::Init() {
	std::uint32_t max_texture_slots{ GLRenderer::GetMaxTextureSlots() };

	PTGN_ASSERT(max_texture_slots > 0, "Max texture slots must be set before initializing shaders");

	PTGN_INFO("Renderer Texture Slots: ", max_texture_slots);
	// This strange way of including files allows for them to be packed into the library binary.
	ShaderCode quad_frag;

	if (max_texture_slots == 8) {
		quad_frag = ShaderCode{
#include PTGN_SHADER_PATH(quad_8.frag)
		};
	} else if (max_texture_slots == 16) {
		quad_frag = ShaderCode{
#include PTGN_SHADER_PATH(quad_16.frag)
		};
	} else if (max_texture_slots == 32) {
		quad_frag = ShaderCode{
#include PTGN_SHADER_PATH(quad_32.frag)
		};
	} else {
		PTGN_ERROR("Unsupported Texture Slot Size: ", max_texture_slots);
	}

	quad_ = { ShaderCode{
#include PTGN_SHADER_PATH(quad.vert)
			  },
			  quad_frag, "Quad" };

	InitShapeShaders();
	InitScreenShaders();
	InitOtherShaders();
}

} // namespace impl

} // namespace ptgn