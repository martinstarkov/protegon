#include "renderer/backend/gl/gl_context.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>

#include <array>
#include <cmrc/cmrc.hpp>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <ostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "core/util/macro.h"
#include "core/util/span.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_handle.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/resources/shader.h"
#include "serialization/json/fwd.h"

#define PTGN_VSYNC_MODE -1

namespace ptgn::impl::gl {

struct GLVersion {
	GLVersion() {
		bool r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
		PTGN_ASSERT(r, SDL_GetError());
		r = SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
		PTGN_ASSERT(r, SDL_GetError());
	}

	int major{ 0 };
	int minor{ 0 };
};

inline std::ostream& operator<<(std::ostream& os, const GLVersion& v) {
	os << v.major << "." << v.minor;
	return os;
}

// TODO: Move all this code to a shader parsing file.

using Header = std::string;

static std::string TrimWhitespace(const std::string& s) {
	std::size_t start{ s.find_first_not_of(" \n\r\t") };
	if (start == std::string::npos) {
		return "";
	}
	std::size_t end{ s.find_last_not_of(" \n\r\t") };
	return s.substr(start, end - start + 1);
}

static GLenum GetShaderType(const std::string& type) {
	if (type == "fragment") {
		return GL_FRAGMENT_SHADER;
	} else if (type == "vertex") {
		return GL_VERTEX_SHADER;
	}
	PTGN_ERROR("Unknown shader type: ", type);
}

// Extract just the content inside R"( ... )"
static void TrimRawStringLiteral(std::string& content) {
	const std::string raw_start{ "R\"(" };
	const std::string raw_end{ ")\"" };

	std::size_t start{ content.find(raw_start) };
	std::size_t end{ content.rfind(raw_end) };

	if (start != std::string::npos && end != std::string::npos &&
		end > start + raw_start.length()) {
		content = content.substr(start + raw_start.length(), end - (start + raw_start.length()));
	}
}

static std::pair<Header, std::vector<ShaderTypeSource>> ParseShaderSources(
	const std::string& source, const std::string& name_without_ext
) {
	Header header;
	std::vector<ShaderTypeSource> sources;

	std::string input{ source };
	TrimRawStringLiteral(input);

	const auto contains_type = [&sources](GLenum type) {
		return VectorFindIf(sources, [type](const ShaderTypeSource& sts) {
			return sts.type == type;
		});
	};

	// Regex to find: #type <stage> and capture everything until next #type or EOF
	std::regex type_regex(R"(#type\s+(\w+))");
	auto words_begin{ std::sregex_iterator(input.begin(), input.end(), type_regex) };
	auto words_end{ std::sregex_iterator() };

	std::vector<std::pair<std::string, std::size_t>> found_types; // (type, position)

	for (auto i{ words_begin }; i != words_end; ++i) {
		std::smatch match{ *i };
		std::string type{ match[1].str() };
		std::size_t pos{ static_cast<std::size_t>(match.position()) };
		found_types.emplace_back(type, pos);
	}

	PTGN_ASSERT(
		!found_types.empty(), "No #type declarations found in shader source: ", name_without_ext
	);

	// Extract header before the first #type
	std::size_t first_type_pos{ found_types.front().second };
	std::string header_code{ input.substr(0, first_type_pos) };
	header = TrimWhitespace(header_code);

	// Extract blocks between #type markers
	for (std::size_t i = 0; i < found_types.size(); i++) {
		auto type_string{ found_types[i].first };
		auto type{ GetShaderType(type_string) };
		std::size_t start{ found_types[i].second + std::string("#type ").size() +
						   type_string.size() };

		std::size_t end{ input.size() };

		if (i + 1 < found_types.size()) {
			end = found_types[i + 1].second;
		}

		std::string code{ input.substr(start, end - start) };
		code = TrimWhitespace(code);

		PTGN_ASSERT(!contains_type(type), "GLSL file can only contain one type of shader: ", type);

		sources.emplace_back(type, ShaderCode{ code }, name_without_ext);
	}

	return { header, sources };
}

static bool HasOption(const std::string& string, const std::string& option_name) {
	return string.find("#option " + option_name) != std::string::npos;
}

static void RemoveOption(std::string& source, const std::string& option = "") {
	// @param option Default: Removes all options in source.
	std::regex pattern;

	if (option.empty()) {
		// Remove ALL `#option <something>` lines (case-insensitive)
		pattern = std::regex(R"(^\s*#option\s+\w+\s*\n?)", std::regex::icase);
	} else {
		// Remove only specific `#option <option>` lines (case-insensitive)
		pattern = std::regex(R"(^\s*#option\s+)" + option + R"(\s*\n?)", std::regex::icase);
	}

	source = std::regex_replace(source, pattern, "");
}

static std::string InjectShaderPreamble(const std::string& source, [[maybe_unused]] GLenum type) {
	std::string result{ source };

	std::regex version_regex(R"(#version\s+(\d+)(?:\s+(\w+))?)");
	std::smatch match;

	if (std::regex_search(source, match, version_regex)) {
		std::string version_number{ match[1].str() };		  // e.g. "330" or "300"
		std::string version_profile{ match.size() > 2 ? match[2].str()
													  : "" }; // e.g. "core" or "es"

#ifdef __EMSCRIPTEN__
		PTGN_ASSERT(
			version_number == "300" && version_profile == "es",
			"For Emscripten, shader must specify '#version 300 es'"
		);
#else
		PTGN_ASSERT(
			version_number == "330" && version_profile == "core",
			"For desktop, shader must specify '#version 330 core'"
		);
#endif
	} else {
#ifdef __EMSCRIPTEN__
		// Automatically add version directive.
		result = "#version 300 es\n" + result;
#else
		result = "#version 330 core\n" + result;
#endif
	}

	// Insert after #version line
	size_t version_line_end{ result.find('\n') };
	size_t insert_pos{ (version_line_end != std::string::npos) ? version_line_end + 1
															   : result.size() };

#ifdef __EMSCRIPTEN__
	// Inject precision (only for on Emscripten)
	std::regex precision_regex(R"(precision\s+(highp|mediump|lowp)\s+float\s*;)");
	if (!std::regex_search(result, precision_regex)) {
		std::string precision{ "precision highp float;\n" };
		result.insert(insert_pos, precision);
		// insert_pos += extension.length(); // Update insert position.
	}
#else
	// Inject #extension if needed (desktop only)
	if (result.find("#extension GL_ARB_separate_shader_objects") == std::string::npos) {
		std::string extension{ "#extension GL_ARB_separate_shader_objects : require\n" };
		result.insert(insert_pos, extension);
		// insert_pos += extension.length(); // Update insert position.
	}
#endif

	return result;
}

static void AddShaderLayout(std::string& source, [[maybe_unused]] GLenum type) {
	std::string result;

	std::istringstream input{ source };
	std::ostringstream output;

	std::string line;
	bool in_main{ false };
	int current_in_location{ 0 };
	int current_out_location{ 0 };

	// Matches GLSL input/output variable declarations like:
	//    in vec3 position;
	//    out vec4 o_Color;
	// The pattern explained:
	// ^\s*                      - Start of line with optional leading whitespace
	// (in|out)                  - Capture group 1: either 'in' or 'out'
	// \s+                       - One or more spaces after 'in' or 'out'
	// [a-zA-Z_][a-zA-Z0-9_]*    - Capture group 2: type name (e.g., vec3, float), must start
	// with a letter or underscore
	// \s+                       - One or more spaces after type
	// [a-zA-Z_][a-zA-Z0-9_]*    - Capture group 3: variable name (e.g., a_Position, o_Color),
	// valid identifier
	// \s*;                      - Optional spaces before semicolon, then a required semicolon
	// \r?                       - Match zero or one carriage return character
	// $                         - Match string end
	std::regex var_decl_regex(
		R"(^\s*(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;\r?$)"
	);

	std::smatch match;

	std::regex layout_regex(R"(layout\s*\(\s*location\s*=\s*\d+\s*\))");

	while (std::getline(input, line)) {
		// Stop injecting once we hit `void main()`
		if (!in_main && line.find("void main") != std::string::npos) {
			in_main = true;
		}

		if (in_main) {
			output << line << "\n";
			continue;
		}

		PTGN_ASSERT(
			!std::regex_search(line, layout_regex),
			"Cannot use #option auto_layout and define a custom attribute layout: ", line
		);

		if (!std::regex_match(line, match, var_decl_regex)) {
			output << line << "\n";
			continue;
		}

		bool inject_layout{ true };

		std::string qualifier{ match[1].str() }; // "in" or "out"

#ifdef __EMSCRIPTEN__
		// Only inject layout for Vertex Shader & 'in' variables on WebAssembly
		if (!(type == GL_VERTEX_SHADER && qualifier == "in")) {
			inject_layout = false;
		}
#endif

		if (inject_layout) {
			std::string variable_type{ match[2].str() }; // (e.g., vec3)
			std::string variable_name{ match[3].str() }; // (e.g., a_Position)

			int location{ (qualifier == "in") ? current_in_location++ : current_out_location++ };

			std::string layout_line{ std::format(
				"layout(location = {}) {} {} {};", location, qualifier, variable_type, variable_name
			) };

			output << layout_line << "\n";
			continue;
		}

		output << line << "\n";
	}

	source = output.str();
}

static std::string GenerateTextureSwitchBlock(std::size_t max_texture_slots) {
	std::ostringstream oss;
	for (std::size_t i{ 0 }; i < max_texture_slots; ++i) {
		oss << std::format(
			"    if (v_TexIndex == {}.0f) {{\n"
			"        texColor *= texture(u_Textures[{}], v_TexCoord);\n"
			"    }}\n",
			i, i
		);
	}
	return oss.str();
}

static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
	if (from.empty()) {
		return str;
	}

