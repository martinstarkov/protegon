#include "protegon/shader.h"

#include "protegon/debug.h"
#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

std::string_view GetShaderTypeName(std::uint32_t type) {
	switch (type) {
		case GL_VERTEX_SHADER:			return "vertex";
		case GL_FRAGMENT_SHADER:		return "fragment";
		case GL_COMPUTE_SHADER:			return "compute";
		case GL_GEOMETRY_SHADER:		return "geometry";
		case GL_TESS_CONTROL_SHADER:	return "tess control";
		case GL_TESS_EVALUATION_SHADER: return "tess evaluation";
		default:						return "invalid";
	}
}

ShaderInstance::ShaderInstance(Id program_id) : program_id_{ program_id } {}

ShaderInstance::~ShaderInstance() {
	gl::DeleteProgram(program_id_);
}

} // namespace impl

Shader::Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader) {
	Create(vertex_shader.source_, fragment_shader.source_);
}

Shader::Shader(const path& vertex_shader_path, const path& fragment_shader_path) {
	PTGN_CHECK(
		FileExists(vertex_shader_path), "Cannot create shader from nonexistent vertex shader path"
	);
	PTGN_CHECK(
		FileExists(fragment_shader_path),
		"Cannot create shader from nonexistent fragment shader path"
	);
	Create(FileToString(vertex_shader_path), FileToString(fragment_shader_path));
}

void Shader::Create(
	const std::string& vertex_shader_source, const std::string& fragment_shader_source
) {
	impl::Id program = CompileProgram(vertex_shader_source, fragment_shader_source);
	instance_		 = std::shared_ptr<impl::ShaderInstance>{ new impl::ShaderInstance(program) };
}

std::uint32_t Shader::CompileShader(std::uint32_t type, const std::string& source) {
	std::uint32_t id = gl::CreateShader(type);
	const char* src	 = source.c_str();
	gl::ShaderSource(id, 1, &src, NULL);
	gl::CompileShader(id);

	// Check for shader compilation errors.
	std::int32_t result{ GL_FALSE };
	gl::GetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		std::int32_t length{ 0 };
		gl::GetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		std::vector<gl::GLchar> log(length);
		gl::GetShaderInfoLog(id, length, &length, &log[0]);

		PTGN_ERROR(log.data());

		gl::DeleteShader(id);

		PTGN_CHECK(false, "Failed to compile ", impl::GetShaderTypeName(type), " shader");
		return 0;
	}
	return id;
}

impl::Id Shader::CompileProgram(
	const std::string& vertex_source, const std::string& fragment_source
) {
	impl::Id program = gl::CreateProgram();

	std::uint32_t vertex   = CompileShader(GL_VERTEX_SHADER, vertex_source);
	std::uint32_t fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);

	if (vertex && fragment) {
		gl::AttachShader(program, vertex);
		gl::AttachShader(program, fragment);
		gl::LinkProgram(program);
		gl::ValidateProgram(program);

		// Check for shader link errors.
		std::int32_t linked = GL_FALSE;
		gl::GetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			gl::GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

			std::vector<gl::GLchar> log(length);
			gl::GetProgramInfoLog(program, length, &length, &log[0]);

			PTGN_ERROR(log.data());

			gl::DeleteProgram(program);

			gl::DeleteShader(vertex);
			gl::DeleteShader(fragment);

			PTGN_CHECK(
				false, "Failed to link shaders to program"
			); // OPTIONAL: crash on shader link fail.
			return 0;
		}
	}
	if (vertex) {
		gl::DeleteShader(vertex);
	}
	if (fragment) {
		gl::DeleteShader(fragment);
	}
	return program;
}

void Shader::WhileBound(std::function<void()> func) {
	Bind();
	func();
	Unbind();
}

void Shader::Bind() const {
	PTGN_CHECK(instance_ != nullptr, "Attempting to bind shader which has not been initialized");
	gl::UseProgram(instance_->program_id_);
}

void Shader::Unbind() const {
	gl::UseProgram(0);
}

std::uint32_t Shader::GetProgramId() const {
	return instance_ != nullptr ? instance_->program_id_ : 0;
}

std::int32_t Shader::GetUniformLocation(const std::string& name) const {
	PTGN_CHECK(
		instance_ != nullptr, "Attempting to get uniform location of shader "
							  "which has not been initialized"
	);
	// if (instance_ == nullptr) return -1;
	auto& location_cache{ instance_->location_cache_ };
	auto it = location_cache.find(name);
	if (it != location_cache.end()) {
		return it->second;
	}

	std::int32_t location = gl::GetUniformLocation(instance_->program_id_, name.c_str());
	// TODO: Consider not adding uniform to cache if it is -1.
	location_cache.emplace(name, location);
	return location;
}

void Shader::SetUniform(const std::string& name, const Matrix4<float>& m) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::UniformMatrix4fv(location, 1, GL_FALSE, m.m.data());
	}
}

void Shader::SetUniform(const std::string& name, float v0) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1f(location, v0);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2f(location, v0, v1);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3f(location, v0, v1, v2);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4f(location, v0, v1, v2, v3);
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1i(location, v0);
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2i(location, v0, v1);
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2
) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3i(location, v0, v1, v2);
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4i(location, v0, v1, v2, v3);
	}
}

void Shader::SetUniform(const std::string& name, bool value) {
	SetUniform(name, static_cast<std::int32_t>(value));
}

} // namespace ptgn
