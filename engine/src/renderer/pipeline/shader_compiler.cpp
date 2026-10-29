#include "renderer/pipeline/shader_compiler.h"

#include "renderer/backend/gl/gl.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ptgn::impl {

namespace {

struct CompiledStage {
	GLuint id{ 0 };
	std::string log{};
	bool success{ false };
};

CompiledStage CompileStage(ShaderStageMask stage, const std::string& source) {
	CompiledStage result;
	auto gl_stage{ stage == ShaderStageMask::Vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER };

	result.id = glCreateShader(static_cast<GLenum>(gl_stage));
	if (result.id == 0) {
		result.log = "OpenGL could not create a temporary shader object.";
		return result;
	}

	const char* raw{ source.c_str() };
	glShaderSource(result.id, 1, &raw, nullptr);
	glCompileShader(result.id);

	GLint compiled{ GL_FALSE };
	glGetShaderiv(result.id, GL_COMPILE_STATUS, &compiled);

	GLint length{ 0 };
	glGetShaderiv(result.id, GL_INFO_LOG_LENGTH, &length);
	if (length > 1) {
		result.log.resize(static_cast<std::size_t>(length));
		GLsizei written{ 0 };
		glGetShaderInfoLog(result.id, length, &written, result.log.data());
		result.log.resize(static_cast<std::size_t>(std::max<GLsizei>(0, written)));
	}

	result.success = compiled == GL_TRUE;
	if (!result.success && result.log.empty()) {
		result.log = "Shader compilation failed without an OpenGL diagnostic message.";
	}
	return result;
}

ShaderCompileResult ValidatePair(const std::string& vertex_source, const std::string& fragment_source) {
	ShaderCompileResult result;
	auto vertex{ CompileStage(ShaderStageMask::Vertex, vertex_source) };
	auto fragment{ CompileStage(ShaderStageMask::Fragment, fragment_source) };

	if (!vertex.log.empty()) {
		result.log += "[Vertex]\n" + vertex.log + "\n";
	}
	if (!fragment.log.empty()) {
		result.log += "[Fragment]\n" + fragment.log + "\n";
	}

	if (!vertex.success || !fragment.success) {
		if (vertex.id) {
			glDeleteShader(vertex.id);
		}
		if (fragment.id) {
			glDeleteShader(fragment.id);
		}
		return result;
	}

	GLuint program{ glCreateProgram() };
	glAttachShader(program, vertex.id);
	glAttachShader(program, fragment.id);
	glLinkProgram(program);

	GLint linked{ GL_FALSE };
	glGetProgramiv(program, GL_LINK_STATUS, &linked);

	GLint length{ 0 };
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
	if (length > 1) {
		std::string log(static_cast<std::size_t>(length), '\0');
		GLsizei written{ 0 };
		glGetProgramInfoLog(program, length, &written, log.data());
		log.resize(static_cast<std::size_t>(std::max<GLsizei>(0, written)));
		if (!log.empty()) {
			result.log += "[Link]\n" + log + "\n";
		}
	}

	result.success = linked == GL_TRUE;
	if (!result.success && result.log.empty()) {
		result.log = "Shader program failed to link.";
	}
	if (result.success && result.log.empty()) {
		result.log = "Compilation and link succeeded.";
	}

	glDeleteProgram(program);
	glDeleteShader(vertex.id);
	glDeleteShader(fragment.id);
	return result;
}

const PreparedShaderStage* FindStage(
	const std::vector<PreparedShaderStage>& stages,
	ShaderStageMask stage
) {
	auto it{ std::ranges::find_if(stages, [stage](const PreparedShaderStage& candidate) {
		return candidate.stage == stage;
	}) };
	return it == stages.end() ? nullptr : &*it;
}

} // namespace

ShaderCompileResult ValidateShaderProgram(
	std::string_view vertex_source,
	std::string_view fragment_source,
	std::size_t max_texture_slots
) {
	auto prepared_vertex{ PrepareShaderSource(vertex_source, max_texture_slots) };
	if (!prepared_vertex.has_value()) {
		return { false, "[Vertex]\n" + prepared_vertex.error() };
	}

	auto prepared_fragment{ PrepareShaderSource(fragment_source, max_texture_slots) };
	if (!prepared_fragment.has_value()) {
		return { false, "[Fragment]\n" + prepared_fragment.error() };
	}

	auto vertex{ FindStage(prepared_vertex.value(), ShaderStageMask::Vertex) };
	auto fragment{ FindStage(prepared_fragment.value(), ShaderStageMask::Fragment) };
	if (!vertex) {
		return { false, "[Vertex]\nSelected source has no #type vertex block." };
	}
	if (!fragment) {
		return { false, "[Fragment]\nSelected source has no #type fragment block." };
	}

	return ValidatePair(vertex->source, fragment->source);
}

ShaderCompileResult ValidateShaderSource(std::string_view source, std::size_t max_texture_slots) {
	auto prepared{ PrepareShaderSource(source, max_texture_slots) };
	if (!prepared.has_value()) {
		return { false, prepared.error() };
	}

	auto vertex{ FindStage(prepared.value(), ShaderStageMask::Vertex) };
	auto fragment{ FindStage(prepared.value(), ShaderStageMask::Fragment) };

	if (vertex && fragment) {
		return ValidatePair(vertex->source, fragment->source);
	}

	auto stage{ vertex ? vertex : fragment };
	if (!stage) {
		return { false, "No supported vertex or fragment shader stage was found." };
	}

	auto compiled{ CompileStage(stage->stage, stage->source) };
	ShaderCompileResult result{ .success = compiled.success, .log = std::move(compiled.log) };
	if (compiled.id) {
		glDeleteShader(compiled.id);
	}
	if (result.success && result.log.empty()) {
		result.log = std::string{ ShaderStageName(stage->stage) } +
					 " stage compiled successfully (link not tested).";
	}
	return result;
}

} // namespace ptgn::impl