	std::size_t start_pos{ 0 };
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Move past the replacement
	}
	return str;
}

static std::vector<ShaderTypeSource> ParseShader(
	const std::string& source, const std::string& name_without_ext
) {
	std::vector<ShaderTypeSource> output;

	auto [header, sources] = ParseShaderSources(source, name_without_ext);

	// PTGN_LOG("-------- Name ---------");
	// PTGN_LOG(name_without_ext);
	// PTGN_LOG("------- Header ---------");
	// PTGN_LOG(header);

	auto auto_layout_name{ "auto_layout" };

	ShaderOptions global_options;
	global_options.auto_layout = HasOption(header, "auto_layout");
	global_options.batchable   = HasOption(header, "batchable");

	for (std::size_t i{ 0 }; i < sources.size(); i++) {
		auto& sts{ sources[i] };
		sts.options = global_options;

		auto& src{ sts.source.source };

		sts.options.auto_layout |= HasOption(src, "auto_layout");
		sts.options.batchable	|= HasOption(src, "batchable");

		if (sts.options.auto_layout) {
			AddShaderLayout(src, sts.type);
		}

		RemoveOption(src);

		src = InjectShaderPreamble(src, sts.type);
		output.emplace_back(sts);

		// PTGN_LOG("------- Source ", i, " (type: ", sts.type, ") -------------");
		// PTGN_LOG(src);
	}
	return output;
}

