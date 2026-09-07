#include "runtime/graphics/text/text.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/draw_context.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

void UpdateLayout(
	Text text, AssetManager& asset_manager, const StyledText& styled_text, TextBox box
) {
	auto origin{ text.TryGet<Origin>() };

	Alignment fallback{ origin ? GetAlignment(*origin) : Alignment{} };

	PTGN_ASSERT(fallback.horizontal.has_value(), "Fallback horizontal alignment must have a value");
	PTGN_ASSERT(fallback.vertical.has_value(), "Fallback vertical alignment must have a value");

	if (!box.style.alignment.horizontal.has_value()) {
		box.style.alignment.horizontal = fallback.horizontal.value();
	}

	if (!box.style.alignment.vertical.has_value()) {
		box.style.alignment.vertical = fallback.vertical.value();
	}

	if (auto layout{ text.TryGet<TextLayout>() };
		layout && !layout->dirty && layout->built_alignment == box.style.alignment) {
		return;
	}

	text.Add<TextLayout>(impl::BuildTextLayout(asset_manager, styled_text, box));
}

} // namespace

namespace {

struct RichTextState {
	FontKey font{ kDefaultFont };
	TextRunStyle style{};
};

struct RichTextStackEntry {
	std::string tag{};
	RichTextState previous{};
	std::size_t position{};
};

[[nodiscard]] std::string_view TrimRichTextToken(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return value;
}

[[nodiscard]] std::string LowerRichTextToken(std::string_view value) {
	std::string result{ value };
	std::ranges::transform(result, result.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return result;
}

[[nodiscard]] std::vector<std::string_view> SplitRichTextArguments(std::string_view value) {
	std::vector<std::string_view> result;
	while (true) {
		auto comma{ value.find(',') };
		if (comma == std::string_view::npos) {
			result.emplace_back(TrimRichTextToken(value));
			break;
		}
		result.emplace_back(TrimRichTextToken(value.substr(0, comma)));
		value.remove_prefix(comma + 1);
	}
	return result;
}

[[nodiscard]] bool ParseRichFloat(std::string_view value, float& result) {
	value = TrimRichTextToken(value);
	if (value.empty()) {
		return false;
	}

	const auto* begin{ value.data() };
	const auto* end{ value.data() + value.size() };
	auto parsed{ std::from_chars(begin, end, result) };
	return parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(result);
}

[[nodiscard]] bool ParseHexNibble(char c, std::uint8_t& value) {
	if (c >= '0' && c <= '9') {
		value = static_cast<std::uint8_t>(c - '0');
		return true;
	}
	if (c >= 'a' && c <= 'f') {
		value = static_cast<std::uint8_t>(10 + c - 'a');
		return true;
	}
	if (c >= 'A' && c <= 'F') {
		value = static_cast<std::uint8_t>(10 + c - 'A');
		return true;
	}
	return false;
}

[[nodiscard]] bool ParseHexByte(std::string_view value, std::uint8_t& result) {
	if (value.size() != 2) {
		return false;
	}
	std::uint8_t high{};
	std::uint8_t low{};
	if (!ParseHexNibble(value[0], high) || !ParseHexNibble(value[1], low)) {
		return false;
	}
	result = static_cast<std::uint8_t>((high << 4) | low);
	return true;
}

[[nodiscard]] bool ParseRichColor(std::string_view value, Color& result) {
	value = TrimRichTextToken(value);
	if (value.empty()) {
		return false;
	}

	if (value.front() == '#') {
		value.remove_prefix(1);
		if (value.size() != 6 && value.size() != 8) {
			return false;
		}

		std::uint8_t r{};
		std::uint8_t g{};
		std::uint8_t b{};
		std::uint8_t a{ 255 };
		if (!ParseHexByte(value.substr(0, 2), r) || !ParseHexByte(value.substr(2, 2), g) ||
			!ParseHexByte(value.substr(4, 2), b) ||
			(value.size() == 8 && !ParseHexByte(value.substr(6, 2), a))) {
			return false;
		}
		result = Color{ r, g, b, a };
		return true;
	}

	const std::string name{ LowerRichTextToken(value) };
	if (name == "white") result = color::White;
	else if (name == "black") result = color::Black;
	else if (name == "red") result = color::Red;
	else if (name == "green") result = color::Green;
	else if (name == "blue") result = color::Blue;
	else if (name == "yellow") result = color::Yellow;
	else if (name == "cyan") result = color::Cyan;
	else if (name == "magenta") result = color::Magenta;
	else if (name == "gray" || name == "grey") result = color::Gray;
	else if (name == "orange") result = Color{ 255, 165, 0, 255 };
	else if (name == "purple") result = Color{ 128, 0, 128, 255 };
	else if (name == "pink") result = Color{ 255, 105, 180, 255 };
	else if (name == "transparent") result = color::Transparent;
	else return false;

	return true;
}

[[nodiscard]] bool IsOffRichTextValue(std::string_view value) {
	const std::string lowered{ LowerRichTextToken(TrimRichTextToken(value)) };
	return lowered == "off" || lowered == "false" || lowered == "0" || lowered == "none";
}

[[nodiscard]] bool ParseGlyphEffectType(std::string_view value, GlyphEffectType& result) {
	const std::string name{ LowerRichTextToken(TrimRichTextToken(value)) };
	if (name == "none") result = GlyphEffectType::None;
	else if (name == "wobble") result = GlyphEffectType::Wobble;
	else if (name == "wave") result = GlyphEffectType::Wave;
	else if (name == "shake") result = GlyphEffectType::Shake;
	else if (name == "pulse") result = GlyphEffectType::Pulse;
	else return false;
	return true;
}

[[nodiscard]] std::string CanonicalRichTag(std::string_view tag) {
	std::string result{ LowerRichTextToken(TrimRichTextToken(tag)) };
	if (result == "strike") {
		result = "s";
	} else if (result == "color") {
		result = "c";
	}
	return result;
}

[[nodiscard]] bool ApplyRichTextOpenTag(
	std::string_view raw_name, std::optional<std::string_view> argument, RichTextState& state,
	std::string& canonical_tag, std::string& error
) {
	canonical_tag = CanonicalRichTag(raw_name);
	const auto arg{ argument ? TrimRichTextToken(*argument) : std::string_view{} };
	const TextRunDefaults engine_defaults{};

	// Value-bearing tags require an equals sign. An empty assignment is intentionally
	// different from a missing assignment: <size=> means the engine default size,
	// while <size> is invalid.
	auto require_assignment = [&]() {
		if (!argument) {
			error = "Tag <" + canonical_tag + "> requires '=value'.";
			return false;
		}
		return true;
	};

	if (canonical_tag == "b") {
		if (!argument || arg.empty()) {
			state.style.flags = SetFontFlag(state.style.flags, FontStyle::Bold, true);
			state.style.bold_weight = engine_defaults.style.bold_weight;
			return true;
		}
		if (IsOffRichTextValue(arg)) {
			state.style.flags = SetFontFlag(state.style.flags, FontStyle::Bold, false);
			return true;
		}
		float weight{};
		if (!ParseRichFloat(arg, weight)) {
			error = "Invalid bold weight. Use <b>, <b=>, <b=weight>, or <b=off>.";
			return false;
		}
		state.style.flags = SetFontFlag(state.style.flags, FontStyle::Bold, true);
		state.style.bold_weight = weight;
		return true;
	}

	if (canonical_tag == "i" || canonical_tag == "u" || canonical_tag == "s") {
		bool enabled{ true };
		if (argument && !arg.empty()) {
			if (!IsOffRichTextValue(arg)) {
				error = "Tag <" + canonical_tag + "> only accepts '=off' when a value is supplied.";
				return false;
			}
			enabled = false;
		}

		FontStyle flag{ FontStyle::Italic };
		if (canonical_tag == "u") flag = FontStyle::Underline;
		if (canonical_tag == "s") flag = FontStyle::Strikethrough;
		state.style.flags = SetFontFlag(state.style.flags, flag, enabled);
		return true;
	}

	if (canonical_tag == "c") {
		if (!require_assignment()) return false;
		if (arg.empty()) {
			state.style.color = engine_defaults.style.color;
			return true;
		}
		Color parsed{};
		if (!ParseRichColor(arg, parsed)) {
			error = "Invalid color. Use a named color, #RRGGBB or #RRGGBBAA.";
			return false;
		}
		state.style.color = parsed;
		return true;
	}

	if (canonical_tag == "font") {
		if (!require_assignment()) return false;
		state.font = arg.empty() ? engine_defaults.font : FontKey{ std::string{ arg } };
		return true;
	}

	auto parse_single_float = [&](
		float& destination, float engine_default, std::string_view label
	) {
		if (!require_assignment()) return false;
		if (arg.empty()) {
			destination = engine_default;
			return true;
		}
		float parsed{};
		if (!ParseRichFloat(arg, parsed)) {
			error = "Invalid " + std::string{ label } + ".";
			return false;
		}
		destination = parsed;
		return true;
	};

	if (canonical_tag == "size") {
		return parse_single_float(state.style.size, engine_defaults.style.size, "font size");
	}
	if (canonical_tag == "kern") {
		return parse_single_float(state.style.kerning, engine_defaults.style.kerning, "kerning");
	}
	if (canonical_tag == "track") {
		return parse_single_float(state.style.tracking, engine_defaults.style.tracking, "tracking");
	}
	if (canonical_tag == "line") {
		return parse_single_float(
			state.style.line_spacing, engine_defaults.style.line_spacing, "line spacing"
		);
	}

	if (canonical_tag == "outline" || canonical_tag == "outerglow" || canonical_tag == "innerglow") {
		if (!require_assignment()) return false;
		DistanceFieldLayerStyle* layer{ nullptr };
		const DistanceFieldLayerStyle* engine_layer{ nullptr };
		if (canonical_tag == "outline") {
			layer = &state.style.sdf.outline;
			engine_layer = &engine_defaults.style.sdf.outline;
		}
		if (canonical_tag == "outerglow") {
			layer = &state.style.sdf.outer_glow;
			engine_layer = &engine_defaults.style.sdf.outer_glow;
		}
		if (canonical_tag == "innerglow") {
			layer = &state.style.sdf.inner_glow;
			engine_layer = &engine_defaults.style.sdf.inner_glow;
		}

		if (arg.empty()) {
			*layer = *engine_layer;
			return true;
		}
		if (IsOffRichTextValue(arg)) {
			*layer = {};
			return true;
		}

		auto args{ SplitRichTextArguments(arg) };
		if (args.size() < 2 || args.size() > 3) {
			error = "Expected color,width[,softness].";
			return false;
		}
		Color parsed_color{};
		float width{};
		float softness{ 1.0f };
		if (!ParseRichColor(args[0], parsed_color) || !ParseRichFloat(args[1], width) ||
			(args.size() == 3 && !ParseRichFloat(args[2], softness))) {
			error = "Invalid distance-field effect values.";
			return false;
		}
		*layer = DistanceFieldLayerStyle{ .color = parsed_color, .width = width, .softness = softness };
		return true;
	}

	if (canonical_tag == "shadow") {
		if (!require_assignment()) return false;
		if (arg.empty()) {
			state.style.sdf.shadow = engine_defaults.style.sdf.shadow;
			state.style.sdf.shadow_offset = engine_defaults.style.sdf.shadow_offset;
			return true;
		}
		if (IsOffRichTextValue(arg)) {
			state.style.sdf.shadow = {};
			state.style.sdf.shadow_offset = {};
			return true;
		}
		auto args{ SplitRichTextArguments(arg) };
		if (args.size() < 3 || args.size() > 5) {
			error = "Expected color,x,y[,width[,softness]].";
			return false;
		}
		Color parsed_color{};
		float x{};
		float y{};
		float width{};
		float softness{ 1.0f };
		if (!ParseRichColor(args[0], parsed_color) || !ParseRichFloat(args[1], x) ||
			!ParseRichFloat(args[2], y) ||
			(args.size() >= 4 && !ParseRichFloat(args[3], width)) ||
			(args.size() >= 5 && !ParseRichFloat(args[4], softness))) {
			error = "Invalid shadow values.";
			return false;
		}
		state.style.sdf.shadow =
			DistanceFieldLayerStyle{ .color = parsed_color, .width = width, .softness = softness };
		state.style.sdf.shadow_offset = { x, y };
		return true;
	}

	if (canonical_tag == "fx") {
		if (!require_assignment()) return false;
		if (arg.empty()) {
			state.style.effect = engine_defaults.style.effect;
			return true;
		}
		if (IsOffRichTextValue(arg)) {
			state.style.effect = {};
			return true;
		}
		auto args{ SplitRichTextArguments(arg) };
		if (args.empty() || args.size() > 5) {
			error = "Expected type[,amplitude[,frequency[,speed[,phase]]]].";
			return false;
		}

		GlyphEffectStyle effect{};
		if (!ParseGlyphEffectType(args[0], effect.type)) {
			error = "Unknown glyph effect type.";
			return false;
		}
		if (args.size() >= 2 && !ParseRichFloat(args[1], effect.amplitude)) {
			error = "Invalid glyph effect amplitude.";
			return false;
		}
		if (args.size() >= 3 && !ParseRichFloat(args[2], effect.frequency)) {
			error = "Invalid glyph effect frequency.";
			return false;
		}
		if (args.size() >= 4 && !ParseRichFloat(args[3], effect.speed)) {
			error = "Invalid glyph effect speed.";
			return false;
		}
		if (args.size() >= 5 && !ParseRichFloat(args[4], effect.phase)) {
			error = "Invalid glyph effect phase.";
			return false;
		}
		state.style.effect = effect;
		return true;
	}

	error = "Unknown rich-text tag <" + canonical_tag + ">.";
	return false;
}

[[nodiscard]] bool IsEscapedRichTextCharacter(std::string_view source, std::size_t position) {
	std::size_t backslashes{ 0 };
	while (position > backslashes && source[position - backslashes - 1] == '\\') {
		++backslashes;
	}
	return (backslashes % 2) != 0;
}

[[nodiscard]] bool HasMatchingRichTextClose(
	std::string_view source, std::size_t from, std::string_view canonical_tag
) {
	std::size_t depth{ 1 };

	for (std::size_t i{ from }; i < source.size();) {
		auto open{ source.find('<', i) };
		while (open != std::string_view::npos && IsEscapedRichTextCharacter(source, open)) {
			open = source.find('<', open + 1);
		}
		if (open == std::string_view::npos) {
			return false;
		}

		const auto close{ source.find('>', open + 1) };
		if (close == std::string_view::npos) {
			return false;
		}

		auto token{ TrimRichTextToken(source.substr(open + 1, close - open - 1)) };
		if (token.empty()) {
			i = close + 1;
			continue;
		}

		const bool closing{ token.front() == '/' };
		if (closing) {
			token.remove_prefix(1);
		}

		const auto equals{ token.find('=') };
		const auto raw_name{ equals == std::string_view::npos ? token : token.substr(0, equals) };
		const std::string tag{ CanonicalRichTag(raw_name) };

		if (tag == canonical_tag) {
			if (closing) {
				if (--depth == 0) {
					return true;
				}
			} else {
				const std::optional<std::string_view> argument{
					equals == std::string_view::npos
						? std::nullopt
						: std::optional<std::string_view>{ token.substr(equals + 1) }
				};
				RichTextState probe{};
				std::string probe_tag;
				std::string error;
				if (ApplyRichTextOpenTag(raw_name, argument, probe, probe_tag, error)) {
					++depth;
				}
			}
		}

		i = close + 1;
	}

	return false;
}

void EmitRichTextRun(StyledText& text, std::string& buffer, const RichTextState& state) {
	if (buffer.empty()) {
		return;
	}

	if (!text.runs.empty() && text.runs.back().font == state.font &&
		text.runs.back().style == state.style) {
		text.runs.back().text += buffer;
	} else {
		text.runs.emplace_back(TextRun{
			.text = std::move(buffer),
			.font = state.font,
			.style = state.style,
		});
	}
	buffer.clear();
}

[[nodiscard]] std::string FormatRichFloat(float value) {
	std::ostringstream stream;
	stream << std::setprecision(6) << std::defaultfloat << value;
	return stream.str();
}

[[nodiscard]] std::string FormatRichColor(Color color) {
	constexpr char digits[]{ "0123456789ABCDEF" };
	std::string result{ "#000000" };
	auto write_byte = [&](std::size_t offset, std::uint8_t value) {
		result[offset] = digits[(value >> 4) & 0x0F];
		result[offset + 1] = digits[value & 0x0F];
	};
	write_byte(1, color.r);
	write_byte(3, color.g);
	write_byte(5, color.b);
	if (color.a != 255) {
		result += "00";
		write_byte(7, color.a);
	}
	return result;
}

[[nodiscard]] std::string GlyphEffectName(GlyphEffectType type) {
	switch (type) {
		case GlyphEffectType::None: return "None";
		case GlyphEffectType::Wobble: return "Wobble";
		case GlyphEffectType::Wave: return "Wave";
		case GlyphEffectType::Shake: return "Shake";
		case GlyphEffectType::Pulse: return "Pulse";
	}
	return "None";
}

void AddRichWrapper(
	std::vector<std::pair<std::string, std::string>>& wrappers, std::string open,
	std::string close
) {
	wrappers.emplace_back(std::move(open), std::move(close));
}

[[nodiscard]] std::string LayerArgument(const DistanceFieldLayerStyle& layer) {
	return FormatRichColor(layer.color) + "," + FormatRichFloat(layer.width) + "," +
		   FormatRichFloat(layer.softness);
}

} // namespace

std::string EscapeRichText(std::string_view text) {
	std::string result;
	result.reserve(text.size());
	for (char c : text) {
		if (c == '<' || c == '>' || c == '\\') {
			result.push_back('\\');
		}
		result.push_back(c);
	}
	return result;
}

std::string ExpandRichTextVariables(
	std::string_view source, const RichTextVariableResolver& resolver
) {
	std::string result;
	result.reserve(source.size());

	for (std::size_t i{ 0 }; i < source.size();) {
		if (source[i] != '$' || i + 1 >= source.size() || source[i + 1] != '{') {
			result.push_back(source[i++]);
			continue;
		}

		const auto close{ source.find('}', i + 2) };
		if (close == std::string_view::npos) {
			result.append(source.substr(i));
			break;
		}

		const auto name{ source.substr(i + 2, close - (i + 2)) };
		if (!name.empty() && resolver) {
			if (auto value{ resolver(name) }) {
				result += EscapeRichText(*value);
				i = close + 1;
				continue;
			}
		}

		result.append(source.substr(i, close - i + 1));
		i = close + 1;
	}

	return result;
}

RichTextParseResult ParseRichText(std::string_view source, const TextRunDefaults& defaults) {
	RichTextParseResult result;
	RichTextState state{ .font = defaults.font, .style = defaults.style };
	std::vector<RichTextStackEntry> stack;
	std::string buffer;

	for (std::size_t i{ 0 }; i < source.size();) {
		if (source[i] == '\\' && i + 1 < source.size() &&
			(source[i + 1] == '<' || source[i + 1] == '>' || source[i + 1] == '\\')) {
			buffer.push_back(source[i + 1]);
			i += 2;
			continue;
		}

		if (source[i] != '<') {
			buffer.push_back(source[i++]);
			continue;
		}

		const auto close{ source.find('>', i + 1) };
		if (close == std::string_view::npos) {
			buffer.append(source.substr(i));
			result.diagnostics.push_back({ i, "Unterminated rich-text tag." });
			break;
		}

		const auto raw_token{ source.substr(i + 1, close - i - 1) };
		auto token{ TrimRichTextToken(raw_token) };
		if (token.empty()) {
			buffer.append(source.substr(i, close - i + 1));
			result.diagnostics.push_back({ i, "Empty rich-text tag." });
			i = close + 1;
			continue;
		}

		if (token.front() == '/') {
			token.remove_prefix(1);
			const std::string tag{ CanonicalRichTag(token) };
			if (stack.empty() || stack.back().tag != tag) {
				buffer.append(source.substr(i, close - i + 1));
				result.diagnostics.push_back({ i, "Mismatched closing tag </" + tag + ">." });
				i = close + 1;
				continue;
			}

			EmitRichTextRun(result.text, buffer, state);
			state = stack.back().previous;
			stack.pop_back();
			i = close + 1;
			continue;
		}

		const auto equals{ token.find('=') };
		const auto name{ equals == std::string_view::npos ? token : token.substr(0, equals) };
		const std::optional<std::string_view> argument{
			equals == std::string_view::npos
				? std::nullopt
				: std::optional<std::string_view>{ token.substr(equals + 1) }
		};

		RichTextState next{ state };
		std::string canonical_tag;
		std::string error;
		if (!ApplyRichTextOpenTag(name, argument, next, canonical_tag, error)) {
			buffer.append(source.substr(i, close - i + 1));
			result.diagnostics.push_back({ i, std::move(error) });
			i = close + 1;
			continue;
		}

		if (!HasMatchingRichTextClose(source, close + 1, canonical_tag)) {
			buffer.append(source.substr(i, close - i + 1));
			result.diagnostics.push_back({
				i, "Unclosed rich-text tag <" + canonical_tag + ">; rendered literally."
			});
			i = close + 1;
			continue;
		}

		EmitRichTextRun(result.text, buffer, state);
		stack.push_back({ .tag = canonical_tag, .previous = state, .position = i });
		state = std::move(next);
		i = close + 1;
	}

	EmitRichTextRun(result.text, buffer, state);

	for (auto it{ stack.rbegin() }; it != stack.rend(); ++it) {
		result.diagnostics.push_back({ it->position, "Unclosed rich-text tag <" + it->tag + ">." });
	}

	if (result.text.runs.empty()) {
		result.text.runs.emplace_back(TextRun{ .font = defaults.font, .style = defaults.style });
	}

	return result;
}

RichTextParseResult ParseRichText(const RichText& rich_text) {
	return ParseRichText(rich_text.source, rich_text.defaults);
}

std::string SerializeStyledTextToRichText(
	const StyledText& styled_text, const TextRunDefaults& defaults
) {
	std::string result;
	const TextRunDefaults engine_defaults{};

	for (const auto& run : styled_text.runs) {
		std::vector<std::pair<std::string, std::string>> wrappers;

		if (run.font != defaults.font) {
			const std::string open{
				run.font == engine_defaults.font ? "<font=>" : "<font=" + run.font.value + ">"
			};
			AddRichWrapper(wrappers, open, "</font>");
		}
		if (!NearlyEqual(run.style.size, defaults.style.size)) {
			const std::string value{
				NearlyEqual(run.style.size, engine_defaults.style.size)
					? std::string{}
					: FormatRichFloat(run.style.size)
			};
			AddRichWrapper(wrappers, "<size=" + value + ">", "</size>");
		}
		if (run.style.color != defaults.style.color) {
			const std::string value{
				run.style.color == engine_defaults.style.color
					? std::string{}
					: FormatRichColor(run.style.color)
			};
			AddRichWrapper(wrappers, "<c=" + value + ">", "</c>");
		}

		auto add_flag = [&](FontStyle flag, std::string_view tag) {
			const bool enabled{ HasFontFlag(run.style.flags, flag) };
			const bool default_enabled{ HasFontFlag(defaults.style.flags, flag) };
			if (enabled == default_enabled) return;
			AddRichWrapper(
				wrappers,
				enabled ? "<" + std::string{ tag } + ">"
						: "<" + std::string{ tag } + "=off>",
				"</" + std::string{ tag } + ">"
			);
		};

		const bool bold{ HasFontFlag(run.style.flags, FontStyle::Bold) };
		const bool default_bold{ HasFontFlag(defaults.style.flags, FontStyle::Bold) };
		if (bold != default_bold ||
			(bold && !NearlyEqual(run.style.bold_weight, defaults.style.bold_weight))) {
			std::string open{ "<b=off>" };
			if (bold) {
				open = NearlyEqual(run.style.bold_weight, engine_defaults.style.bold_weight)
					? "<b>"
					: "<b=" + FormatRichFloat(run.style.bold_weight) + ">";
			}
			AddRichWrapper(wrappers, std::move(open), "</b>");
		}
		add_flag(FontStyle::Italic, "i");
		add_flag(FontStyle::Underline, "u");
		add_flag(FontStyle::Strikethrough, "s");

		auto add_float = [&](float value, float rich_default, float engine_default, std::string_view tag) {
			if (NearlyEqual(value, rich_default)) return;
			const std::string argument{
				NearlyEqual(value, engine_default) ? std::string{} : FormatRichFloat(value)
			};
			AddRichWrapper(
				wrappers, "<" + std::string{ tag } + "=" + argument + ">",
				"</" + std::string{ tag } + ">"
			);
		};

		add_float(run.style.kerning, defaults.style.kerning, engine_defaults.style.kerning, "kern");
		add_float(run.style.tracking, defaults.style.tracking, engine_defaults.style.tracking, "track");
		add_float(
			run.style.line_spacing, defaults.style.line_spacing,
			engine_defaults.style.line_spacing, "line"
		);

		auto add_layer = [&](const DistanceFieldLayerStyle& layer,
							 const DistanceFieldLayerStyle& rich_default,
							 const DistanceFieldLayerStyle& engine_default,
							 std::string_view tag) {
			if (layer == rich_default) return;
			const std::string value{
				layer == engine_default ? std::string{} : LayerArgument(layer)
			};
			AddRichWrapper(
				wrappers, "<" + std::string{ tag } + "=" + value + ">",
				"</" + std::string{ tag } + ">"
			);
		};

		add_layer(
			run.style.sdf.outline, defaults.style.sdf.outline,
			engine_defaults.style.sdf.outline, "outline"
		);
		if (run.style.sdf.shadow != defaults.style.sdf.shadow ||
			run.style.sdf.shadow_offset != defaults.style.sdf.shadow_offset) {
			std::string value;
			if (run.style.sdf.shadow != engine_defaults.style.sdf.shadow ||
				run.style.sdf.shadow_offset != engine_defaults.style.sdf.shadow_offset) {
				value = FormatRichColor(run.style.sdf.shadow.color) + "," +
						FormatRichFloat(run.style.sdf.shadow_offset.x) + "," +
						FormatRichFloat(run.style.sdf.shadow_offset.y) + "," +
						FormatRichFloat(run.style.sdf.shadow.width) + "," +
						FormatRichFloat(run.style.sdf.shadow.softness);
			}
			AddRichWrapper(wrappers, "<shadow=" + value + ">", "</shadow>");
		}
		add_layer(
			run.style.sdf.outer_glow, defaults.style.sdf.outer_glow,
			engine_defaults.style.sdf.outer_glow, "outerglow"
		);
		add_layer(
			run.style.sdf.inner_glow, defaults.style.sdf.inner_glow,
			engine_defaults.style.sdf.inner_glow, "innerglow"
		);

		if (run.style.effect != defaults.style.effect) {
			std::string value;
			if (run.style.effect != engine_defaults.style.effect) {
				const auto& effect{ run.style.effect };
				value = GlyphEffectName(effect.type) + "," + FormatRichFloat(effect.amplitude) + "," +
						FormatRichFloat(effect.frequency) + "," + FormatRichFloat(effect.speed) + "," +
						FormatRichFloat(effect.phase);
			}
			AddRichWrapper(wrappers, "<fx=" + value + ">", "</fx>");
		}

		for (const auto& [open, close] : wrappers) {
			(void)close;
			result += open;
		}
		result += EscapeRichText(run.text);
		for (auto it{ wrappers.rbegin() }; it != wrappers.rend(); ++it) {
			result += it->second;
		}
	}

	return result;
}


namespace impl {

ResolvedTextRun ResolveTextRun(AssetManager& asset_manager, const TextRun& text_run) {
	bool has_font{ impl::AssetAccessor{ asset_manager }.Has<Font>(text_run.font) };

	if (!has_font) {
		PTGN_WARN("Font not found: ", text_run.font, ". Using default font instead.");
	}

	FontKey font_key{ has_font ? text_run.font : FontKey{ kDefaultFont } };

	auto font{ impl::AssetAccessor{ asset_manager }.Get<Font>(font_key) };

	auto font_atlas{ &font.GetEntity().Get<impl::FontAtlas>() };

	return {
		.text  = text_run.text,
		.font  = font_atlas,
		.style = text_run.style,
	};
}

ResolvedStyledText ResolveStyledText(AssetManager& asset_manager, const StyledText& styled_text) {
	ResolvedStyledText result;
	result.runs.reserve(styled_text.runs.size());
	for (const auto& run : styled_text.runs) {
		result.runs.emplace_back(ResolveTextRun(asset_manager, run));
	}
	return result;
}

TextLayout BuildTextLayout(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	return BuildTextLayout(ResolveStyledText(asset_manager, styled_text), box);
}

} // namespace impl

void Text::Draw(DrawContext& ctx, Entity entity) {
	auto& scene{ entity.GetScene() };

	if (!entity.Has<impl::TextData>()) {
		PTGN_WARN("Text entity cannot be drawn without TextData component");
		return;
	}

	Text text{ entity };

	const auto& data{ entity.Get<impl::TextData>() };

	if (!data.text.HasContent()) {
		return;
	}

	const auto& layout{ text.GetLayout() };

	auto transform{ GetDrawTransform(entity) };
	auto origin{ entity.GetOrDefault<Origin>() };

	auto prepared{ impl::PrepareTextDraw(transform, layout, data.box, origin, data.clip) };

	if (!prepared.drawable) {
		return;
	}

	auto effects{ impl::GetEffectParams(entity) };

	ctx.SetBlendMode(GetBlendMode(entity));

	DrawTextRequest request{
		.layout				= layout,
		.tint				= GetTint(entity),
		.depth				= GetDepth(entity),
		.entity_id			= entity.Get<UUID>(),
		.clips				= prepared.GetClips(),
		.reveal_glyph_count = text.GetRevealGlyphCount(),
		.time				= scene.ctx().GameTime().count(),
	};

	ctx.DrawText(prepared.transform, request, effects);
}

Text::Text(Entity entity) : Entity{ entity } {}

Text& Text::Clear() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	bool changed{ data.text.HasContent() };
	data.text.runs.clear();
	data.text.runs.emplace_back(TextRun{
		.font = data.defaults.font,
		.style = data.defaults.style,
	});

