#include "renderer/pipeline/shader_preprocessor.h"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/util/string.h"

namespace ptgn::impl {

namespace {

struct ParsedStage {
	ShaderStageMask stage{ ShaderStageMask::None };
	std::string source{};
};

struct InterfaceVariable {
	std::string interpolation{};
	std::string type{};
	std::string name{};
};

[[nodiscard]] std::optional<ShaderStageMask> ParseStage(std::string_view name) {
	if (name == "vertex") {
		return ShaderStageMask::Vertex;
	}
	if (name == "fragment") {
		return ShaderStageMask::Fragment;
	}
	return std::nullopt;
}

[[nodiscard]] std::expected<std::vector<ParsedStage>, std::string> ParseStages(
	std::string_view source
) {
	std::string input{ TrimRawStringLiteral(source) };
	std::regex marker{ R"(#type\s+(\w+))" };

	struct StageMarker {
		std::string type{};
		std::size_t begin{ 0 };
		std::size_t end{ 0 };
	};

	std::vector<StageMarker> markers;
	for (auto it{ std::sregex_iterator(input.begin(), input.end(), marker) };
		 it != std::sregex_iterator{}; ++it) {
		std::size_t begin{ static_cast<std::size_t>((*it).position()) };
		markers.push_back(StageMarker{
			.type = (*it)[1].str(),
			.begin = begin,
			.end = begin + static_cast<std::size_t>((*it).length()),
		});
	}

	if (markers.empty()) {
		return std::unexpected{ "No #type vertex or #type fragment declaration was found." };
	}

	std::string header{ TrimWhitespace(input.substr(0, markers.front().begin)) };
	std::vector<ParsedStage> stages;
	stages.reserve(markers.size());

	for (std::size_t i{ 0 }; i < markers.size(); ++i) {
		auto stage{ ParseStage(markers[i].type) };
		if (!stage.has_value()) {
			return std::unexpected{ "Unsupported shader stage: " + markers[i].type };
		}

		if (std::ranges::any_of(stages, [stage](const ParsedStage& parsed) {
				return parsed.stage == stage.value();
			})) {
			return std::unexpected{
				"A GLSL file may contain only one #type " +
				std::string{ ShaderStageName(stage.value()) } + " block."
			};
		}

		std::size_t start{ markers[i].end };
		std::size_t end{ i + 1 < markers.size() ? markers[i + 1].begin : input.size() };
		std::string body{ TrimWhitespace(input.substr(start, end - start)) };

		std::string combined;
		if (!header.empty()) {
			combined = header + '\n';
		}
		combined += std::move(body);

		stages.push_back(ParsedStage{
			.stage = stage.value(),
			.source = std::move(combined),
		});
	}

	return stages;
}

[[nodiscard]] bool HasOption(std::string_view source, std::string_view option) {
	return source.contains(std::string{ "#option " } + std::string{ option });
}

void RemoveOptions(std::string& source) {
	const std::regex option{
		R"(^\s*#option\s+\w+\s*\n?)",
		std::regex_constants::icase | std::regex_constants::multiline
	};
	source = std::regex_replace(source, option, "");
}

[[nodiscard]] bool ApplyAutoLayout(
	std::string& source,
	[[maybe_unused]] ShaderStageMask stage,
	std::string& error
) {
	std::istringstream input{ source };
	std::ostringstream output;
	std::string line;
	bool in_main{ false };
	int current_in_location{ 0 };
	int current_out_location{ 0 };

	const std::regex variable{
		R"(^\s*((?:flat|smooth|noperspective)\s+)?(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;\s*(//.*)?\r?$)"
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
		std::string interpolation{ match[1].str() };
		std::string qualifier{ match[2].str() };

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

		int location{ qualifier == "in" ? current_in_location++ : current_out_location++ };
		std::string comment{ match[5].str() };

		output << std::format(
			"layout(location = {}) {}{} {} {};{}\n",
			location,
			interpolation,
			qualifier,
			match[3].str(),
			match[4].str(),
			comment.empty() ? "" : " " + comment
		);
	}

	source = output.str();
	return true;
}

[[nodiscard]] bool InjectShaderPreamble(std::string& source, std::string& error) {
	const std::regex version_regex{ R"(#version\s+(\d+)(?:\s+(\w+))?)" };

	if (std::smatch match; std::regex_search(source, match, version_regex)) {
		std::string version_number{ match[1].str() };
		std::string version_profile{ match.size() > 2 ? match[2].str() : "" };

#ifdef __EMSCRIPTEN__
		if (version_number != "300" || version_profile != "es") {
			error = "For Emscripten, shader must specify '#version 300 es'.";
			return false;
		}
#else
		if (version_number != "330" || version_profile != "core") {
			error = "For desktop, shader must specify '#version 330 core'.";
			return false;
		}
#endif
	} else {
#ifdef __EMSCRIPTEN__
		source = "#version 300 es\n" + source;
#else
		source = "#version 330 core\n" + source;
#endif
	}

	std::size_t line_end{ source.find('\n') };
	std::size_t insert_position{ line_end == std::string::npos ? source.size() : line_end + 1 };

#ifdef __EMSCRIPTEN__
	if (!source.contains("precision ")) {
		source.insert(insert_position, "precision highp float;\n");
	}
#else
	if (!source.contains("#extension GL_ARB_separate_shader_objects")) {
		source.insert(insert_position, "#extension GL_ARB_separate_shader_objects : require\n");
	}
#endif

	return true;
}

[[nodiscard]] std::string TextureColorSwitch(std::size_t max_slots) {
	std::ostringstream output;
	for (std::size_t i{ 0 }; i < max_slots; ++i) {
		output << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n"
			"\t\ttexture_color *= texture({}[{}], v_TexCoord);\n"
			"\t}}\n",
			i,
			kTexturesUniform,
			i
		);
	}
	return output.str();
}

[[nodiscard]] std::string TextureSizeSwitch(std::size_t max_slots) {
	std::ostringstream output;
	for (std::size_t i{ 0 }; i < max_slots; ++i) {
		output << std::format(
			"\tif (v_TexIndex == {}.0f) {{\n"
			"\t\ttexture_size = vec2(textureSize({}[{}], 0));\n"
			"\t}}\n",
			i,
			kTexturesUniform,
			i
		);
	}
	return output.str();
}

[[nodiscard]] std::expected<std::string, std::string> PrepareStage(
	std::string source,
	ShaderStageMask stage,
	std::size_t max_texture_slots
) {
	std::string error;
	if (HasOption(source, "auto_layout") && !ApplyAutoLayout(source, stage, error)) {
		return std::unexpected{ std::move(error) };
	}

	RemoveOptions(source);

	if (!InjectShaderPreamble(source, error)) {
		return std::unexpected{ std::move(error) };
	}

	std::size_t slots{ std::max<std::size_t>(1, max_texture_slots) };
	std::string slots_string{ std::to_string(slots) };
	std::string color_switch{ TextureColorSwitch(slots) };
	std::string size_switch{ TextureSizeSwitch(slots) };

	source = ReplaceAll(source, "{MAX_TEXTURE_SLOTS}", slots_string);
	source = ReplaceAll(source, "{TEXTURE_COLOR_SWITCH_BLOCK}", color_switch);
	source = ReplaceAll(source, "{TEXTURE_SIZE_SWITCH_BLOCK}", size_switch);
	return source;
}

[[nodiscard]] std::vector<InterfaceVariable> ParseInterfaceVariables(
	std::string_view source,
	std::string_view qualifier
) {
	std::vector<InterfaceVariable> variables;
	std::istringstream input{ std::string{ source } };
	std::string line;
	bool in_main{ false };
	const std::regex variable{
		R"(^\s*(?:layout\s*\([^)]*\)\s*)?((?:flat|smooth|noperspective)\s+)?(in|out)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:\[[^\]]+\])?\s*;\s*(?://.*)?\r?$)"
	};
	std::smatch match;

	while (std::getline(input, line)) {
		if (!in_main && line.contains("void main")) {
			in_main = true;
		}
		if (in_main || !std::regex_match(line, match, variable) || match[2].str() != qualifier) {
			continue;
		}

		std::string interpolation{ TrimWhitespace(match[1].str()) };
		if (interpolation.empty()) {
			interpolation = "smooth";
		}

		variables.push_back(InterfaceVariable{
			.interpolation = std::move(interpolation),
			.type = match[3].str(),
			.name = match[4].str(),
		});
	}

	return variables;
}

[[nodiscard]] std::optional<std::string> ParsedStageSource(
	std::string_view source,
	ShaderStageMask requested
) {
	auto stages{ ParseStages(source) };
	if (!stages.has_value()) {
		return std::nullopt;
	}

	for (auto& stage : stages.value()) {
		if (stage.stage == requested) {
			return std::move(stage.source);
		}
	}
	return std::nullopt;
}

} // namespace