void GLContext::CompileShaders(
	const std::vector<ShaderTypeSource>& sources,
	std::unordered_map<std::size_t, GLuint>& vertex_shaders,
	std::unordered_map<std::size_t, GLuint>& fragment_shaders,
	std::vector<GLuint>& batchable_shaders
) const {
	for (const auto& sts : sources) {
		auto hash{ Hash(sts.name) };
		auto shader_id{ CompileShaderFromSource(sts.type, sts.source.source) };

		switch (sts.type) {
			case GL_FRAGMENT_SHADER:
				PTGN_ASSERT(
					!fragment_shaders.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				fragment_shaders.emplace(hash, std::move(shader_id));
				if (sts.options.batchable) {
					batchable_shaders.push_back(shader_id);
				}
				break;
			case GL_VERTEX_SHADER:
				PTGN_ASSERT(
					!vertex_shaders.contains(hash), "Cannot add shader to cache twice: ", sts.name
				);
				vertex_shaders.emplace(hash, std::move(shader_id));
				PTGN_ASSERT(
					!sts.options.batchable, "Vertex shaders cannot use the 'batchable' option"
				);
				break;
			default: PTGN_ERROR("Unknown shader type");
		}
	}
}

static void SubstituteShaderTokens(
	std::vector<ShaderTypeSource>& sources, std::size_t max_texture_slots
) {
	// This is primarily for the quad shader, which requires a block of if-statements based on
	// how many texture slots there are.

	PTGN_ASSERT(max_texture_slots > 0, "Cannot substitute shader tokens for 0 texture slots");

	std::string switch_block{ GenerateTextureSwitchBlock(max_texture_slots) };
	auto slots{ std::to_string(max_texture_slots) };

	for (auto& sts : sources) {
		sts.source.source = ReplaceAll(sts.source.source, "{MAX_TEXTURE_SLOTS}", slots);
		sts.source.source = ReplaceAll(sts.source.source, "{TEXTURE_SWITCH_BLOCK}", switch_block);
	}
}

void GLContext::PopulateShaderCache(
	const cmrc::embedded_filesystem& filesystem,
	std::unordered_map<std::size_t, GLuint>& vertex_shaders,
	std::unordered_map<std::size_t, GLuint>& fragment_shaders,
	std::vector<GLuint>& batchable_shaders, std::size_t max_texture_slots
) const {
	const std::string subdir{ "common/" };
	auto dir{ filesystem.iterate_directory(subdir) };

	std::vector<ShaderTypeSource> sources;

	for (auto resource : dir) {
		if (!resource.is_file()) {
			continue;
		}
		auto filename{ resource.filename() };
		auto file{ filesystem.open(subdir + filename) };
		std::string shader_src(file.begin(), file.end());
		std::string name_without_ext{ std::filesystem::path(filename).stem().string() };
		auto srcs{ ParseShader(shader_src, name_without_ext) };
		sources.insert(sources.end(), srcs.begin(), srcs.end());
	}

	SubstituteShaderTokens(sources, max_texture_slots);
	CompileShaders(sources, vertex_shaders, fragment_shaders, batchable_shaders);
}

static json GetShaderManifest(const cmrc::embedded_filesystem& fs) {
	std::string manifest_name{ "manifest.json" };

	PTGN_ASSERT(
		fs.exists(manifest_name), "Could not find shader manifest file with name: ", manifest_name
	);
	auto manifest_file{ fs.open(manifest_name) };

	std::string_view manifest_data(manifest_file.begin(), manifest_file.end());

	json manifest = json::parse(manifest_data);

	// PTGN_LOG("--------- Manifest Name ----------");
	// PTGN_LOG(manifest_name);
	// PTGN_LOG("-------- Manifest Content -------");
	// PTGN_LOG(manifest.dump(4));
	return manifest;
}

GLuint GLContext::CompileShaderFromSource(GLenum type, const std::string& source) const {
	GLuint id{ GLCallReturn(::CreateShader(type)) };

	auto src{ source.c_str() };

	GLCall(ShaderSource(id, 1, &src, nullptr));
	GLCall(::CompileShader(id));

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

		PTGN_ERROR("Failed to compile ", type, " shader: \n", source, "\n", log);
	}

	return id;
}

void GLContext::PopulateShadersFromCache(const json& manifest) {
	std::vector<GLuint> batchable_shaders;

	for (const auto& [shader_name, shader_object] : manifest.items()) {
		std::string vertex_name;
		std::string fragment_name;

		if (shader_object.contains("vertex") && shader_object.contains("fragment")) {
			vertex_name	  = shader_object.at("vertex").get<std::string>();
			fragment_name = shader_object.at("fragment").get<std::string>();
		} else if (shader_object.contains("source")) {
			auto name	  = shader_object.at("source").get<std::string>();
			vertex_name	  = name;
			fragment_name = name;
		} else {
			PTGN_ERROR(
				"Manifest shader ", shader_name,
				" must specify either a 'vertex' and 'fragment' property for individual "
				"specification, or a combined 'source' "
				"property for same-name vertex/fragment shaders"
			);
		}

		auto vert_hash{ Hash(vertex_name) };
		auto frag_hash{ Hash(fragment_name) };

		PTGN_ASSERT(
			vertex_shaders_.contains(vert_hash), "Vertex shader: ", vertex_name, " for ",
			shader_name, " not found in shader directory"
		);

		PTGN_ASSERT(
			fragment_shaders_.contains(frag_hash), "Fragment shader: ", fragment_name, " for ",
			shader_name, " not found in shader directory"
		);

		auto vert_id{ vertex_shaders_.find(vert_hash)->second };
		auto frag_id{ fragment_shaders_.find(frag_hash)->second };

		auto hash{ Hash(shader_name) };

		PTGN_ASSERT(!shaders_.contains(hash), "Shader names in the manifest must be unique");

		auto shader{ CreateShaderImpl(shader_name) };

		LinkShader(shader, vert_id, frag_id);

		PTGN_ASSERT(
			!VectorContains(batchable_shaders_, vert_id),
			"Vertex shaders cannot use the 'batchable' option"
		);

		if (VectorContains(batchable_shaders_, frag_id)) {
			batchable_shaders.push_back(shader);
		}

		shaders_.emplace(hash, std::move(shader));
	}

	// batchable_shaders_ goes from containing fragment shader ids to shader program ids.
	batchable_shaders_ = batchable_shaders;
}

std::vector<ShaderTypeSource> GLContext::ParseShaderSourceFile(
	const std::string& source, const std::string& name, std::size_t max_texture_slots
) const {
	auto srcs{ ParseShader(source, name) };
	SubstituteShaderTokens(srcs, max_texture_slots);
	return srcs;
}