	data.current_run_index = 0;

	if (changed) {
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(std::string_view content) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot add text content without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.size() == 1 && data.text.runs.front().text.empty()) {
		data.current_run_index = 0;

		auto& run{ data.text.runs.front() };
		const TextRun baseline{
			.font = data.defaults.font,
			.style = data.defaults.style,
		};
		bool changed{ run.font != baseline.font || run.style != baseline.style };
		run.font = baseline.font;
		run.style = baseline.style;
		if (run.text != content) {
			run.text = std::string{ content };
			changed = true;
		}
		if (changed) {
			InvalidateLayout();
		}

		return *this;
	}

	auto& run{ data.text.runs.emplace_back(TextRun{
		.font = data.defaults.font,
		.style = data.defaults.style,
	}) };

	data.current_run_index = data.text.runs.size() - 1;

	if (!content.empty()) {
		run.text = std::string{ content };
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(StyledText styled_text) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot add text content without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back(TextRun{
			.font = data.defaults.font,
			.style = data.defaults.style,
		});
	}

	if (data.text != styled_text) {
		data.text = std::move(styled_text);
		InvalidateLayout();
	}

	data.current_run_index = 0;

	return *this;
}

Text& Text::Select(std::size_t index) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot select text index without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.empty()) {
		data.text.runs.emplace_back(TextRun{
			.font = data.defaults.font,
			.style = data.defaults.style,
		});
	}

	data.current_run_index = std::min(index, data.text.runs.size() - 1);

	return *this;
}

