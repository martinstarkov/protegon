#include "runtime/ui/dialogue.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <optional>
#include <span>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/event/key_event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/text/text_pagination.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace {

constexpr std::string_view kDialogueScrollChannel{ "dialogue.scroll" };

[[nodiscard]] std::optional<Entity> FindDialoguePart(Entity dialogue, DialoguePartRole role) {
	if (!HasChildren(dialogue)) {
		return std::nullopt;
	}

	for (Entity child : GetChildren(dialogue)) {
		auto part{ child.TryGet<impl::DialoguePart>() };
		if (part && part->role == role) {
			return child;
		}
	}

	return std::nullopt;
}

void AppendStyledRun(StyledText& text, const TextRun& source, std::string_view content) {
	if (content.empty()) {
		return;
	}

	if (!text.runs.empty() && text.runs.back().font == source.font &&
		text.runs.back().style == source.style) {
		text.runs.back().text.append(content);
		return;
	}

	text.runs.emplace_back(TextRun{
		.text = std::string{ content },
		.font = source.font,
		.style = source.style,
	});
}

[[nodiscard]] std::vector<StyledText> SplitDialogueParagraphs(const StyledText& text) {
	std::vector<StyledText> paragraphs;
	StyledText current;

	std::size_t pending_newlines{ 0 };
	std::optional<TextRun> newline_anchor;

	auto flush_pending_newlines = [&]() {
		if (pending_newlines == 0) {
			return;
		}

		if (pending_newlines == 1) {
			const TextRun& anchor{
				newline_anchor.has_value()
					? newline_anchor.value()
					: TextRun{}
			};
			AppendStyledRun(current, anchor, "\n");
		} else if (current.HasContent()) {
			paragraphs.emplace_back(std::move(current));
			current = {};
		}

		pending_newlines = 0;
		newline_anchor.reset();
	};

	for (const auto& run : text.runs) {
		std::size_t position{ 0 };
		std::size_t chunk_begin{ 0 };

		while (position < run.text.size()) {
			const char c{ run.text[position] };
			if (c != '\n' && c != '\r') {
				++position;
				continue;
			}

			if (position > chunk_begin) {
				flush_pending_newlines();
				AppendStyledRun(
					current,
					run,
					std::string_view{ run.text }.substr(
						chunk_begin,
						position - chunk_begin
					)
				);
			}

			if (!newline_anchor.has_value()) {
				newline_anchor = run;
				newline_anchor->text.clear();
			}
			++pending_newlines;

			if (c == '\r' && position + 1 < run.text.size() &&
				run.text[position + 1] == '\n') {
				position += 2;
			} else {
				++position;
			}
			chunk_begin = position;
		}

		if (chunk_begin < run.text.size()) {
			flush_pending_newlines();
			AppendStyledRun(
				current,
				run,
				std::string_view{ run.text }.substr(chunk_begin)
			);
		}
	}

	flush_pending_newlines();

	if (current.HasContent()) {
		paragraphs.emplace_back(std::move(current));
	}

	if (paragraphs.empty()) {
		paragraphs.emplace_back(text);
	}

	return paragraphs;
}

void ReplaceDialogueInlineNewlines(StyledText& text) {
	for (auto& run : text.runs) {
		std::size_t position{ 0 };
		while ((position = run.text.find("\\n", position)) != std::string::npos) {
			run.text.replace(position, 2, "\n");
			++position;
		}
	}
}

[[nodiscard]] std::vector<DialoguePage> PaginateDialogueParagraph(
	AssetManager& asset_manager, const StyledText& styled_text,
	const DialoguePageProperties& properties, std::string_view split_end,
	std::string_view split_begin
) {
	auto pagination{ impl::PaginateText(
		asset_manager, styled_text, properties.ToTextBox(),
		impl::TextPageOptions{
			.split_end = std::string{ split_end },
			.split_begin = std::string{ split_begin },
			.max_lines_per_page = 0,
			.add_split_markers = true,
		}
	) };

	std::vector<DialoguePage> pages;
	pages.reserve(pagination.pages.size());
	for (auto& page : pagination.pages) {
		pages.emplace_back(std::move(page.styled_text), properties);
	}
	return pages;
}

struct PreparedDialogueSource {
	std::string source{};
	std::vector<std::size_t> instant_prefix_ends{};
};

[[nodiscard]] bool IsDialogueInstantTagLine(std::string_view line) {
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
		line.remove_prefix(1);
	}
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
		line.remove_suffix(1);
	}
	return line == impl::kDialogueInstantPageTag;
}

[[nodiscard]] PreparedDialogueSource PrepareDialogueSource(std::string_view source) {
	PreparedDialogueSource prepared;
	prepared.source.reserve(source.size());

	std::size_t line_begin{ 0 };
	while (line_begin <= source.size()) {
		const std::size_t newline{ source.find('\n', line_begin) };
		const std::size_t line_end{
			newline == std::string_view::npos ? source.size() : newline
		};
		std::string_view line{ source.substr(line_begin, line_end - line_begin) };
		if (!line.empty() && line.back() == '\r') {
			line.remove_suffix(1);
		}

		if (IsDialogueInstantTagLine(line)) {
			std::size_t prefix_end{ prepared.source.size() };
			while (prefix_end > 0 &&
				(prepared.source[prefix_end - 1] == '\n' ||
				 prepared.source[prefix_end - 1] == '\r')) {
				--prefix_end;
			}
			prepared.instant_prefix_ends.emplace_back(prefix_end);

			// Replacing the standalone control with an extra blank line guarantees a manual
			// page divider without ever passing the control token into the rich-text parser.
			prepared.source.push_back('\n');
			if (newline != std::string_view::npos) {
				prepared.source.push_back('\n');
			}
		} else {
			prepared.source.append(line);
			if (newline != std::string_view::npos) {
				prepared.source.push_back('\n');
			}
		}

		if (newline == std::string_view::npos) {
			break;
		}
		line_begin = newline + 1;
	}

	return prepared;
}

[[nodiscard]] std::vector<DialoguePage> PaginateDialogueSourceCore(
	AssetManager& asset_manager, std::string_view source,
	const DialoguePageProperties& properties, std::string_view split_end,
	std::string_view split_begin
) {
	const auto parsed{ ParseRichText(source, properties.text_defaults) };
	auto paragraphs{ SplitDialogueParagraphs(parsed.text) };

	std::vector<DialoguePage> pages;
	for (auto& paragraph : paragraphs) {
		ReplaceDialogueInlineNewlines(paragraph);
		auto paragraph_pages{
			PaginateDialogueParagraph(asset_manager, paragraph, properties, split_end, split_begin)
		};
		pages.append_range(paragraph_pages);
	}
	return pages;
}