GLuint GLContext::CompileShaderSource(
	const std::string& source, GLenum type, const std::string& name, std::size_t max_texture_slots
) const {
	auto srcs{ ParseShaderSourceFile(source, name, max_texture_slots) };
	PTGN_ASSERT(srcs.size() == 1, "Wrong constructor for a multi-source shader file");
	const auto& front{ srcs.front() };
	PTGN_ASSERT(front.type == type, "Shader type mismatch");
	return CompileShaderFromSource(type, front.source.source);
}

GLuint GLContext::CompileShaderPath(
	const path& shader_path, GLenum type, const std::string& name, std::size_t max_texture_slots
) const {
	PTGN_ASSERT(
		FileExists(shader_path),
		"Cannot create shader from nonexistent shader path: ", shader_path.string()
	);
	auto source{ FileToString(shader_path) };
	return CompileShaderSource(source, type, name, max_texture_slots);
}

void GLContext::LinkShader(GLuint id, GLuint vertex, GLuint fragment) {
	shader_cache_.Get(id).uniform_locations.clear();

	PTGN_ASSERT(vertex);
	PTGN_ASSERT(fragment);

	GLCall(AttachShader(id, vertex));
	GLCall(AttachShader(id, fragment));
	GLCall(LinkProgram(id));

	// Check for shader link errors.
	std::int32_t linked{ GL_FALSE };
	GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

	if (linked == GL_FALSE) {
		std::int32_t length{ 0 };
		GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
		std::string log;
		log.resize(static_cast<std::size_t>(length));
		GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

		GLCall(DeleteProgram(id));

		GLCall(DeleteShader(vertex));
		GLCall(DeleteShader(fragment));

		PTGN_ERROR(
			"Failed to link shaders to program:\nVertex : ", vertex, "\nFragment : ", fragment,
			"\n ", log
		);
	}

	GLCall(ValidateProgram(id));
}

void GLContext::CompileShader(
	GLuint id, const std::string& vertex_source, const std::string& fragment_source
) const {
	// TODO: Ensure shader cache is cleared if it exists.

	GLuint vertex{ CompileShaderFromSource(GL_VERTEX_SHADER, vertex_source) };
	GLuint fragment{ CompileShaderFromSource(GL_FRAGMENT_SHADER, fragment_source) };

	if (vertex && fragment) {
		GLCall(AttachShader(id, vertex));
		GLCall(AttachShader(id, fragment));
		GLCall(LinkProgram(id));

		// Check for shader link errors.
		std::int32_t linked{ GL_FALSE };
		GLCall(GetProgramiv(id, GL_LINK_STATUS, &linked));

		if (linked == GL_FALSE) {
			std::int32_t length{ 0 };
			GLCall(GetProgramiv(id, GL_INFO_LOG_LENGTH, &length));
			std::string log;
			log.resize(static_cast<std::size_t>(length));
			GLCall(GetProgramInfoLog(id, length, &length, &log[0]));

			GLCall(DeleteProgram(id));

			GLCall(DeleteShader(vertex));
			GLCall(DeleteShader(fragment));

			PTGN_ERROR(
				"Failed to link shaders to program: \n", vertex_source, "\n", fragment_source, "\n",
				log
			);
		}

		GLCall(ValidateProgram(id));
	}

	if (vertex) {
		GLCall(DeleteShader(vertex));
	}

	if (fragment) {
		GLCall(DeleteShader(fragment));
	}
}

GLContext::GLContext(Window& window) {
	if (context_ != nullptr) {
		int result = SDL_GL_MakeCurrent(window, context_);
		PTGN_ASSERT(!result, SDL_GetError());
		return;
	}

	context_ = SDL_GL_CreateContext(window);
	PTGN_ASSERT(context_, SDL_GetError());

	GLVersion gl_version;

	PTGN_INFO("Initialized OpenGL version: ", gl_version);
	PTGN_INFO("Created OpenGL context");

	// From: https://nullprogram.com/blog/2023/01/08/
	// Set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes.
	if (!SDL_GL_SetSwapInterval(PTGN_VSYNC_MODE)) {
		// If no adaptive VSYNC available, fallback to VSYNC.
		SDL_GL_SetSwapInterval(1);
	}

	LoadGLFunctions();

	// PTGN_LOG("OpenGL Build: ", GLCall(glGetString(GL_VERSION)));

	auto max_texture_slots{ GetInteger<GLuint>(GL_MAX_TEXTURE_IMAGE_UNITS) };
	PTGN_ASSERT(max_texture_slots > 0);
	bound_.texture_units.resize(max_texture_slots, {});

	auto fs{ cmrc::shader::get_filesystem() };

	PopulateShaderCache(
		fs, vertex_shaders_, fragment_shaders_, batchable_shaders_, max_texture_slots
	);

	auto manifest = GetShaderManifest(fs);

	PopulateShadersFromCache(manifest);
}

GLContext::~GLContext() {
	const auto delete_shaders = [](const auto& container) {
		for (const auto& [hash, id] : container) {
			if (id) {
				GLCall(DeleteShader(id));
			}
		}
	};

	// Delete cached vertex and fragment shaders.
	delete_shaders(vertex_shaders_);
	delete_shaders(fragment_shaders_);

	// Must be done before deleting context.
	shaders_.clear();

	if (context_) {
		SDL_GL_DestroyContext(context_);
		context_ = nullptr;
		PTGN_INFO("Destroyed OpenGL context");
		// Note: If this is the last message you see and the window does not close, it is likely
		// that a GL asset is destructed after the GL context has been deleted.
	}
}

StrongGLHandle<VertexBuffer> GLContext::CreateVertexBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
) {
	return CreateBufferImpl<VertexBuffer>(
		GL_ARRAY_BUFFER, data, element_count, element_size, usage
	);
}

StrongGLHandle<ElementBuffer> GLContext::CreateElementBuffer(
	const void* data, std::uint32_t element_count, std::uint32_t element_size, GLenum usage
) {
	return CreateBufferImpl<ElementBuffer>(
		GL_ELEMENT_ARRAY_BUFFER, data, element_count, element_size, usage
	);
}