Text& Text::SetRichText(std::string_view source) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set rich text without TextData component");
		return *this;
	}

	const auto& defaults{ Get<impl::TextData>().defaults };
	return Content(ParseRichText(source, defaults).text);
}

Text& Text::SetRichText(std::string_view source, const TextRunDefaults& defaults) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set rich text without TextData component");
		return *this;
	}

	Get<impl::TextData>().defaults = defaults;
	return Content(ParseRichText(source, defaults).text);
}

Text& Text::SetRichText(const RichText& rich_text) {
	return SetRichText(rich_text.source, rich_text.defaults);
}

Text& Text::Box(Rect text_rect) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text box rect without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; text_rect != box.rect) {
		box.rect = text_rect;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Box(const TextBox& text_box) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text box without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; text_box != box) {
		box = text_box;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Reveal(std::optional<std::size_t> glyph_count) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text glyph reveal count without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };
	data.glyph_count = glyph_count;

	return *this;
}

Text& Text::Align(Alignment alignment) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current != alignment) {
		current = alignment;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Align(Origin origin) {
	return Align(GetAlignment(origin));
}

Text& Text::Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	return Align(
		{
			.horizontal = horizontal,
			.vertical	= vertical,
		}
	);
}

Text& Text::HorizontalAlign(ptgn::HorizontalAlign horizontal) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text horizontal alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.horizontal != horizontal) {
		current.horizontal = horizontal;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign vertical) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text vertical alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.vertical != vertical) {
		current.vertical = vertical;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ClearAlignment() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.vertical.has_value() || current.horizontal.has_value()) {
		current.vertical.reset();
		current.horizontal.reset();
		InvalidateLayout();
	}

	return *this;
}