[[nodiscard]] std::vector<DialoguePage> PaginateDialogueSourceImpl(
	AssetManager& asset_manager, std::string_view source,
	const DialoguePageProperties& properties, std::string_view split_end,
	std::string_view split_begin
) {
	const PreparedDialogueSource prepared{ PrepareDialogueSource(source) };
	auto pages{ PaginateDialogueSourceCore(
		asset_manager, prepared.source, properties, split_end, split_begin
	) };

	for (const std::size_t prefix_end : prepared.instant_prefix_ends) {
		const std::size_t clamped_end{ std::min(prefix_end, prepared.source.size()) };
		std::size_t preceding_page_count{ 0 };
		if (clamped_end > 0) {
			auto preceding_pages{ PaginateDialogueSourceCore(
				asset_manager,
				std::string_view{ prepared.source }.substr(0, clamped_end),
				properties,
				split_end,
				split_begin
			) };
			preceding_page_count = preceding_pages.size();
		}
		if (preceding_page_count < pages.size()) {
			pages[preceding_page_count].instant = true;
		}
	}

	return pages;
}

[[nodiscard]] std::string_view TrimKeyToken(std::string_view value) {
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
		value.remove_prefix(1);
	}
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
		value.remove_suffix(1);
	}
	return value;
}

[[nodiscard]] std::string NormalizeKeyToken(std::string_view value) {
	std::string result;
	result.reserve(value.size());
	for (const char c : value) {
		if (std::isalnum(static_cast<unsigned char>(c))) {
			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		}
	}
	return result;
}

[[nodiscard]] std::string NormalizeDialogueKeyAlias(std::string normalized) {
	if (normalized.size() == 1 && std::isdigit(static_cast<unsigned char>(normalized.front()))) {
		normalized.insert(normalized.begin(), 'k');
	}

	if (normalized == "shift" || normalized == "lshift") return "leftshift";
	if (normalized == "rshift") return "rightshift";
	if (normalized == "ctrl" || normalized == "control" || normalized == "lctrl" ||
		normalized == "leftcontrol") {
		return "leftctrl";
	}
	if (normalized == "rctrl" || normalized == "rightcontrol") return "rightctrl";
	if (normalized == "alt" || normalized == "option" || normalized == "lalt") return "leftalt";
	if (normalized == "ralt") return "rightalt";
	if (normalized == "super" || normalized == "cmd" || normalized == "command") {
		return "leftsuper";
	}
	return normalized;
}

struct DialogueNamedKey {
	std::string_view name;
	int value;
};

inline constexpr std::array kDialogueNamedKeys{
	DialogueNamedKey{ "space", 32 },
	DialogueNamedKey{ "apostrophe", 39 },
	DialogueNamedKey{ "comma", 44 },
	DialogueNamedKey{ "minus", 45 },
	DialogueNamedKey{ "period", 46 },
	DialogueNamedKey{ "slash", 47 },
	DialogueNamedKey{ "semicolon", 59 },
	DialogueNamedKey{ "equal", 61 },
	DialogueNamedKey{ "leftbracket", 91 },
	DialogueNamedKey{ "backslash", 92 },
	DialogueNamedKey{ "rightbracket", 93 },
	DialogueNamedKey{ "graveaccent", 96 },
	DialogueNamedKey{ "world1", 161 },
	DialogueNamedKey{ "world2", 162 },
	DialogueNamedKey{ "escape", 256 },
	DialogueNamedKey{ "enter", 257 },
	DialogueNamedKey{ "tab", 258 },
	DialogueNamedKey{ "backspace", 259 },
	DialogueNamedKey{ "insert", 260 },
	DialogueNamedKey{ "delete", 261 },
	DialogueNamedKey{ "right", 262 },
	DialogueNamedKey{ "left", 263 },
	DialogueNamedKey{ "down", 264 },
	DialogueNamedKey{ "up", 265 },
	DialogueNamedKey{ "pageup", 266 },
	DialogueNamedKey{ "pagedown", 267 },
	DialogueNamedKey{ "home", 268 },
	DialogueNamedKey{ "end", 269 },
	DialogueNamedKey{ "capslock", 280 },
	DialogueNamedKey{ "scrolllock", 281 },
	DialogueNamedKey{ "numlock", 282 },
	DialogueNamedKey{ "printscreen", 283 },
	DialogueNamedKey{ "pause", 284 },
	DialogueNamedKey{ "kpdecimal", 330 },
	DialogueNamedKey{ "kpdivide", 331 },
	DialogueNamedKey{ "kpmultiply", 332 },
	DialogueNamedKey{ "kpsubtract", 333 },
	DialogueNamedKey{ "kpadd", 334 },
	DialogueNamedKey{ "kpenter", 335 },
	DialogueNamedKey{ "kpequal", 336 },
	DialogueNamedKey{ "leftshift", 340 },
	DialogueNamedKey{ "leftctrl", 341 },
	DialogueNamedKey{ "leftalt", 342 },
	DialogueNamedKey{ "leftsuper", 343 },
	DialogueNamedKey{ "rightshift", 344 },
	DialogueNamedKey{ "rightctrl", 345 },
	DialogueNamedKey{ "rightalt", 346 },
	DialogueNamedKey{ "rightsuper", 347 },
	DialogueNamedKey{ "menu", 348 },
};

[[nodiscard]] std::optional<Key> ParseDialogueKey(std::string_view token) {
	token = TrimKeyToken(token);
	if (token.empty()) {
		return std::nullopt;
	}

	const std::string normalized{
		NormalizeDialogueKeyAlias(NormalizeKeyToken(token))
	};

	if (normalized.size() == 1 &&
		normalized.front() >= 'a' && normalized.front() <= 'z') {
		return static_cast<Key>(
			static_cast<int>('A') +
			static_cast<int>(normalized.front() - 'a')
		);
	}

	if (normalized.size() == 2 &&
		normalized.front() == 'k' &&
		std::isdigit(static_cast<unsigned char>(normalized[1]))) {
		return static_cast<Key>(
			static_cast<int>('0') +
			static_cast<int>(normalized[1] - '0')
		);
	}

	if (normalized.size() >= 2 && normalized.front() == 'f') {
		int number{};
		bool valid{ true };
		for (std::size_t i{ 1 }; i < normalized.size(); ++i) {
			if (!std::isdigit(static_cast<unsigned char>(normalized[i]))) {
				valid = false;
				break;
			}
			number = number * 10 + static_cast<int>(normalized[i] - '0');
		}
		if (valid && number >= 1 && number <= 25) {
			return static_cast<Key>(289 + number);
		}
	}

	if (normalized.size() == 3 &&
		normalized[0] == 'k' && normalized[1] == 'p' &&
		std::isdigit(static_cast<unsigned char>(normalized[2]))) {
		return static_cast<Key>(
			320 + static_cast<int>(normalized[2] - '0')
		);
	}

	const auto it{ std::ranges::find(
		kDialogueNamedKeys, normalized, &DialogueNamedKey::name
	) };
	if (it != kDialogueNamedKeys.end()) {
		return static_cast<Key>(it->value);
	}

	return std::nullopt;
}