StrongGLHandle<UniformBuffer> GLContext::CreateUniformBuffer(
	const void* data, std::uint32_t size, GLenum usage
) {
	return CreateBufferImpl<UniformBuffer>(GL_UNIFORM_BUFFER, data, size, 1, usage);
}

StrongGLHandle<Shader> GLContext::CreateShader(
	GLuint vertex, GLuint fragment, const std::string& shader_name
) {
	auto shader{ CreateShaderImpl(shader_name) };

	LinkShader(shader, vertex, fragment);

	return shader;
}

StrongGLHandle<Shader> GLContext::CreateShader(
	std::variant<ShaderCode, std::string> vertex, std::variant<ShaderCode, std::string> fragment,
	const std::string& shader_name
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

	auto max_texture_slots{ GetMaxTextureSlots() };

	// @return { id, bool }: If true, delete shader id after.
	const auto get_id = [&get, &has, this, shader_name, max_texture_slots](
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
				PTGN_ERROR(name, " is not a valid shader path or loaded ", type, " shader name");
			}
		} else if (std::holds_alternative<ShaderCode>(v)) {
			const auto& src{ std::get<ShaderCode>(v) };
			return { CompileShaderSource(src.source, type, shader_name, max_texture_slots), true };
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

StrongGLHandle<Shader> GLContext::CreateShader(
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

	const auto max_texture_slots{ GetMaxTextureSlots() };

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

bool GLContext::IsBatchableShader(GLuint shader) const {
	PTGN_ASSERT(
		ValuesContain(shaders_, shader), "Shader id must be in cache to check if it is batchable"
	);

	return VectorContains(batchable_shaders_, shader);
}

void GLContext::AttachTexture(GLuint framebuffer, GLuint texture, GLenum texture_attachment) {
	PTGN_ASSERT(
		IsBound<FrameBuffer>(framebuffer), "Framebuffer must be bound before attaching a texture"
	);

	if (texture) {
		PTGN_ASSERT(texture_cache_.Has(texture), "Texture not in cache");
		PTGN_ASSERT(
			texture_cache_.Get(texture).size.BothAboveZero(), "Cannot attach a texture with no size"
		);
	}

	GLCall(FramebufferTexture2D(GL_FRAMEBUFFER, texture_attachment, GL_TEXTURE_2D, texture, 0));

	UpdateFrameBufferCache(framebuffer, texture, texture_attachment, GL_TEXTURE_2D);
}

V2_int GLContext::GetTextureSize(GLuint texture) const {
	PTGN_ASSERT(texture_cache_.Has(texture), "Texture not in cache");
	return texture_cache_.Get(texture).size;
}

void GLContext::AttachRenderBuffer(
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

void GLContext::SetVertexBuffer(GLuint vertex_array, GLuint vertex_buffer) {
	PTGN_ASSERT(
		IsBound<VertexArray>(vertex_array),
		"Vertex array must be bound before setting vertex buffer"
	);

	auto _ = Bind<VertexBuffer, false>(vertex_buffer);
}

void GLContext::SetElementBuffer(GLuint vertex_array, GLuint element_buffer) {
	PTGN_ASSERT(
		IsBound<VertexArray>(vertex_array),
		"Vertex array must be bound before setting element buffer"
	);

	auto _ = Bind<ElementBuffer, false>(element_buffer);
}

void GLContext::EnableGammaCorrection() {
#ifndef __EMSCRIPTEN__
	GLCall(glEnable(GL_FRAMEBUFFER_SRGB));
#else
	PTGN_WARN("glEnable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
}

void GLContext::DisableGammaCorrection() {
#ifndef __EMSCRIPTEN__
	GLCall(glDisable(GL_FRAMEBUFFER_SRGB));
#else
	PTGN_WARN("glDisable(GL_FRAMEBUFFER_SRGB) not supported by Emscripten");
#endif
}

void GLContext::SetDepthMask(GLboolean enabled) {
	if (bound_.depth.write == enabled) {
		return;
	}
	GLCall(glDepthMask(enabled));
	bound_.depth.write = enabled;
}

void GLContext::SetBlending(GLboolean enabled) {
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

void GLContext::SetDepthFunc(GLenum depth_func) {
	if (bound_.depth.func == depth_func) {
		return;
	}
	GLCall(glDepthFunc(depth_func));
	bound_.depth.func = depth_func;
}

void GLContext::SetDepthTesting(GLboolean enabled) {
	if (enabled) {
		SetBlending(GL_FALSE);
	}
	if (bound_.depth.test == enabled) {
		return;
	}
	if (enabled) {
		GLCall(glClearDepth(1.0));
		GLCall(glEnable(GL_DEPTH_TEST));
	} else {
		GLCall(glDisable(GL_DEPTH_TEST));
	}
	bound_.depth.test = enabled;
}

void GLContext::SetDepthRange(float near_val, float far_val) {
	if (NearlyEqual(bound_.depth.range_near, near_val) &&
		NearlyEqual(bound_.depth.range_far, far_val)) {
		return;
	}
	GLCall(glDepthRange(near_val, far_val));
	bound_.depth.range_near = near_val;
	bound_.depth.range_far	= far_val;
}

void GLContext::SetLineWidth(float width) {
	if (bound_.line_width == width) {
		return;
	}
	GLCall(glLineWidth(width));
	bound_.line_width = width;
}

void GLContext::SetLineSmoothing(bool enabled) {
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

void GLContext::SetPolygonMode(GLenum front_mode, GLenum back_mode) {
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

void GLContext::SetBlendMode(BlendMode mode) {
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
			MultiplyRGBAWithAlphaBlend, GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ZERO
		)
		default: PTGN_ERROR("Failed to identify blend mode");
	}

	bound_.blend_mode = mode;
}

void GLContext::DrawElements(
	GLuint vertex_array, GLsizei element_count, GLenum element_type, GLenum primitive_mode
) const {
	PTGN_ASSERT(IsBound<VertexArray>(vertex_array));
	PTGN_ASSERT(vertex_array_cache_.Get(vertex_array).layout_set);
	PTGN_ASSERT(GetBound<ElementBuffer>());

	GLCall(glDrawElements(primitive_mode, element_count, element_type, nullptr));
}

void GLContext::DrawArrays(GLuint vertex_array, GLsizei vertex_count, GLenum primitive_mode) const {
	PTGN_ASSERT(IsBound<VertexArray>(vertex_array));
	PTGN_ASSERT(vertex_array_cache_.Get(vertex_array).layout_set);

	constexpr GLint starting_index{ 0 };
	GLCall(glDrawArrays(primitive_mode, starting_index, vertex_count));
}

void GLContext::SetViewport(const Viewport& viewport) {
	if (bound_.viewport == viewport) {
		return;
	}
	GLCall(glViewport(viewport.position.x, viewport.position.y, viewport.size.x, viewport.size.y));
	bound_.viewport = viewport;
}

Viewport GLContext::GetViewport() const {
	return bound_.viewport;
}

void GLContext::SetClearColor(Color color) {
	if (bound_.clear_color == color) {
		return;
	}
	auto n{ static_cast<V4_float>(color) };
	GLCall(glClearColor(n.x, n.y, n.z, n.w));
	bound_.clear_color = color;
}

void GLContext::Clear() {
	GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

void GLContext::ClearToColor(GLuint framebuffer, Color color) const {
	PTGN_ASSERT(IsBound<FrameBuffer>(framebuffer));
	auto c{ static_cast<V4_float>(color) };
	GLCall(ClearBufferfv(GL_COLOR, 0, c.Data()));
}

void GLContext::SetColorMask(const ColorMaskState& mask) {
	if (bound_.color_mask == mask) {
		return;
	}
	GLCall(glColorMask(mask.red, mask.green, mask.blue, mask.alpha));
	bound_.color_mask = mask;
}

void GLContext::SetScissor(const ScissorState& scissor) {
	if (bound_.scissor == scissor) {
		return;
	}

	if (scissor.enabled) {
		GLCall(glEnable(GL_SCISSOR_TEST));
		GLCall(glScissor(scissor.position.x, scissor.position.y, scissor.size.x, scissor.size.y));
	} else {
		GLCall(glDisable(GL_SCISSOR_TEST));
	}

	bound_.scissor = scissor;
}

void GLContext::SetCull(const CullState& cull) {
	if (bound_.cull == cull) {
		return;
	}

	if (cull.enabled) {
		GLCall(glEnable(GL_CULL_FACE));
	} else {
		GLCall(glDisable(GL_CULL_FACE));
	}

	GLCall(glCullFace(cull.cull_face));
	GLCall(glFrontFace(cull.front_face));

	bound_.cull = cull;
}

void GLContext::SetStencil(const StencilState& stencil) {
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

void GLContext::SetUniform(GLuint shader, const char* uniform_name, V2_float v) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v.x, v.y));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, V3_float v) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v.x, v.y, v.z));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, V4_float v) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v.x, v.y, v.z, v.w));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, const Matrix4& matrix) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(UniformMatrix4fv(location, 1, GL_FALSE, matrix.Data()));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, const std::int32_t* data, std::int32_t count
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1iv(location, count, data));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, const float* data, std::int32_t count
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1fv(location, count, data));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, const Vector2<std::int32_t>& v
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v.x, v.y));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, const Vector3<std::int32_t>& v
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v.x, v.y, v.z));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, const Vector4<std::int32_t>& v
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v.x, v.y, v.z, v.w));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, float v0) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1f(location, v0));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, float v0, float v1) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2f(location, v0, v1));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, float v0, float v1, float v2) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3f(location, v0, v1, v2));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, float v0, float v1, float v2, float v3
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4f(location, v0, v1, v2, v3));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, std::int32_t v0) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform1i(location, v0));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform2i(location, v0, v1));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform3i(location, v0, v1, v2));
	}
}