std::string_view ShaderStageName(ShaderStageMask stage) {
	if (stage == ShaderStageMask::Vertex) {
		return "vertex";
	}
	if (stage == ShaderStageMask::Fragment) {
		return "fragment";
	}
	return "unknown";
}

ShaderPreprocessResult PrepareShaderSource(
	std::string_view source,
	std::size_t max_texture_slots
) {
	auto parsed{ ParseStages(source) };
	if (!parsed.has_value()) {
		return std::unexpected{ parsed.error() };
	}

	std::vector<PreparedShaderStage> result;
	result.reserve(parsed->size());

	for (auto& stage : parsed.value()) {
		auto prepared{ PrepareStage(std::move(stage.source), stage.stage, max_texture_slots) };
		if (!prepared.has_value()) {
			return std::unexpected{
				"[" + std::string{ ShaderStageName(stage.stage) } + "]\n" + prepared.error()
			};
		}

		result.push_back(PreparedShaderStage{
			.stage = stage.stage,
			.source = std::move(prepared.value()),
		});
	}

	return result;
}

std::string ExtractShaderStageSource(std::string_view source, ShaderStageMask requested) {
	auto parsed{ ParseStages(source) };
	if (!parsed.has_value()) {
		return {};
	}

	for (const auto& stage : parsed.value()) {
		if (stage.stage == requested) {
			return std::string{ "#type " } + std::string{ ShaderStageName(requested) } + "\n" +
				   stage.source;
		}
	}
	return {};
}

int ShaderStageCompatibilityScore(
	std::string_view vertex_source,
	std::string_view fragment_source
) {
	auto vertex{ ParsedStageSource(vertex_source, ShaderStageMask::Vertex) };
	auto fragment{ ParsedStageSource(fragment_source, ShaderStageMask::Fragment) };
	if (!vertex.has_value() || !fragment.has_value()) {
		return -1;
	}

	auto outputs{ ParseInterfaceVariables(vertex.value(), "out") };
	auto inputs{ ParseInterfaceVariables(fragment.value(), "in") };
	int score{ 1000 };

	for (const auto& input : inputs) {
		auto it{ std::ranges::find_if(outputs, [&](const InterfaceVariable& output) {
			return output.name == input.name;
		}) };
		if (it == outputs.end() || it->type != input.type || it->interpolation != input.interpolation) {
			return -1;
		}
		score += 100;
	}

	std::size_t extra_outputs{ outputs.size() - std::min(outputs.size(), inputs.size()) };
	score -= static_cast<int>(std::min<std::size_t>(extra_outputs, 999));
	return score;
}

} // namespace ptgn::impl