V2_float Text::GetSize() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text size without TextData component");
		return {};
	}

	return GetLayout().size;
}

Rect Text::GetBounds() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text bounds without TextData component");
		return {};
	}

	const auto& layout{ GetLayout() };
	auto bounds{ layout.GetBounds() };

	if (const auto& box{ Get<impl::TextData>().box }; box.HasBox()) {
		auto origin{ GetOrDefault<Origin>() };
		auto origin_point{ box.rect.GetOriginPoint(origin) };
		// TODO: Check if this is correct.
		return bounds.Translated(-origin_point);
	}

	return bounds;
}

Text& Text::Wrap(WrapMode mode) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text wrap without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.wrap.mode != mode) {
		box.style.wrap.mode = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::WrapSettings(const ptgn::WrapSettings& settings) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text wrap settings without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.wrap != settings) {
		box.style.wrap = settings;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Overflow(OverflowMode mode) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text overflow without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.overflow != mode) {
		box.style.overflow = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Clip(std::optional<TextClip> clip) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text clip without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };
	data.clip = clip;

	return *this;
}

Text& Text::CollapseSpaces(bool collapse) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot collapse text spaces without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.collapse_spaces != collapse) {
		box.style.collapse_spaces = collapse;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::JustifyLastLine(bool justify) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot just last text line without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.justify_last_line != justify) {
		box.style.justify_last_line = justify;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::TabWidth(std::size_t spaces) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text tab width without TextData component");
		return *this;
	}

	spaces = std::max(1uz, spaces);

	if (auto& box{ Get<impl::TextData>().box }; box.style.tab_width != spaces) {
		box.style.tab_width = spaces;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::MaxLines(std::size_t max_lines) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set max text lines without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.max_lines != max_lines) {
		box.style.max_lines = max_lines;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ScaleToFit(float min_scale, float max_scale) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text scale to fit without TextData component");
		return *this;
	}

	PTGN_ASSERT(
		std::isfinite(min_scale) && std::isfinite(max_scale),
		"Text scale limits must be finite"
	);

	// Order is important.
	max_scale = std::max(kEpsilon<float>, max_scale);
	min_scale = std::clamp(min_scale, kEpsilon<float>, max_scale);

	ShrinkScale scale{ .min = min_scale, .max = max_scale };

	if (auto& style{ Get<impl::TextData>().box.style };
		style.overflow != OverflowMode::ScaleToFit || style.shrink_scale != scale) {
		style.overflow		   = OverflowMode::ScaleToFit;
		style.shrink_scale.min = min_scale;
		style.shrink_scale.max = max_scale;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Font(FontKey font_key) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text font without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.font != font_key) {
		run.font = std::move(font_key);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Color(ptgn::Color color) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text color without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.color != color) {
		run.style.color = color;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Size(float font_size) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text size without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.size, font_size)) {
		run.style.size = font_size;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Kerning(float kerning) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text kerning without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.kerning, kerning)) {
		run.style.kerning = kerning;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Tracking(float tracking) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text tracking without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.tracking, tracking)) {
		run.style.tracking = tracking;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::LineSpacing(float line_spacing) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text line spacing without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.line_spacing, line_spacing)) {
		run.style.line_spacing = line_spacing;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Style(FontStyle flags) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text font style without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.flags != flags) {
		run.style.flags = flags;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Bold(bool enabled, float weight) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot bold text without TextData component");
		return *this;
	}

	auto& run{ CurrentRun() };

	bool was_enabled{ HasFontFlag(run.style.flags, FontStyle::Bold) };
	bool layout_changed{ was_enabled != enabled ||
						 (enabled && !NearlyEqual(run.style.bold_weight, weight)) };

	run.style.flags		  = SetFontFlag(run.style.flags, FontStyle::Bold, enabled);
	run.style.bold_weight = weight;

	if (layout_changed) {
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Italic(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot italicize text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Italic) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Italic, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Underline(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot underline text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Underline) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Underline, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Strikethrough(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot strikethrough text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() };
		HasFontFlag(run.style.flags, FontStyle::Strikethrough) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Strikethrough, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Outline(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot outline text without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle outline{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outline != outline) {
		run.style.sdf.outline = outline;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text shadow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle shadow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() };
		run.style.sdf.shadow != shadow || run.style.sdf.shadow_offset != offset) {
		run.style.sdf.shadow		= shadow;
		run.style.sdf.shadow_offset = offset;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

Text& Text::OuterGlow(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set outer text glow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle outer_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outer_glow != outer_glow) {
		run.style.sdf.outer_glow = outer_glow;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::InnerGlow(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set inner text glow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle inner_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.inner_glow != inner_glow) {
		run.style.sdf.inner_glow = inner_glow;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ClearSdfEffects() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text sdf effects without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.sdf != DistanceFieldStyle{}) {
		run.style.sdf = {};
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text effect without TextData component");
		return *this;
	}

	GlyphEffectStyle effect{
		.type = type, .amplitude = amplitude, .frequency = frequency, .speed = speed, .phase = phase
	};
	if (auto& run{ CurrentRun() }; run.style.effect != effect) {
		run.style.effect = effect;
		InvalidateLayout();
	}

	return *this;
}

TextMeasurement Text::Measure() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot measure text without TextData component");
		return {};
	}
	
	const auto& layout{ GetLayout() };

	return {
		.size			   = layout.size,
		.line_count		   = layout.lines.size(),
		.truncated		   = layout.truncated,
		.used_shrink_scale = layout.used_shrink_scale,
	};
}