void GLContext::SetUniform(
	GLuint shader, const char* uniform_name, std::int32_t v0, std::int32_t v1, std::int32_t v2,
	std::int32_t v3
) {
	std::int32_t location{ GetUniform(shader, uniform_name) };
	if (location != -1) {
		GLCall(Uniform4i(location, v0, v1, v2, v3));
	}
}

void GLContext::SetUniform(GLuint shader, const char* uniform_name, bool value) {
	SetUniform(shader, uniform_name, static_cast<std::int32_t>(value));
}

StrongGLHandle<Shader> GLContext::GetShader(std::string_view shader_name) const {
	auto key{ Hash(shader_name) };
	PTGN_ASSERT(shaders_.contains(key));
	return shaders_.find(key)->second;
}

void GLContext::SetActiveTextureSlot(GLuint slot) {
	if (bound_.active_texture_slot == slot) {
		return;
	}
	PTGN_ASSERT(
		slot < GetMaxTextureSlots(),
		"Attempting to bind a slot outside of OpenGL texture slot maximum"
	);
	GLCall(ActiveTexture(GL_TEXTURE0 + slot));
	bound_.active_texture_slot = slot;
}

std::size_t GLContext::GetMaxTextureSlots() const {
	return bound_.texture_units.size();
}

GLContext::PixelValue GLContext::ReadPixel(
	GLuint framebuffer, V2_int coordinate, GLenum attachment
) {
	auto _1 = Bind<FrameBuffer, true>(framebuffer);

	auto type		 = GetAttachmentDataType(attachment);
	const auto& info = GetFrameBufferAttachment(framebuffer, attachment);
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

		return Color{ v[0], v[1], v[2], components == 4 ? v[3] : static_cast<std::uint8_t>(255) };
	}

	if (type == AttachmentDataType::Depth) {
		float depth = 0.0f;
		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth));
		return depth;
	}

	if (type == AttachmentDataType::Stencil) {
		std::uint8_t stencil = 0;
		GLCall(
			glReadPixels(coordinate.x, read_y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil)
		);
		return stencil;
	}

	if (type == AttachmentDataType::DepthStencil) {
		// GL_DEPTH_STENCIL returns two integers: depth + stencil packed.
		struct {
			std::uint32_t depth;
			std::uint8_t stencil;
		} ds{};

		GLCall(glReadPixels(coordinate.x, read_y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, &ds)
		);

		float depth = (ds.depth & 0xFFFFFF) / float(0xFFFFFF);
		return std::make_pair(depth, ds.stencil);
	}

	PTGN_ERROR("Unhandled attachment type");
}