[[nodiscard]] std::string DialogueKeyDisplayName(Key key) {
	const int value{ static_cast<int>(key) };

	if (value >= static_cast<int>('0') && value <= static_cast<int>('9')) {
		return std::string{ 1, static_cast<char>(value) };
	}
	if (value >= static_cast<int>('A') && value <= static_cast<int>('Z')) {
		return std::string{ 1, static_cast<char>(value) };
	}
	if (value >= 290 && value <= 314) {
		return "F" + std::to_string(value - 289);
	}
	if (value >= 320 && value <= 329) {
		return "KP" + std::to_string(value - 320);
	}

	const auto it{ std::ranges::find(
		kDialogueNamedKeys, value, &DialogueNamedKey::value
	) };
	if (it == kDialogueNamedKeys.end()) {
		return "Enter";
	}

	const std::string_view name{ it->name };
	if (name == "leftctrl") return "LeftCtrl";
	if (name == "rightctrl") return "RightCtrl";
	if (name == "leftalt") return "LeftAlt";
	if (name == "rightalt") return "RightAlt";
	if (name == "leftshift") return "LeftShift";
	if (name == "rightshift") return "RightShift";
	if (name == "leftsuper") return "LeftSuper";
	if (name == "rightsuper") return "RightSuper";
	if (name == "pageup") return "PageUp";
	if (name == "pagedown") return "PageDown";
	if (name == "printscreen") return "PrintScreen";
	if (name == "graveaccent") return "GraveAccent";
	if (name == "leftbracket") return "LeftBracket";
	if (name == "rightbracket") return "RightBracket";
	if (name == "capslock") return "CapsLock";
	if (name == "scrolllock") return "ScrollLock";
	if (name == "numlock") return "NumLock";
	if (name == "kpdecimal") return "KPDecimal";
	if (name == "kpdivide") return "KPDivide";
	if (name == "kpmultiply") return "KPMultiply";
	if (name == "kpsubtract") return "KPSubtract";
	if (name == "kpadd") return "KPAdd";
	if (name == "kpenter") return "KPEnter";
	if (name == "kpequal") return "KPEqual";
	if (name == "world1") return "World1";
	if (name == "world2") return "World2";

	std::string result{ name };
	result.front() = static_cast<char>(
		std::toupper(static_cast<unsigned char>(result.front()))
	);
	return result;
}

using DialogueKeyChord = std::vector<Key>;

[[nodiscard]] std::optional<std::vector<DialogueKeyChord>> ParseDialogueKeyExpression(
	std::string_view expression, std::string* error
) {
	auto fail = [&](std::string message)
		-> std::optional<std::vector<DialogueKeyChord>> {
		if (error) {
			*error = std::move(message);
		}
		return std::nullopt;
	};

	if (error) {
		error->clear();
	}
	if (TrimKeyToken(expression).empty()) {
		return fail("Enter at least one key.");
	}

	std::vector<DialogueKeyChord> alternatives;
	std::size_t alternative_begin{ 0 };
	while (alternative_begin <= expression.size()) {
		const std::size_t comma{ expression.find(',', alternative_begin) };
		const std::size_t alternative_end{
			comma == std::string_view::npos ? expression.size() : comma
		};
		const std::string_view alternative{
			TrimKeyToken(expression.substr(alternative_begin, alternative_end - alternative_begin))
		};
		if (alternative.empty()) {
			return fail("Empty key alternative. Use commas only between key expressions.");
		}

		DialogueKeyChord chord;
		std::size_t key_begin{ 0 };
		while (key_begin <= alternative.size()) {
			const std::size_t plus{ alternative.find('+', key_begin) };
			const std::size_t key_end{
				plus == std::string_view::npos ? alternative.size() : plus
			};
			const std::string_view token{
				TrimKeyToken(alternative.substr(key_begin, key_end - key_begin))
			};
			if (token.empty()) {
				return fail("Empty key in chord. Use + only between key names.");
			}

			const auto key{ ParseDialogueKey(token) };
			if (!key.has_value()) {
				return fail("Unknown key: " + std::string{ token });
			}
			if (!std::ranges::contains(chord, key.value())) {
				chord.emplace_back(key.value());
			}

			if (plus == std::string_view::npos) {
				break;
			}
			key_begin = plus + 1;
		}

		alternatives.emplace_back(std::move(chord));
		if (comma == std::string_view::npos) {
			break;
		}
		alternative_begin = comma + 1;
	}

	return alternatives;
}

[[nodiscard]] DialoguePageProperties InheritDialogueLayout(
	const DialoguePageProperties& parent, const json& value
) {
	auto result{ parent.InheritProperties(value) };
	result.text_defaults = parent.text_defaults;
	return result;
}

} // namespace