Text& Text::RevealFraction(float fraction) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text reveal fraction without TextData component");
		return *this;
	}

	fraction = std::clamp(fraction, 0.0f, 1.0f);

	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };

	auto reveal_count{
		static_cast<std::size_t>(std::round(static_cast<float>(glyph_count) * fraction))
	};

	return Reveal(reveal_count);
}

std::size_t Text::GetRevealGlyphCount() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text glyph reveal count without TextData component");
		return 0;
	}

	const auto& data{ Get<impl::TextData>() };

	return data.glyph_count.value_or(std::numeric_limits<std::size_t>::max());
}

bool Text::IsFullyRevealed() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot check if text is fully revealed without TextData component");
		return false;
	}

	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };
	auto revealed_glyphs{ GetRevealGlyphCount() };

	return revealed_glyphs >= glyph_count;
}

const StyledText& Text::GetStyledText() const {
	return Get<impl::TextData>().text;
}

const TextBox& Text::GetTextBox() const {
	return Get<impl::TextData>().box;
}

const TextLayout& Text::GetLayout() const {
	UpdateLayout();
	return Get<TextLayout>();
}

void Text::UpdateLayout() const {
	auto& asset_manager{ GetScene().ctx().asset };
	const auto& styled_text{ GetStyledText() };
	const auto& box{ GetTextBox() };

	ptgn::UpdateLayout(*this, asset_manager, styled_text, box);
}

