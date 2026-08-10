#include "renderer/shader_compiler.h"

#include <glad/gl.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ptgn::impl {

namespace {

struct ParsedStage {
	ShaderStageMask stage{ ShaderStageMask::None };
	std::string source;
};

std::string Trim(std::string value) {
	const auto first{ value.find_first_not_of(" \t\r\n") };
	if (first == std::string::npos) {
		return {};
	}
	const auto last{ value.find_last_not_of(" \t\r\n") };
	return value.substr(first, last - first + 1);
}

std::string ReplaceAll(std::string value, std::string_view from, std::string_view to) {
	if (from.empty()) {
		return value;
	}
	std::size_t position{ 0 };
	while ((position = value.find(from, position)) != std::string::npos) {
		value.replace(position, from.size(), to);
		position += to.size();
	}
	return value;
}

std::string StageName(ShaderStageMask stage) {
	if (stage == ShaderStageMask::Vertex) {
		return "vertex";
	}
	if (stage == ShaderStageMask::Fragment) {
		return "fragment";
	}
	return "unknown";
}

std::vector<ParsedStage> ParseStages(std::string_view source, std::string& error) {
	std::vector<ParsedStage> stages;
	std::string input{ source };
	std::regex marker{ R"(#type\s+(\w+))" };
	struct StageMarker {
		std::string type;
		std::size_t begin{ 0 };
		std::size_t end{ 0 };
	};
	std::vector<StageMarker> markers;
	for (auto it{ std::sregex_iterator(input.begin(), input.end(), marker) };
		 it != std::sregex_iterator{}; ++it) {
		const auto begin{ static_cast<std::size_t>((*it).position()) };
		markers.push_back(StageMarker{
			.type = (*it)[1].str(),
			.begin = begin,
			.end = begin + static_cast<std::size_t>((*it).length()),
		});
	}
	if (markers.empty()) {
		error = "No #type vertex or #type fragment declaration was found.";
		return stages;
	}

	const std::string header{ Trim(input.substr(0, markers.front().begin)) };
	for (std::size_t i{ 0 }; i < markers.size(); ++i) {
		ShaderStageMask stage{ ShaderStageMask::None };
		if (markers[i].type == "vertex") {
			stage = ShaderStageMask::Vertex;
		} else if (markers[i].type == "fragment") {
			stage = ShaderStageMask::Fragment;
		} else {
			continue;
		}
		if (std::ranges::any_of(stages, [stage](const ParsedStage& s) { return s.stage == stage; })) {
			error = "A GLSL file may contain only one #type " + StageName(stage) + " block.";
			return {};
		}
		const std::size_t start{ markers[i].end };
		const std::size_t end{ i + 1 < markers.size() ? markers[i + 1].begin : input.size() };
		std::string body{ Trim(input.substr(start, end - start)) };
		std::string combined;
		if (!header.empty()) {
			combined = header + "\n";
		}
		combined += std::move(body);
		stages.push_back({ .stage = stage, .source = std::move(combined) });
	}
	if (stages.empty()) {
		error = "No supported vertex or fragment shader stage was found.";
	}
	return stages;
}

bool HasOption(std::string_view source, std::string_view option) {
	return source.contains(std::string{ "#option " } + std::string{ option });
}

void RemoveOptions(std::string& source) {
	const std::regex option{
		R"(^\s*#option\s+\w+\s*\n?)",
		std::regex_constants::icase | std::regex_constants::multiline
	};
	source = std::regex_replace(source, option, "");
}

bool AddAutoLayout(std::string& source, [[maybe_unused]] ShaderStageMask stage, std::string& error) {
	std::istringstream input{ source };
	std::ostringstream output;
	std::string line;
	bool in_main{ false };
	int current_in_location{ 0 };
	int current_out_location{ 0 };
	const std::regex variable{
		R"(^\s*((?:flat|smooth|noperspective)\s+)?(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;\r?$)"
	};
	const std::regex explicit_layout{ R"(layout\s*\(\s*location\s*=\s*\d+\s*\))" };
	std::smatch match;

	while (std::getline(input, line)) {
		if (!in_main && line.contains("void main")) {
			in_main = true;
		}
		if (in_main || !std::regex_match(line, match, variable)) {
			if (!in_main && std::regex_search(line, explicit_layout)) {
				error = "#option auto_layout cannot be combined with explicit layout(location=...).";
				return false;
			}
			output << line << '\n';
			continue;
		}
		bool inject{ true };
		const std::string interpolation{ match[1].str() };
		const std::string qualifier{ match[2].str() };
#ifdef __EMSCRIPTEN__
		if (!((stage == ShaderStageMask::Vertex && qualifier == "in") ||
			  (stage == ShaderStageMask::Fragment && qualifier == "out"))) {
			inject = false;
		}
#endif
		if (!inject) {
			output << line << '\n';
			continue;
		}
		const int location{ qualifier == "in" ? current_in_location++ : current_out_location++ };
		output << std::format(
			"layout(location = {}) {}{} {} {};\n",
			location,
			interpolation,
			qualifier,
			match[3].str(),
			match[4].str()
		);
	}
	source = output.str();
	return true;
}

std::string TextureColorSwitch(std::size_t max_slots) {
	std::ostringstream output;
	for (std::size_t i{ 0 }; i < max_slots; ++i) {
		output << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n\t\ttexture_color *= texture(u_Textures[{}], v_TexCoord);\n\t}}\n",
			i,
			i
		);
	}
	return output.str();
}