namespace impl {

std::vector<DialoguePage> PaginateDialogueSource(
	AssetManager& asset_manager, std::string_view source,
	const DialoguePageProperties& properties, std::string_view split_end,
	std::string_view split_begin
) {
	return PaginateDialogueSourceImpl(
		asset_manager, source, properties, split_end, split_begin
	);
}

std::string DialogueKeyName(Key key) {
	return DialogueKeyDisplayName(key);
}

bool ValidateDialogueKeyExpression(std::string_view expression, std::string* error) {
	return ParseDialogueKeyExpression(expression, error).has_value();
}

bool DialogueKeyExpressionMatches(
	std::string_view expression, std::span<const Key> held_keys
) {
	const auto alternatives{ ParseDialogueKeyExpression(expression, nullptr) };
	if (!alternatives.has_value()) {
		return false;
	}

	return std::ranges::any_of(alternatives.value(), [&](const DialogueKeyChord& chord) {
		return std::ranges::all_of(chord, [&](Key key) {
			return std::ranges::contains(held_keys, key);
		});
	});
}

void DialogueWaitScript::OnEvent(Event event) {
	event.Dispatch<event::KeyPressed>(&DialogueWaitScript::OnKeyPressed, this);
	event.Dispatch<event::KeyReleased>(&DialogueWaitScript::OnKeyReleased, this);
}

void DialogueWaitScript::OnKeyPressed(Key key) {
	if (std::ranges::contains(held_keys_, key)) {
		return;
	}

	DialogueBox dialogue{ Target() };
	const bool matched_before{
		dialogue.IsOpen() &&
		DialogueKeyExpressionMatches(dialogue.GetContinueKeys(), held_keys_)
	};
	held_keys_.emplace_back(key);

	if (!dialogue.IsOpen() || matched_before ||
		!DialogueKeyExpressionMatches(dialogue.GetContinueKeys(), held_keys_)) {
		return;
	}

	if (Text text{ dialogue.TextPart() }; !text.IsFullyRevealed()) {
		dialogue.CompletePage();
		return;
	}

	dialogue.NextPage();
}

void DialogueWaitScript::OnKeyReleased(Key key) {
	std::erase(held_keys_, key);
}

ScriptStatus DialogueScrollScript::OnUpdate() {
	DialogueBox dialogue{ Owner() };
	if (!dialogue.IsOpen()) {
		return ScriptStatus::Complete;
	}

	dialogue.TextPart().RevealFraction(Progress());
	return ScriptStatus::Running;
}

void DialogueScrollScript::OnComplete() {
	DialogueBox dialogue{ Owner() };
	if (dialogue.IsOpen()) {
		dialogue.TextPart().Reveal();
	}
}

} // namespace impl

DialoguePage::DialoguePage(StyledText styled, const DialoguePageProperties& dialogue_properties) :
	styled_text{ std::move(styled) }, properties{ dialogue_properties } {}

DialoguePageProperties DialoguePageProperties::InheritProperties(const json& j) const {
	DialoguePageProperties properties{ *this };
	if (!j.is_object()) {
		return properties;
	}

	if (j.contains("text_defaults")) {
		properties.text_defaults = j.at("text_defaults").get<TextRunDefaults>();
	}

	properties.text_defaults.style.color = j.value("color", properties.text_defaults.style.color);
	properties.text_defaults.font = j.value("font", properties.text_defaults.font);
	properties.text_defaults.style.size = j.value("font_size", properties.text_defaults.style.size);

	properties.scroll_duration = j.value("scroll_duration", properties.scroll_duration);
	properties.box_size = j.value("box_size", properties.box_size);
	properties.horizontal_align = j.value("horizontal_align", properties.horizontal_align);
	properties.vertical_align = j.value("vertical_align", properties.vertical_align);
	properties.wrap_mode = j.value("wrap_mode", properties.wrap_mode);
	properties.overflow_mode = j.value("overflow_mode", properties.overflow_mode);
	properties.padding = j.value("padding", properties.padding);
	return properties;
}

V2_float DialoguePageProperties::TextAreaSize() const {
	return box_size - padding.GetLeftTop() - padding.GetRightBottom();
}

Rect DialoguePageProperties::TextAreaRect() const {
	return Rect{ {}, TextAreaSize() };
}

TextBox DialoguePageProperties::ToTextBox() const {
	TextBox box;
	box.rect = TextAreaRect();
	box.style.alignment.horizontal = horizontal_align;
	box.style.alignment.vertical = vertical_align;
	box.style.wrap.mode = wrap_mode;
	box.style.overflow = overflow_mode;
	return box;
}

void DialoguePageProperties::ApplyToText(Text text) const {
	if (!text.Has<impl::TextData>()) {
		return;
	}

	text.Get<impl::TextData>().defaults = text_defaults;
	text.Box(ToTextBox());
}

void DialogueEntry::ResetRuntimeState() {
	variant_cursor = initial_variant;
	used_variant_indices.clear();
	opened = false;
}

std::size_t DialogueEntry::PickRandomVariantIndex() const {
	PTGN_ASSERT(variants.size() > used_variant_indices.size());
	if (variants.size() == 1) {
		return 0;
	}

	RNG<std::size_t> index_rng{ 0, variants.size() - 1 };
	std::size_t chosen_index{};
	do {
		chosen_index = index_rng();
	} while (std::ranges::contains(used_variant_indices, chosen_index));
	return chosen_index;
}

const DialogueVariant* DialogueEntry::GetCurrentDialogueVariant() const {
	if (variants.empty() || used_variant_indices.empty()) {
		return nullptr;
	}

	const std::size_t current_index{ used_variant_indices.back() };
	PTGN_ASSERT(current_index < variants.size());
	return &variants[current_index];
}

std::optional<std::size_t> DialogueEntry::GetNewDialogueVariant() {
	if (variants.empty()) {
		return std::nullopt;
	}

	if (!repeatable) {
		if (opened) {
			return std::nullopt;
		}

		opened = true;
		used_variant_indices = { 0 };
		return variants.front().pages.empty() ? std::nullopt : std::optional<std::size_t>{ 0 };
	}

	// The first opening is deterministic for both sequential and random behavior.
	// Random behavior only takes over after the authored Initial Variant has been shown.
	if (!opened) {
		opened = true;
		const std::size_t chosen_index{ std::min(initial_variant, variants.size() - 1) };
		used_variant_indices = { chosen_index };
		variant_cursor = Mod(chosen_index + 1, variants.size());
		return variants[chosen_index].pages.empty()
			? std::nullopt
			: std::optional<std::size_t>{ chosen_index };
	}

	std::size_t chosen_index{};
	if (behavior == DialogueBehavior::Sequential) {
		chosen_index = Mod(variant_cursor, variants.size());
		variant_cursor = Mod(chosen_index + 1, variants.size());
		used_variant_indices = { chosen_index };
	} else {
		if (used_variant_indices.size() == variants.size()) {
			const std::size_t previous{ used_variant_indices.back() };
			used_variant_indices.clear();
			if (variants.size() > 1) {
				used_variant_indices.emplace_back(previous);
			}
		}

		chosen_index = PickRandomVariantIndex();
		used_variant_indices.emplace_back(chosen_index);
	}

	return variants[chosen_index].pages.empty()
		? std::nullopt
		: std::optional<std::size_t>{ chosen_index };
}