TextRun& Text::CurrentRun() {
	PTGN_ASSERT(
		Has<impl::TextData>(),
		"Cannot get current text run without TextData component"
	);

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.empty()) {
		data.text.runs.emplace_back(TextRun{
			.font = data.defaults.font,
			.style = data.defaults.style,
		});
		data.current_run_index = 0;
	}

	data.current_run_index =
		std::min(data.current_run_index, data.text.runs.size() - 1);

	return data.text.runs[data.current_run_index];
}

void Text::InvalidateLayout() {
	if (auto layout{ TryGet<TextLayout>() }) {
		layout->dirty = true;
	}
}

Text CreateText(Scene& scene, Transform transform, StyledText styled_text, Origin origin) {
	Text text{ scene.CreateEntity() };

	text.Add<impl::TextData>();
	text.Add<TextLayout>();
	text.Add<Transform>(transform);
	text.Add<Origin>(origin);
	text.Add<Visible>(true);
	text.Add<Tag>("Text");

	text.Content(std::move(styled_text));

	SetDraw<Text>(text);

	return text;
}

Text CreateText(
	Scene& scene, Transform transform, std::string_view content, Color color, float font_size,
	Origin origin, FontKey font
) {
	return CreateText(
		scene, transform,
		{ { .text  = std::string{ content },
			.font  = std::move(font),
			.style = { .color = color, .size = font_size } } },
		origin
	);
}

} // namespace ptgn