GLContext::PixelBuffer GLContext::ReadPixels(GLuint framebuffer, GLenum attachment) {
	auto type = GetAttachmentDataType(attachment);

	const auto& info = GetFrameBufferAttachment(framebuffer, attachment);
	PTGN_ASSERT(info.id != 0);

	auto _ = Bind<FrameBuffer, true>(framebuffer);

	V2_int size = (info.type == GL_TEXTURE_2D) ? texture_cache_.Get(info.id).size
											   : renderbuffer_cache_.Get(info.id).size;

	GLenum format	 = GL_RGBA;
	GLenum type_enum = GL_UNSIGNED_BYTE;

	switch (type) {
		using enum AttachmentDataType;

		case Color: {
			const auto& tex = texture_cache_.Get(info.id);
			PTGN_ASSERT(GetColorComponentCount(tex.internal_format) >= 3);
			format	  = tex.internal_format;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		}
		case Depth:
			format	  = GL_DEPTH_COMPONENT;
			type_enum = GL_FLOAT;
			break;
		case Stencil:
			format	  = GL_STENCIL_INDEX;
			type_enum = GL_UNSIGNED_BYTE;
			break;
		case DepthStencil:
			format	  = GL_DEPTH_STENCIL;
			type_enum = GL_UNSIGNED_INT_24_8;
			break;
	}

	// Allocate max possible size (RGBA8 worst case)
	std::vector<std::uint8_t> buffer(size.x * size.y * 4);

	GLCall(glReadPixels(0, 0, size.x, size.y, format, type_enum, buffer.data()));

	return PixelBuffer{ .size = size, .type = type, .data = std::move(buffer) };
}

bool GLContext::FrameBufferIsComplete(GLuint framebuffer) const {
	PTGN_ASSERT(
		IsBound<FrameBuffer>(framebuffer), "Cannot check status of framebuffer until it is bound"
	);
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	return status == GL_FRAMEBUFFER_COMPLETE;
}

const char* GLContext::GetFrameBufferStatus() {
	auto status{ GLCallReturn(CheckFramebufferStatus(GL_FRAMEBUFFER)) };
	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE:  return "Framebuffer is complete.";
		case GL_FRAMEBUFFER_UNDEFINED: return "Framebuffer is undefined (no framebuffer bound).";
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

GLContext::AttachmentDataType GLContext::GetAttachmentDataType(GLenum attachment) const {
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

AttachmentInfo& GLContext::GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment) {
	return const_cast<AttachmentInfo&>(
		std::as_const(*this).GetFrameBufferAttachment(framebuffer, attachment)
	);
}

const AttachmentInfo& GLContext::GetFrameBufferAttachment(GLenum framebuffer, GLenum attachment)
	const {
	const auto& cache = framebuffer_cache_.Get(framebuffer);

	if (attachment >= GL_COLOR_ATTACHMENT0 && attachment < GL_COLOR_ATTACHMENT0 + 8) {
		PTGN_ASSERT(
			attachment >= GL_COLOR_ATTACHMENT0 &&
				attachment < GL_COLOR_ATTACHMENT0 + cache.color.size(),
			"Color attachment out of valid range"
		);
		auto idx{ attachment - GL_COLOR_ATTACHMENT0 };
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

void GLContext::UpdateFrameBufferCache(
	GLenum framebuffer, GLuint image_id, GLenum attachment, GLenum image_type
) {
	PTGN_ASSERT(image_type == GL_TEXTURE_2D || image_type == GL_RENDERBUFFER, "Invalid image type");

	auto& cache = framebuffer_cache_.Get(framebuffer);

	auto& info{ GetFrameBufferAttachment(framebuffer, attachment) };
	info.id	  = image_id;
	info.type = image_id ? image_type : 0;
}

std::int32_t GLContext::GetUniform(GLuint shader, const char* name) {
	PTGN_ASSERT(
		IsBound<Shader>(shader),
		"Cannot get uniform location of shader which is not currently bound"
	);

	auto& cache{ shader_cache_.Get(shader) };

	auto hash{ Hash(name) };

	if (auto location{ cache.uniform_locations.find(hash) };
		location != cache.uniform_locations.end()) {
		return location->second;
	}

	std::int32_t location{ GLCallReturn(GetUniformLocation(shader, name)) };

	cache.uniform_locations.emplace(hash, location);

	return location;
}

void GLContext::ResizeFrameBuffer(GLuint framebuffer, V2_int new_size) {
	auto& cache = framebuffer_cache_.Get(framebuffer);

	auto resize_attachment = [&](const AttachmentInfo& info) {
		if (info.id == 0) {
			return;
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

void GLContext::ResizeRenderBuffer(GLuint renderbuffer, V2_int new_size) {
	PTGN_ASSERT(renderbuffer);

	const auto& cache = renderbuffer_cache_.Get(renderbuffer);

	if (cache.size == new_size) {
		return;
	}

	auto _ = Bind<RenderBuffer, true>(renderbuffer);

	SetRenderBufferStorage(renderbuffer, new_size, cache.internal_format);
}

void GLContext::ResizeTexture(GLuint texture, V2_int new_size) {
	PTGN_ASSERT(texture);

	const auto& cache = texture_cache_.Get(texture);

	if (cache.size == new_size) {
		return;
	}

	auto _ = Bind<Texture, true>(texture);

	SetTextureData(texture, nullptr, GL_RGBA, GL_UNSIGNED_BYTE, new_size, cache.internal_format);
}

void GLContext::SetRenderBufferStorage(GLuint renderbuffer, V2_int size, GLenum internal_format) {
	PTGN_ASSERT(
		IsBound<RenderBuffer>(renderbuffer),
		"Renderbuffer must be bound prior to setting its storage"
	);

	GLCall(RenderbufferStorage(GL_RENDERBUFFER, internal_format, size.x, size.y));

	auto& cache			  = renderbuffer_cache_.Get(renderbuffer);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

void GLContext::SetTextureData(
	GLuint texture, const void* pixel_data, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int size, GLenum internal_format
) {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its data");

	constexpr GLint mipmap_level{ 0 };
	constexpr GLint border{ 0 };

#ifdef __EMSCRIPTEN__
	PTGN_ASSERT(
		pixel_data_format != GL_BGRA && pixel_data_format != GL_BGR && internal_format != GL_BGRA &&
			internal_format != GL_BGR,
		"OpenGL ES3.0 does not support BGR(A) formats in glTexImage2D"
	);
#endif

	GLCall(glTexImage2D(
		GL_TEXTURE_2D, mipmap_level, internal_format, size.x, size.y, border, pixel_data_format,
		pixel_data_type, pixel_data
	));

	auto& cache			  = texture_cache_.Get(texture);
	cache.size			  = size;
	cache.internal_format = internal_format;
}

void GLContext::SetTextureSubData(
	GLuint texture, const void* pixel_subdata, GLenum pixel_data_format, GLenum pixel_data_type,
	V2_int subdata_size, V2_int subdata_offset
) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its subdata");
	PTGN_ASSERT(pixel_subdata != nullptr, "Cannot set texture subdata to nullptr");

	constexpr GLint mipmap_level{ 0 };

	GLCall(glTexSubImage2D(
		GL_TEXTURE_2D, mipmap_level, subdata_offset.x, subdata_offset.y, subdata_size.x,
		subdata_size.y, pixel_data_format, pixel_data_type, pixel_subdata
	));
}

void GLContext::SetTextureClampBorderColor(GLuint texture, Color color) const {
	PTGN_ASSERT(
		IsBound<Texture>(texture), "Texture must be bound prior to setting its clamp border color"
	);

	auto c{ static_cast<V4_float>(color) };
	SetTextureParameter(texture, GL_TEXTURE_BORDER_COLOR, c.Data());
}

void GLContext::SetTextureParameter(GLuint texture, GLenum param, const GLfloat* values) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameterfv(GL_TEXTURE_2D, param, values));
}

void GLContext::SetTextureParameter(GLuint texture, GLenum param, const GLint* values) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(values != nullptr, "Cannot set texture parameter values to nullptr");
	GLCall(glTexParameteriv(GL_TEXTURE_2D, param, values));
}

void GLContext::SetTextureParameter(GLuint texture, GLenum param, GLfloat value) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameterf(GL_TEXTURE_2D, param, value));
}

void GLContext::SetTextureParameter(GLuint texture, GLenum param, GLint value) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to setting its parameters");
	PTGN_ASSERT(value != -1, "Cannot set texture parameter value to -1");
	GLCall(glTexParameteri(GL_TEXTURE_2D, param, value));
}