json DialogueData::MakeDefaultDefinition() {
	DialoguePageProperties properties;
	properties.box_size = V2_float{ 600.0f, 160.0f };

	json root = json::object();
	to_json(root, properties);
	root["continue_key"] = "Enter";
	root["start"] = "dialogue";
	root["scroll"] = true;
	root["dialogues"] = json::object();
	root["dialogues"]["dialogue"] = json{
		{ "repeatable", true },
		{ "next", "" },
		{ "behavior", DialogueBehavior::Sequential },
		{ "initial_variant", 0 },
		{ "variants", json::array({ "" }) },
	};
	return root;
}

const json& DialogueData::Definition() const {
	return definition;
}

void DialogueData::SetDefinition(json value) {
	if (!value.is_object() || value.empty() ||
		!value.contains("dialogues") || !value.at("dialogues").is_object()) {
		value = MakeDefaultDefinition();
	}

	definition = std::move(value);
	runtime_dirty = true;

	continue_keys = "Enter";
	if (definition.contains("continue_key")) {
		const auto& continue_json{ definition.at("continue_key") };
		continue_keys = continue_json.is_string()
			? continue_json.get<std::string>()
			: impl::DialogueKeyName(continue_json.get<Key>());
	}

	current_dialogue = definition.value("start", std::string{});
	dialogues.clear();
	ClearRuntimeState();
}

void DialogueData::MarkRuntimeDirty() {
	runtime_dirty = true;
}

void DialogueData::RebuildRuntime(const Scene& scene) {
	const bool valid_definition{
		definition.is_object() && !definition.empty() &&
		definition.contains("dialogues") &&
		definition.at("dialogues").is_object()
	};
	const json authored = valid_definition
		? definition
		: MakeDefaultDefinition();

	DialoguePageProperties defaults;
	defaults = defaults.InheritProperties(authored);
	LoadFromJson(scene, authored, defaults);
}

void DialogueData::ClearRuntimeState() {
	current_variant = 0;
	current_page = 0;
	open = false;
	for (auto& [_, dialogue] : dialogues) {
		dialogue.ResetRuntimeState();
	}
}

void DialogueData::LoadFromJson(
	const Scene& scene, const json& root, const DialoguePageProperties& default_properties
) {
	dialogues.clear();

	const bool valid_root{
		root.is_object() && !root.empty() &&
		root.contains("dialogues") &&
		root.at("dialogues").is_object()
	};
	const json authored = valid_root
		? root
		: MakeDefaultDefinition();
	auto root_properties{ default_properties.InheritProperties(authored) };

	definition = authored;
	json resolved_properties = root_properties;
	definition.update(resolved_properties);
	runtime_dirty = false;
	PTGN_ASSERT(
		!root_properties.box_size.IsZero(),
		"Dialogue requires either a sprite background or a non-zero box size"
	);

	const std::string split_end{ authored.value("split_end", "...") };
	const std::string split_begin{ authored.value("split_begin", "") };
	const int default_initial_variant{ authored.value("initial_variant", authored.value("index", 0)) };
	PTGN_ASSERT(default_initial_variant >= 0, "Initial variant must be greater than or equal to zero");

	const DialogueBehavior default_behavior{ authored.value("behavior", DialogueBehavior::Sequential) };
	const bool default_repeatable{ authored.value("repeatable", true) };
	const bool default_scroll{ authored.value("scroll", true) };
	const std::string default_next{ authored.value("next", "") };

	if (authored.contains("continue_key")) {
		const auto& continue_json{ authored.at("continue_key") };
		if (continue_json.is_string()) {
			continue_keys = continue_json.get<std::string>();
		} else {
			continue_keys = impl::DialogueKeyName(continue_json.get<Key>());
		}
	}
	PTGN_ASSERT(
		impl::ValidateDialogueKeyExpression(continue_keys),
		"Invalid dialogue continue key expression: ", continue_keys
	);
	current_dialogue = authored.value("start", std::string{});
	definition["continue_key"] = continue_keys;
	definition["start"] = current_dialogue;

	PTGN_ASSERT(authored.contains("dialogues"));
	const auto& dialogues_json{ authored.at("dialogues") };
	PTGN_ASSERT(
		current_dialogue.empty() || dialogues_json.contains(current_dialogue),
		"Start key not found in dialogue json"
	);

	for (const auto& [dialogue_name, dialogue_json] : dialogues_json.items()) {
		DialogueEntry dialogue;
		const json dialogue_settings = dialogue_json.is_object()
			? dialogue_json
			: json::object();
		auto dialogue_properties{
			InheritDialogueLayout(root_properties, dialogue_settings)
		};

		int initial_variant{
			dialogue_settings.value(
				"initial_variant",
				dialogue_settings.value("index", default_initial_variant)
			)
		};
		PTGN_ASSERT(initial_variant >= 0, "Initial variant must be greater than or equal to zero");

		dialogue.repeatable = dialogue_settings.value("repeatable", default_repeatable);
		dialogue.scroll = dialogue_settings.value("scroll", default_scroll);
		dialogue.next_dialogue = dialogue_settings.value("next", default_next);
		dialogue.behavior = dialogue_settings.value("behavior", default_behavior);

		PTGN_ASSERT(
			dialogue.next_dialogue.empty() || dialogues_json.contains(dialogue.next_dialogue),
			"Next key not found in dialogue json"
		);

		auto append_source = [&](DialogueVariant& variant, std::string_view source,
							 const DialoguePageProperties& properties) {
			auto pages{ impl::PaginateDialogueSource(scene.ctx().asset, source, properties, split_end, split_begin) };
			variant.pages.append_range(pages);
		};

		auto append_variants = [&](const json& variants_json) {
			if (variants_json.is_string()) {
				DialogueVariant variant;
				append_source(
					variant,
					variants_json.get<std::string>(),
					dialogue_properties
				);
				dialogue.variants.emplace_back(std::move(variant));
			} else if (variants_json.is_array()) {
				for (const auto& variant_json : variants_json) {
					DialogueVariant variant;
					if (variant_json.is_string()) {
						append_source(
							variant,
							variant_json.get<std::string>(),
							dialogue_properties
						);
					} else if (variant_json.is_object() && variant_json.contains("source")) {
						append_source(
							variant,
							variant_json.at("source").get<std::string>(),
							dialogue_properties
						);
					}
					dialogue.variants.emplace_back(std::move(variant));
				}
			}
		};

		if (!dialogue_json.is_object()) {
			append_variants(dialogue_json);
		} else if (dialogue_json.contains("variants")) {
			append_variants(dialogue_json.at("variants"));
		} else {
			PTGN_ASSERT(dialogue_json.contains("lines"), "Dialogue requires 'variants'");
			const auto& lines_json{ dialogue_json.at("lines") };

			auto append_legacy_line = [&](const json& line_json) {
				DialogueVariant variant;
				if (line_json.is_string()) {
					append_source(variant, line_json.get<std::string>(), dialogue_properties);
				} else if (line_json.is_object()) {
					auto line_properties{ InheritDialogueLayout(dialogue_properties, line_json) };
					PTGN_ASSERT(line_json.contains("pages"));
					const auto& pages_json{ line_json.at("pages") };
					if (pages_json.is_string()) {
						append_source(variant, pages_json.get<std::string>(), line_properties);
					} else if (pages_json.is_array()) {
						for (const auto& page_json : pages_json) {
							if (page_json.is_string()) {
								append_source(variant, page_json.get<std::string>(), line_properties);
							} else if (page_json.is_object()) {
								auto page_properties{
									InheritDialogueLayout(line_properties, page_json)
								};
								if (page_json.contains("text")) {
									const auto& text_json{ page_json.at("text") };
									const std::string source{
										text_json.is_string()
											? text_json.get<std::string>()
											: text_json.at("source").get<std::string>()
									};
									append_source(variant, source, page_properties);
								} else if (page_json.contains("content")) {
									append_source(
										variant,
										page_json.at("content").get<std::string>(),
										page_properties
									);
								}
							}
						}
					}
				}
				dialogue.variants.emplace_back(std::move(variant));
			};

			if (lines_json.is_string()) {
				append_legacy_line(lines_json);
			} else if (lines_json.is_array()) {
				for (const auto& line_json : lines_json) {
					append_legacy_line(line_json);
				}
			}
		}
		if (dialogue.variants.empty()) {
			dialogue.variants.emplace_back();
		}

		initial_variant = std::clamp(initial_variant, 0, static_cast<int>(dialogue.variants.size() - 1));
		dialogue.initial_variant = static_cast<std::size_t>(initial_variant);
		dialogue.ResetRuntimeState();
		dialogues.emplace(dialogue_name, std::move(dialogue));
	}

	ClearRuntimeState();
}