std::string TextureSizeSwitch(std::size_t max_slots) {
	std::ostringstream output;
	for (std::size_t i{ 0 }; i < max_slots; ++i) {
		output << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n\t\ttexture_size = vec2(textureSize(u_Textures[{}], 0));\n\t}}\n",
			i,
			i
		);
	}
	return output.str();
}

bool PrepareSource(std::string& source, ShaderStageMask stage, std::size_t max_slots, std::string& error) {
	if (HasOption(source, "auto_layout") && !AddAutoLayout(source, stage, error)) {
		return false;
	}
	RemoveOptions(source);

	std::regex version_regex{ R"(#version\s+(\d+)(?:\s+(\w+))?)" };
	if (!std::regex_search(source, version_regex)) {
#ifdef __EMSCRIPTEN__
		source = "#version 300 es\n" + source;
#else
		source = "#version 330 core\n" + source;
#endif
	}
	const auto line_end{ source.find('\n') };
	const std::size_t insert_position{ line_end == std::string::npos ? source.size() : line_end + 1 };
#ifdef __EMSCRIPTEN__
	if (!source.contains("precision ")) {
		source.insert(insert_position, "precision highp float;\n");
	}
#else
	if (!source.contains("#extension GL_ARB_separate_shader_objects")) {
		source.insert(insert_position, "#extension GL_ARB_separate_shader_objects : require\n");
	}
#endif
	const auto slots{ std::to_string(std::max<std::size_t>(1, max_slots)) };
	source = ReplaceAll(std::move(source), "{MAX_TEXTURE_SLOTS}", slots);
	source = ReplaceAll(std::move(source), "{TEXTURE_COLOR_SWITCH_BLOCK}", TextureColorSwitch(max_slots));
	source = ReplaceAll(std::move(source), "{TEXTURE_SIZE_SWITCH_BLOCK}", TextureSizeSwitch(max_slots));
	return true;
}

struct CompiledStage {
	GLuint id{ 0 };
	std::string log;
	bool success{ false };
};

