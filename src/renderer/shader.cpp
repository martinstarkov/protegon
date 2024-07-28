#include "protegon/shader.h"

#include "renderer/gl_loader.h"
#include "utility/debug.h"

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
	id_ = gl::CreateProgram();
}

ShaderInstance::~ShaderInstance() {
	gl::DeleteProgram(id_);
}

} // namespace impl

Shader::Shader(const ShaderSource& vertex_shader, const ShaderSource& fragment_shader) {
	CompileProgram(vertex_shader.source_, fragment_shader.source_);
}

Shader::Shader(const path& vertex_shader_path, const path& fragment_shader_path) {
	PTGN_CHECK(
		FileExists(vertex_shader_path), "Cannot create shader from nonexistent vertex shader path"
	);
	PTGN_CHECK(
		FileExists(fragment_shader_path),
		"Cannot create shader from nonexistent fragment shader path"
	);
	CompileProgram(FileToString(vertex_shader_path), FileToString(fragment_shader_path));
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

		PTGN_ERROR("Failed to compile ", impl::GetShaderTypeName(type), " shader");
		PTGN_EXCEPTION("Failed to compile shader");
		return 0;
	}
	return id;
}

void Shader::CompileProgram(const std::string& vertex_source, const std::string& fragment_source) {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::ShaderInstance>();
	} else {
		instance_->location_cache_.clear();
	}

	std::uint32_t vertex   = CompileShader(GL_VERTEX_SHADER, vertex_source);
	std::uint32_t fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);

	if (vertex && fragment) {
		gl::AttachShader(instance_->id_, vertex);
		gl::AttachShader(instance_->id_, fragment);
		gl::LinkProgram(instance_->id_);
		gl::ValidateProgram(instance_->id_);

		// Check for shader link errors.
		std::int32_t linked = GL_FALSE;
		gl::GetProgramiv(instance_->id_, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			gl::GetProgramiv(instance_->id_, GL_INFO_LOG_LENGTH, &length);

			std::vector<gl::GLchar> log(length);
			gl::GetProgramInfoLog(instance_->id_, length, &length, &log[0]);

			PTGN_ERROR(log.data());

			gl::DeleteProgram(instance_->id_);

			gl::DeleteShader(vertex);
			gl::DeleteShader(fragment);

			PTGN_CHECK(
				false, "Failed to link shaders to program"
			); // OPTIONAL: crash on shader link fail.
		}
	}
	if (vertex) {
		gl::DeleteShader(vertex);
	}
	if (fragment) {
		gl::DeleteShader(fragment);
	}
}

void Shader::WhileBound(const std::function<void()>& func) const {
	Bind();
	func();
	Unbind();
}

void Shader::Bind() const {
	PTGN_CHECK(IsValid(), "Attempting to bind shader which has not been initialized");
	gl::UseProgram(instance_->id_);
}

void Shader::Unbind() const {
	gl::UseProgram(0);
}

std::int32_t Shader::GetUniformLocation(const std::string& name) const {
	PTGN_CHECK(
		IsValid(), "Attempting to get uniform location of shader "
				   "which has not been initialized"
	);
	// if (instance_ == nullptr) return -1;
	auto& location_cache{ instance_->location_cache_ };
	auto it = location_cache.find(name);
	if (it != location_cache.end()) {
		return it->second;
	}

	std::int32_t location = gl::GetUniformLocation(instance_->id_, name.c_str());
	// TODO: Consider not adding uniform to cache if it is -1.
	location_cache.emplace(name, location);
	return location;
}

void Shader::SetUniform(const std::string& name, const Vector2<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2f(location, v.x, v.y);
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3f(location, v.x, v.y, v.z);
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<float>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4f(location, v.x, v.y, v.z, v.w);
	}
}

void Shader::SetUniform(const std::string& name, const Matrix4<float>& m) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::UniformMatrix4fv(location, 1, GL_FALSE, m.Data());
	}
}

void Shader::SetUniform(const std::string& name, const std::int32_t* data, std::size_t count)
	const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1iv(location, static_cast<std::uint32_t>(count), data);
	}
}

void Shader::SetUniform(const std::string& name, const float* data, std::size_t count) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1fv(location, static_cast<std::uint32_t>(count), data);
	}
}

void Shader::SetUniform(const std::string& name, const Vector2<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2i(location, v.x, v.y);
	}
}

void Shader::SetUniform(const std::string& name, const Vector3<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3i(location, v.x, v.y, v.z);
	}
}

void Shader::SetUniform(const std::string& name, const Vector4<std::int32_t>& v) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4i(location, v.x, v.y, v.z, v.w);
	}
}

void Shader::SetUniform(const std::string& name, float v0) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1f(location, v0);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2f(location, v0, v1);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3f(location, v0, v1, v2);
	}
}

void Shader::SetUniform(const std::string& name, float v0, float v1, float v2, float v3) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4f(location, v0, v1, v2, v3);
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform1i(location, v0);
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform2i(location, v0, v1);
	}
}

void Shader::SetUniform(const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2)
	const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform3i(location, v0, v1, v2);
	}
}

void Shader::SetUniform(
	const std::string& name, std::int32_t v0, std::int32_t v1, std::int32_t v2, std::int32_t v3
) const {
	std::int32_t location = GetUniformLocation(name);
	if (location != -1) {
		gl::Uniform4i(location, v0, v1, v2, v3);
	}
}

void Shader::SetUniform(const std::string& name, bool value) const {
	SetUniform(name, static_cast<std::int32_t>(value));
}

} // namespace ptgn