DialogueBox::DialogueBox(Entity entity) : Entity{ entity } {}

DialogueData& DialogueBox::Data() {
	return Get<DialogueData>();
}

const DialogueData& DialogueBox::Data() const {
	return Get<DialogueData>();
}

std::string_view DialogueBox::GetContinueKeys() const {
	return Data().continue_keys;
}

DialogueBox& DialogueBox::SetContinueKeys(std::string_view continue_keys) {
	PTGN_ASSERT(impl::ValidateDialogueKeyExpression(continue_keys));
	auto& data{ Data() };
	data.continue_keys = std::string{ continue_keys };
	if (!data.definition.is_object() || data.definition.empty()) {
		data.definition = DialogueData::MakeDefaultDefinition();
	}
	data.definition["continue_key"] = data.continue_keys;
	data.MarkRuntimeDirty();
	return *this;
}

Key DialogueBox::GetContinueKey() const {
	const auto expression{ ParseDialogueKeyExpression(Data().continue_keys, nullptr) };
	if (!expression.has_value() || expression->empty() || expression->front().empty()) {
		return Key::Enter;
	}
	return expression->front().front();
}

DialogueBox& DialogueBox::SetContinueKey(Key continue_key) {
	return SetContinueKeys(impl::DialogueKeyName(continue_key));
}

bool DialogueBox::IsOpen() const {
	return Data().open;
}

void DialogueBox::EnsureRuntimeData() {
	auto& data{ Data() };
	if (data.runtime_dirty) {
		data.RebuildRuntime(GetScene());
	}

	if (!HasScript<impl::DialogueWaitScript>(*this)) {
		AddScript<impl::DialogueWaitScript>(*this);
	}
}

DialogueBox& DialogueBox::Open(std::string_view dialogue_name) {
	EnsureRuntimeData();
	auto& data{ Data() };
	PTGN_ASSERT(!data.dialogues.empty());

	if (!dialogue_name.empty()) {
		if (dialogue_name == data.current_dialogue && data.open) {
			return *this;
		}
		data.current_dialogue = dialogue_name;
	} else if (data.open) {
		return *this;
	}

	if (data.current_dialogue.empty()) {
		return *this;
	}

	auto* dialogue{ GetCurrentDialogue() };
	if (!dialogue) {
		return *this;
	}

	const auto variant_index{ dialogue->GetNewDialogueVariant() };
	if (!variant_index.has_value()) {
		return *this;
	}

	data.current_variant = variant_index.value();
	data.current_page = 0;
	data.open = true;
	ApplyCurrentPage();
	StartCurrentPageScroll();
	return *this;
}

DialogueBox& DialogueBox::Close() {
	StopCurrentPageScroll();
	auto& data{ Data() };
	data.open = false;
	data.current_variant = 0;
	data.current_page = 0;

	if (auto text{ TryTextPart() }) {
		Hide(text.value());
	}
	if (auto background{ TryBackgroundEntity() }) {
		Hide(background.value());
	}
	return *this;
}

DialogueBox& DialogueBox::NextPage() {
	auto& data{ Data() };
	++data.current_page;
	if (!GetCurrentDialoguePage()) {
		Close();
		SetNextDialogue();
		return *this;
	}
	ApplyCurrentPage();
	StartCurrentPageScroll();
	return *this;
}

DialogueBox& DialogueBox::CompletePage() {
	StopCurrentPageScroll();
	TextPart().Reveal();
	return *this;
}

DialogueBox& DialogueBox::SetDialogue(std::string_view name) {
	EnsureRuntimeData();
	auto& data{ Data() };
	PTGN_ASSERT(name.empty() || data.dialogues.contains(name));
	data.current_dialogue = std::string{ name };
	data.current_variant = 0;
	data.current_page = 0;
	return *this;
}

DialogueBox& DialogueBox::SetNextDialogue() {
	auto& data{ Data() };
	const auto* dialogue{ GetCurrentDialogue() };
	data.current_variant = 0;
	data.current_page = 0;

	if (!dialogue || dialogue->next_dialogue.empty()) {
		return *this;
	}

	PTGN_ASSERT(data.dialogues.contains(dialogue->next_dialogue));
	data.current_dialogue = dialogue->next_dialogue;
	return *this;
}