CompiledStage CompileStage(ShaderStageMask stage, std::string source, std::size_t max_slots) {
	CompiledStage result;
	std::string prepare_error;
	if (!PrepareSource(source, stage, max_slots, prepare_error)) {
		result.log = prepare_error;
		return result;
	}
	const auto gl_stage{ stage == ShaderStageMask::Vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER };
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

ShaderCompileResult ValidatePair(
	std::string vertex_source,
	std::string fragment_source,
	std::size_t max_slots
) {
	ShaderCompileResult result;
	auto vertex{ CompileStage(ShaderStageMask::Vertex, std::move(vertex_source), max_slots) };
	auto fragment{ CompileStage(ShaderStageMask::Fragment, std::move(fragment_source), max_slots) };
	if (!vertex.log.empty()) {
		result.log += "[Vertex]\n" + vertex.log + "\n";
	}
	if (!fragment.log.empty()) {
		result.log += "[Fragment]\n" + fragment.log + "\n";
	}
	if (!vertex.success || !fragment.success) {
		if (vertex.id) glDeleteShader(vertex.id);
		if (fragment.id) glDeleteShader(fragment.id);
		result.success = false;
		return result;
	}
	const GLuint program{ glCreateProgram() };
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
		if (!log.empty()) result.log += "[Link]\n" + log + "\n";
	}
	result.success = linked == GL_TRUE;
	if (!result.success && result.log.empty()) result.log = "Shader program failed to link.";
	if (result.success && result.log.empty()) result.log = "Compilation and link succeeded.";
	glDeleteProgram(program);
	glDeleteShader(vertex.id);
	glDeleteShader(fragment.id);
	return result;
}

} // namespace

std::string ExtractShaderStageSource(std::string_view source, ShaderStageMask requested) {
	std::string error;
	auto stages{ ParseStages(source, error) };
	for (const auto& stage : stages) {
		if (stage.stage == requested) {
			return std::string{ "#type " } + StageName(requested) + "\n" + stage.source;
		}
	}
	return {};
}

ShaderCompileResult ValidateShaderProgram(
	std::string_view vertex_source,
	std::string_view fragment_source,
	std::size_t max_texture_slots
) {
	std::string vertex_error;
	auto vertex_stages{ ParseStages(vertex_source, vertex_error) };
	if (!vertex_error.empty()) return { false, "[Vertex]\n" + vertex_error };
	std::string fragment_error;
	auto fragment_stages{ ParseStages(fragment_source, fragment_error) };
	if (!fragment_error.empty()) return { false, "[Fragment]\n" + fragment_error };
	std::string vertex;
	std::string fragment;
	for (auto& stage : vertex_stages) {
		if (stage.stage == ShaderStageMask::Vertex) {
			vertex = std::move(stage.source);
		}
	}
	for (auto& stage : fragment_stages) {
		if (stage.stage == ShaderStageMask::Fragment) {
			fragment = std::move(stage.source);
		}
	}
	if (vertex.empty()) return { false, "[Vertex]\nSelected source has no #type vertex block." };
	if (fragment.empty()) return { false, "[Fragment]\nSelected source has no #type fragment block." };
	return ValidatePair(std::move(vertex), std::move(fragment), max_texture_slots);
}

ShaderCompileResult ValidateShaderSource(std::string_view source, std::size_t max_texture_slots) {
	std::string error;
	auto stages{ ParseStages(source, error) };
	if (!error.empty()) return { false, std::move(error) };
	std::string vertex;
	std::string fragment;
	for (auto& stage : stages) {
		if (stage.stage == ShaderStageMask::Vertex) vertex = std::move(stage.source);
		else if (stage.stage == ShaderStageMask::Fragment) fragment = std::move(stage.source);
	}
	if (!vertex.empty() && !fragment.empty()) {
		return ValidatePair(std::move(vertex), std::move(fragment), max_texture_slots);
	}
	const ShaderStageMask stage{ !vertex.empty() ? ShaderStageMask::Vertex : ShaderStageMask::Fragment };
	auto compiled{ CompileStage(
		stage,
		!vertex.empty() ? std::move(vertex) : std::move(fragment),
		max_texture_slots
	) };
	ShaderCompileResult result{ .success = compiled.success };
	result.log = compiled.log;
	if (compiled.id) glDeleteShader(compiled.id);
	if (result.success && result.log.empty()) {
		result.log = StageName(stage) + " stage compiled successfully (link not tested).";
	}
	return result;
}

} // namespace ptgn::impl