GLint GLContext::GetTextureParameter(GLuint texture, GLenum param) const {
	PTGN_ASSERT(IsBound<Texture>(texture), "Texture must be bound prior to getting its parameters");
	GLint value{ -1 };
	GLCall(glGetTexParameteriv(GL_TEXTURE_2D, param, &value));
	PTGN_ASSERT(value != -1, "Failed to retrieve texture parameter");
	return value;
}

GLuint GLContext::GetActiveTextureSlot() const {
	return bound_.active_texture_slot;
}

bool GLContext::SupportsMipmaps(GLenum texture_min_filter) {
	return texture_min_filter == GL_LINEAR_MIPMAP_LINEAR ||
		   texture_min_filter == GL_LINEAR_MIPMAP_NEAREST ||
		   texture_min_filter == GL_NEAREST_MIPMAP_LINEAR ||
		   texture_min_filter == GL_NEAREST_MIPMAP_NEAREST;
}

void GLContext::GenerateMipmaps(GLuint texture) const {
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

StrongGLHandle<Shader> GLContext::CreateShaderImpl(const std::string& shader_name) {
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

StrongGLHandle<VertexArray> GLContext::CreateVertexArrayImpl() {
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

StrongGLHandle<FrameBuffer> GLContext::CreateFrameBufferImpl() {
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

StrongGLHandle<Texture> GLContext::CreateTextureImpl() {
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

StrongGLHandle<RenderBuffer> GLContext::CreateRenderBufferImpl() {
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

void GLContext::SavePNG(const path& path, GLuint framebuffer, GLenum attachment) {
	// Ensure output directory exists
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	// Read all pixels from the framebuffer attachment
	PixelBuffer pb = ReadPixels(framebuffer, attachment);

	PTGN_ASSERT(pb.type == AttachmentDataType::Color, "SavePNG only supports color attachments");

	const V2_int size	   = pb.size;
	constexpr int channels = 4;

	std::vector<std::uint8_t> rgba(static_cast<std::size_t>(size.x) * size.y * channels);

	// Convert PixelBuffer -> tightly packed RGBA8
	ForEachPixel(pb, [&rgba, size](V2_int pos, const PixelValue& px) {
		const Color* c = std::get_if<Color>(&px);
		PTGN_ASSERT(c != nullptr);

		const std::size_t idx = static_cast<std::size_t>(pos.y * size.x + pos.x) * channels;

		rgba[idx + 0] = c->r;
		rgba[idx + 1] = c->g;
		rgba[idx + 2] = c->b;
		rgba[idx + 3] = c->a;
	});

	SDL_Surface* surface = SDL_CreateSurfaceFrom(
		size.x, size.y, SDL_PIXELFORMAT_RGBA32, rgba.data(), size.x * channels
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	auto saved{ IMG_SavePNG(surface, path.string().c_str()) };

	PTGN_ASSERT(saved, SDL_GetError());

	SDL_DestroySurface(surface);
}

} // namespace ptgn::impl::gl