DialogueEntry* DialogueBox::GetCurrentDialogue() {
	auto& data{ Data() };
	if (data.current_dialogue.empty()) {
		return nullptr;
	}

	auto it{ data.dialogues.find(data.current_dialogue) };
	return it == data.dialogues.end() ? nullptr : &it->second;
}

DialogueVariant* DialogueBox::GetCurrentDialogueVariant() {
	const auto& data{ Data() };
	auto* dialogue{ GetCurrentDialogue() };
	if (!dialogue || data.current_variant >= dialogue->variants.size()) {
		return nullptr;
	}
	return &dialogue->variants[data.current_variant];
}

DialoguePage* DialogueBox::GetCurrentDialoguePage() {
	const auto& data{ Data() };
	auto* variant{ GetCurrentDialogueVariant() };
	if (!variant || data.current_page >= variant->pages.size()) {
		return nullptr;
	}
	return &variant->pages[data.current_page];
}

std::optional<Entity> DialogueBox::TryPart(DialoguePartRole role) const {
	return FindDialoguePart(*this, role);
}

Entity DialogueBox::Part(DialoguePartRole role) {
	if (auto part{ TryPart(role) }) {
		return part.value();
	}

	Entity entity{ GetScene().CreateEntity() };
	switch (role) {
		case DialoguePartRole::Text: entity.Add<Tag>("Dialogue Text"); break;
		case DialoguePartRole::Background: entity.Add<Tag>("Dialogue Sprite"); break;
		default: PTGN_ERROR("Unknown DialoguePartRole: ", std::to_underlying(role));
	}
	entity.Add<impl::DialoguePart>(role);
	SetParent(entity, *this);
	return entity;
}

Text DialogueBox::TextPart() {
	if (auto text{ TryTextPart() }) {
		return text.value();
	}

	Text text{ CreateText(GetScene(), {}, {}, Origin::TopLeft) };
	text.Add<Tag>("Dialogue Text");
	text.Add<impl::DialoguePart>(DialoguePartRole::Text);
	SetParent(text, *this);
	Hide(text);
	return text;
}

std::optional<Text> DialogueBox::TryTextPart() const {
	auto part{ TryPart(DialoguePartRole::Text) };
	return part.has_value() ? std::optional<Text>{ Text{ part.value() } } : std::nullopt;
}

std::optional<Sprite> DialogueBox::TryBackground() const {
	auto part{ TryPart(DialoguePartRole::Background) };
	if (!part.has_value() || !part.value().HasAny<Texture, TextureKey>()) {
		return std::nullopt;
	}
	return Sprite{ part.value() };
}

std::optional<Entity> DialogueBox::TryBackgroundEntity() const {
	return TryPart(DialoguePartRole::Background);
}

void DialogueBox::ApplyCurrentPage() {
	const auto* page{ GetCurrentDialoguePage() };
	if (!page) {
		Close();
		return;
	}

	Text text{ TextPart() };
	page->properties.ApplyToText(text);
	text.Clear().Content(page->styled_text).Reveal(0);
	PositionTextForPage(page->properties);
	Show(text);
	if (auto background{ TryBackgroundEntity() }) {
		Show(background.value());
	}
}

void DialogueBox::StartCurrentPageScroll() {
	const auto* page{ GetCurrentDialoguePage() };
	const auto* dialogue{ GetCurrentDialogue() };
	if (!page || !dialogue) {
		Close();
		return;
	}

	StopCurrentPageScroll();
	if (page->instant || !dialogue->scroll || page->properties.scroll_duration <= 0ms) {
		TextPart().Reveal();
		return;
	}

	ScriptSequence sequence{ "Dialogue Text Reveal" };
	sequence.Transient().During(
		static_cast<float>(page->properties.scroll_duration.count()),
		impl::DialogueScrollScript{}
	);
	script_runtime::RunInChannel(
		*this, SequenceChannelKey{ kDialogueScrollChannel }, std::move(sequence), ReentryMode::Restart
	);
}

void DialogueBox::StopCurrentPageScroll() {
	if (!*this) {
		return;
	}
	script_runtime::StopChannel(
		*this, SequenceChannelKey{ kDialogueScrollChannel }, SequenceStopMode::All
	);
}

void DialogueBox::PositionTextForPage(const DialoguePageProperties& properties) {
	Rect outer_rect{ properties.box_size, GetOrDefault<Origin>() };
	Rect content_rect{ outer_rect.Expanded(
		-properties.padding.GetLeftTop(), -properties.padding.GetRightBottom()
	) };
	Text text{ TextPart() };
	SetPosition(text, content_rect.min);
	text.Add<Origin>(Origin::TopLeft);
	text.Box(Rect{ {}, content_rect.GetSize() });
}

DialogueBox CreateDialogueBox(Scene& scene, Transform transform, const DialogueDesc& desc) {
	const bool has_authored_definition{
		desc.data.is_object() && !desc.data.empty() &&
		desc.data.contains("dialogues") &&
		desc.data.at("dialogues").is_object()
	};
	json definition = has_authored_definition
		? desc.data
		: DialogueData::MakeDefaultDefinition();

	if (!has_authored_definition && desc.box_size.IsPositive()) {
		definition["box_size"] = desc.box_size;
	}

	DialoguePageProperties default_properties;
	default_properties.box_size = desc.box_size;
	default_properties = default_properties.InheritProperties(definition);

	DialogueBox dialogue{ scene.CreateEntity() };
	dialogue.Add<Tag>("Dialogue Box");
	dialogue.Add<DialogueData>();
	dialogue.Add<Transform>(transform);
	dialogue.Add<Origin>(desc.origin);

	if (desc.ui_layer) {
		SetUI(dialogue, true);
	}

	if (desc.background_texture.has_value()) {
		Sprite background{ CreateSprite(scene, {}, desc.background_texture.value(), Origin::Center) };
		background.Add<Tag>("Dialogue Sprite");
		background.Add<impl::DialoguePart>(DialoguePartRole::Background);
		SetParent(background, dialogue);

		if (auto size{ GetDisplaySize(background) }; size.has_value() && size.value().IsPositive()) {
			default_properties.box_size = size.value();
			if (!has_authored_definition) {
				definition["box_size"] = size.value();
			}
		}

		SetPosition(background, GetOffset(desc.origin, default_properties.box_size));
		Hide(background);
	} else if (default_properties.box_size.IsPositive()) {
		Entity background{ scene.CreateEntity() };
		background.Add<Tag>("Dialogue Background");
		background.Add<Rect>(Rect{ default_properties.box_size });
		background.Add<impl::DialoguePart>(DialoguePartRole::Background);
		background.Add<Color>(desc.background_color);
		background.Add<Origin>(Origin::Center);
		SetDraw<RectDraw>(background);
		SetPosition(background, GetOffset(desc.origin, default_properties.box_size));
		SetParent(background, dialogue);
		Hide(background);
	}

	dialogue.TextPart();
	dialogue.Data().LoadFromJson(scene, definition, default_properties);
	if (!HasScript<impl::DialogueWaitScript>(dialogue)) {
		AddScript<impl::DialogueWaitScript>(dialogue);
	}
	dialogue.Close();
	return dialogue;
}

void to_json(json& j, const DialoguePageProperties& properties) {
	j = json{
		{ "text_defaults", properties.text_defaults },
		{ "box_size", properties.box_size },
		{ "padding", properties.padding },
		{ "scroll_duration", properties.scroll_duration },
		{ "horizontal_align", properties.horizontal_align },
		{ "vertical_align", properties.vertical_align },
		{ "wrap_mode", properties.wrap_mode },
		{ "overflow_mode", properties.overflow_mode },
	};
}

void from_json(const json& j, DialoguePageProperties& properties) {
	properties = properties.InheritProperties(j);
}

void to_json(json& j, const DialoguePage& page) {
	j = json{
		{ "styled_text", page.styled_text },
		{ "properties", page.properties },
		{ "instant", page.instant },
	};
}

void from_json(const json& j, DialoguePage& page) {
	if (!j.is_object()) {
		page = DialoguePage{};
		return;
	}

	page.styled_text = j.value("styled_text", StyledText{});
	page.instant = j.value("instant", false);
	if (j.contains("properties")) {
		page.properties = j.at("properties").get<DialoguePageProperties>();
	}
}

void to_json(json& j, const DialogueVariant& variant) {
	j = json{ { "pages", variant.pages } };
}

void from_json(const json& j, DialogueVariant& variant) {
	variant.pages.clear();
	if (!j.is_object()) {
		return;
	}
	if (j.contains("pages")) {
		variant.pages = j.at("pages").get<std::vector<DialoguePage>>();
	}
}

void to_json(json& j, const DialogueEntry& dialogue) {
	j = json{
		{ "initial_variant", dialogue.initial_variant },
		{ "repeatable", dialogue.repeatable },
		{ "behavior", dialogue.behavior },
		{ "scroll", dialogue.scroll },
		{ "next", dialogue.next_dialogue },
		{ "variants", dialogue.variants },
	};
}

void from_json(const json& j, DialogueEntry& dialogue) {
	if (!j.is_object()) {
		dialogue = DialogueEntry{};
		dialogue.ResetRuntimeState();
		return;
	}

	dialogue.initial_variant = j.value("initial_variant", j.value("index", 0uz));
	dialogue.repeatable = j.value("repeatable", true);
	dialogue.behavior = j.value("behavior", DialogueBehavior::Sequential);
	dialogue.scroll = j.value("scroll", true);
	dialogue.next_dialogue = j.value("next", std::string{});
	dialogue.variants.clear();
	if (j.contains("variants")) {
		dialogue.variants = j.at("variants").get<std::vector<DialogueVariant>>();
	} else if (j.contains("lines")) {
		dialogue.variants = j.at("lines").get<std::vector<DialogueVariant>>();
	}
	if (!dialogue.variants.empty()) {
		dialogue.initial_variant = std::min(dialogue.initial_variant, dialogue.variants.size() - 1);
	} else {
		dialogue.initial_variant = 0;
	}
	dialogue.ResetRuntimeState();
}

void to_json(json& j, const DialogueData& data) {
	j = data.definition.is_object() && !data.definition.empty()
		? data.definition
		: DialogueData::MakeDefaultDefinition();
}

void from_json(const json& j, DialogueData& data) {
	if (!j.is_object()) {
		data.SetDefinition(DialogueData::MakeDefaultDefinition());
		return;
	}

	// DialogueData now serializes its authoring definition. Migrate the previous runtime-page
	// component shape once so existing scenes retain their text when opened in the new inspector.
	const bool legacy_runtime_shape{
		j.contains("current_variant") ||
		j.contains("current_line") ||
		j.contains("current_page") ||
		j.contains("open")
	};

	if (!legacy_runtime_shape) {
		data.SetDefinition(j);
		return;
	}

	json definition = DialogueData::MakeDefaultDefinition();
	definition["continue_key"] = j.value(
		"continue_key",
		json("Enter")
	);
	definition["start"] = j.value(
		"current_dialogue",
		std::string{}
	);
	definition["dialogues"] = json::object();

	bool copied_root_properties{ false };
	bool copied_root_typewriter{ false };

	if (j.contains("dialogues") && j.at("dialogues").is_object()) {
		for (const auto& [name, entry_json] : j.at("dialogues").items()) {
			DialogueEntry entry{ entry_json.get<DialogueEntry>() };

			json authored{
				{ "initial_variant", entry.initial_variant },
				{ "repeatable", entry.repeatable },
				{ "behavior", entry.behavior },
				{ "next", entry.next_dialogue },
				{ "variants", json::array() },
			};

			if (!copied_root_typewriter) {
				definition["scroll"] = entry.scroll;
				copied_root_typewriter = true;
			}

			for (const auto& variant : entry.variants) {
				std::string source;
				for (const auto& page : variant.pages) {
					if (page.instant) {
						if (!source.empty()) {
							source += "\n";
						}
						source += std::string{ impl::kDialogueInstantPageTag };
						source += "\n";
					} else if (!source.empty()) {
						source += "\n\n";
					}
					source += SerializeStyledTextToRichText(
						page.styled_text,
						page.properties.text_defaults
					);

					if (!copied_root_properties) {
						json properties = page.properties;
						definition.update(properties);
						copied_root_properties = true;
					}
				}
				authored["variants"].push_back(std::move(source));
			}

			if (authored["variants"].empty()) {
				authored["variants"].push_back("");
			}

			definition["dialogues"][name] = std::move(authored);
		}
	}

	if (definition["start"].get<std::string>().empty() &&
		!definition["dialogues"].empty()) {
		definition["start"] = definition["dialogues"].begin().key();
	}

	data.SetDefinition(std::move(definition));
}

} // namespace ptgn
