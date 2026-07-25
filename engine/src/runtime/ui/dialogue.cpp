#include "runtime/ui/dialogue.h"

#include <algorithm>
#include <chrono>
#include <list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

void StopDialogueScroll(Entity dialogue) {
	if (!dialogue) {
		return;
	}
	script_runtime::StopChannel(
		dialogue, SequenceChannelKey{ kDialogueScrollChannel }, SequenceStopMode::All
	);
}

[[nodiscard]] Rect GetLocalRect(Origin origin, V2_float size) {
	V2_float center{ GetOffset(origin, size) };
	V2_float half_size{ size * 0.5f };

	return Rect{
		center - half_size,
		center + half_size,
	};
}

[[nodiscard]] Rect ApplyPadding(Rect rect, Rect padding) {
	rect.min += padding.min;
	rect.max -= padding.max;
	return rect;
}

[[nodiscard]] std::optional<Entity> FindDialoguePart(Entity dialogue, DialoguePartRole role) {
	if (!HasChildren(dialogue)) {
		return std::nullopt;
	}

	for (Entity child : GetChildren(dialogue)) {
		auto part{ child.TryGet<impl::DialoguePart>() };

		if (!part) {
			continue;
		}

		if (part->role == role) {
			return child;
		}
	}

	return std::nullopt;
}

[[nodiscard]] std::vector<DialoguePage> PaginateDialogueText(
	const Scene& scene, const StyledText& styled_text, const DialoguePageProperties& properties,
	std::string_view split_end, std::string_view split_begin
) {
	auto pagination{ impl::PaginateText(
		scene.ctx().asset, styled_text, properties.ToTextBox(),
		impl::TextPageOptions{
			.split_end			= std::string{ split_end },
			.split_begin		= std::string{ split_begin },
			.max_lines_per_page = 0,
			.add_split_markers	= true,
		}
	) };

	std::vector<DialoguePage> pages;
	pages.reserve(pagination.pages.size());

	for (auto& page : pagination.pages) {
		pages.emplace_back(std::move(page.styled_text), properties);
	}

	return pages;
}

[[nodiscard]] std::vector<DialoguePage> PaginateDialogueText(
	const Scene& scene, std::string_view content, const DialoguePageProperties& properties,
	std::string_view split_end, std::string_view split_begin
) {
	StyledText styled_text;

	styled_text.runs.emplace_back(
		TextRun{
			.text  = std::string{ content },
			.font  = properties.font,
			.style = properties.ToTextRunStyle(),
		}
	);

	return PaginateDialogueText(scene, styled_text, properties, split_end, split_begin);
}

} // namespace

namespace impl {

void DialogueWaitScript::OnEvent(Event event) {
	event.Dispatch<event::KeyPressed>(&DialogueWaitScript::OnKeyPressed, this);
}

void DialogueWaitScript::OnKeyPressed(Key key) const {
	DialogueBox dialogue{ entity };

	if (!dialogue.IsOpen()) {
		return;
	}

	if (key != dialogue.GetContinueKey()) {
		return;
	}

	if (Text text{ dialogue.TextPart() }; !text.IsFullyRevealed()) {
		dialogue.CompletePage();
		return;
	}

	dialogue.NextPage();
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

DialoguePage::DialoguePage(StyledText styled_text, const DialoguePageProperties& properties) :
	styled_text{ std::move(styled_text) }, properties{ properties } {}

DialoguePageProperties DialoguePageProperties::InheritProperties(const json& j) const {
	DialoguePageProperties properties{ *this };

	properties.color		   = j.value("color", properties.color);
	properties.scroll_duration = j.value("scroll_duration", properties.scroll_duration);
	properties.box_size		   = j.value("box_size", properties.box_size);
	properties.font			   = j.value("font", properties.font);
	properties.font_size	   = j.value("font_size", properties.font_size);

	properties.horizontal_align = j.value("horizontal_align", properties.horizontal_align);
	properties.vertical_align	= j.value("vertical_align", properties.vertical_align);
	properties.wrap_mode		= j.value("wrap_mode", properties.wrap_mode);
	properties.overflow_mode	= j.value("overflow_mode", properties.overflow_mode);

	if (j.contains("padding")) {
		auto padding_json{ j.at("padding").get<int>() };
		properties.SetPadding(padding_json);
	}

	if (j.contains("padding_x")) {
		auto padding_x{ j.at("padding_x").get<int>() };
		properties.padding.min.x = static_cast<float>(padding_x);
		properties.padding.max.x = static_cast<float>(padding_x);
	}

	if (j.contains("padding_y")) {
		auto padding_y{ j.at("padding_y").get<int>() };
		properties.padding.min.y = static_cast<float>(padding_y);
		properties.padding.max.y = static_cast<float>(padding_y);
	}

	properties.padding.min.x =
		static_cast<float>(j.value("padding_left", static_cast<int>(properties.padding.min.x)));
	properties.padding.max.x =
		static_cast<float>(j.value("padding_right", static_cast<int>(properties.padding.max.x)));
	properties.padding.min.y =
		static_cast<float>(j.value("padding_top", static_cast<int>(properties.padding.min.y)));
	properties.padding.max.y =
		static_cast<float>(j.value("padding_bottom", static_cast<int>(properties.padding.max.y)));

	return properties;
}

void DialoguePageProperties::SetPadding(int padding_value) {
	SetPadding(padding_value, padding_value, padding_value, padding_value);
}

void DialoguePageProperties::SetPadding(V2_int padding_value) {
	padding.min =
		V2_float{ static_cast<float>(padding_value.x), static_cast<float>(padding_value.y) };
	padding.max =
		V2_float{ static_cast<float>(padding_value.x), static_cast<float>(padding_value.y) };
}

void DialoguePageProperties::SetPadding(int top, int right, int bottom, int left) {
	padding.min = V2_float{ static_cast<float>(left), static_cast<float>(top) };
	padding.max = V2_float{ static_cast<float>(right), static_cast<float>(bottom) };
}

V2_float DialoguePageProperties::TextAreaSize() const {
	return box_size - padding.min - padding.max;
}

Rect DialoguePageProperties::TextAreaRect() const {
	return Rect{ {}, TextAreaSize() };
}

TextBox DialoguePageProperties::ToTextBox() const {
	TextBox box;
	box.rect = TextAreaRect();

	box.style.alignment.horizontal = horizontal_align;
	box.style.alignment.vertical   = vertical_align;
	box.style.wrap.mode			   = wrap_mode;
	box.style.overflow			   = overflow_mode;

	return box;
}

TextRunStyle DialoguePageProperties::ToTextRunStyle() const {
	TextRunStyle style;
	style.color = color;
	style.size	= font_size;
	return style;
}

void DialoguePageProperties::ApplyToText(Text text) const {
	text.Font(font)
		.Color(color)
		.Size(font_size)
		.Box(ToTextBox().rect)
		.Align(horizontal_align, vertical_align)
		.Wrap(wrap_mode)
		.Overflow(overflow_mode);
}

std::size_t DialogueEntry::PickRandomIndex() const {
	PTGN_ASSERT(lines.size() > used_line_indices.size());

	if (lines.size() == 1) {
		return 0;
	}

	RNG<std::size_t> index_rng{ 0, lines.size() - 1 };

	auto chosen_index{ index };

	do {
		chosen_index = index_rng();
	} while (std::ranges::contains(used_line_indices, chosen_index));

	return chosen_index;
}

const DialogueLine* DialogueEntry::GetCurrentDialogueLine() const {
	PTGN_ASSERT(!lines.empty());
	PTGN_ASSERT(!used_line_indices.empty());

	auto current_index{ Mod(index, lines.size()) };

	PTGN_ASSERT(current_index < lines.size());
	PTGN_ASSERT(std::ranges::contains(used_line_indices, current_index));

	return &lines[current_index];
}

std::optional<std::size_t> DialogueEntry::GetNewDialogueLine() {
	if (lines.empty()) {
		return std::nullopt;
	}

	if (lines.size() == used_line_indices.size()) {
		if (!repeatable) {
			return std::nullopt;
		}

		used_line_indices.clear();

		if (lines.size() > 1 && behavior == DialogueBehavior::Random) {
			used_line_indices.emplace_back(index);
		}
	}

	auto chosen_index{ index };

	switch (behavior) {
		using enum DialogueBehavior;

		case Sequential:
			chosen_index = Mod(chosen_index, lines.size());
			++index;
			break;

		case Random:
			chosen_index = PickRandomIndex();
			index		 = chosen_index;
			break;
	}

	PTGN_ASSERT(chosen_index < lines.size());
	PTGN_ASSERT(!std::ranges::contains(used_line_indices, chosen_index));

	used_line_indices.emplace_back(chosen_index);

	if (lines[chosen_index].pages.empty()) {
		return std::nullopt;
	}

	return chosen_index;
}

void DialogueData::ClearRuntimeState() {
	current_line = 0;
	current_page = 0;
	open		 = false;
}

void DialogueData::LoadFromJson(
	const Scene& scene, const json& root, const DialoguePageProperties& default_properties
) {
	dialogues.clear();

	auto root_properties{ default_properties.InheritProperties(root) };

	PTGN_ASSERT(
		!root_properties.box_size.IsZero(),
		"Dialogue requires either a sprite background or a non-zero box size"
	);

	std::string split_end{ root.value("split_end", "...") };
	std::string split_begin{ root.value("split_begin", ",,,") };

	auto default_index{ root.value("index", 0) };

	PTGN_ASSERT(default_index >= 0, "Index must be greater than or equal to zero");

	auto behavior{ root.value("behavior", DialogueBehavior::Sequential) };
	auto repeatable{ root.value("repeatable", true) };
	auto scroll{ root.value("scroll", true) };

	std::string next{ root.value("next", "") };

	continue_key	 = root.value("continue_key", continue_key);
	current_dialogue = root.value("start", std::string{});

	PTGN_ASSERT(root.contains("dialogues"));

	const auto& dialogues_json{ root.at("dialogues") };

	PTGN_ASSERT(
		current_dialogue.empty() || dialogues_json.contains(current_dialogue),
		"Start key not found in dialogue json"
	);

	PTGN_ASSERT(
		next.empty() || dialogues_json.contains(next), "Next key not found in dialogue json"
	);

	for (const auto& [dialogue_name, dialogue_json] : dialogues_json.items()) {
		DialogueEntry dialogue;

		auto dialogue_properties{ root_properties.InheritProperties(dialogue_json) };

		auto index{ dialogue_json.value("index", default_index) };

		dialogue.repeatable	   = dialogue_json.value("repeatable", repeatable);
		dialogue.scroll		   = dialogue_json.value("scroll", scroll);
		dialogue.next_dialogue = dialogue_json.value("next", next);
		dialogue.behavior	   = dialogue_json.value("behavior", behavior);

		PTGN_ASSERT(
			dialogue.next_dialogue.empty() || dialogues_json.contains(dialogue.next_dialogue),
			"Next key not found in dialogue json"
		);

		PTGN_ASSERT(dialogue_json.contains("lines"));

		const auto& lines_json{ dialogue_json.at("lines") };

		auto append_pages = [&](DialogueLine& line, std::string_view content,
								const DialoguePageProperties& properties) {
			auto pages{ PaginateDialogueText(scene, content, properties, split_end, split_begin) };

			line.pages.append_range(pages);
		};

		if (lines_json.is_string()) {
			DialogueLine line;
			append_pages(line, lines_json.get<std::string>(), dialogue_properties);
			dialogue.lines.emplace_back(std::move(line));
		} else if (lines_json.is_array()) {
			for (const auto& line_json : lines_json) {
				DialogueLine line;

				if (line_json.is_string()) {
					append_pages(line, line_json.get<std::string>(), dialogue_properties);
				} else if (line_json.is_object()) {
					auto line_properties{ dialogue_properties.InheritProperties(line_json) };

					PTGN_ASSERT(line_json.contains("pages"));

					const auto& pages_json{ line_json.at("pages") };

					if (pages_json.is_string()) {
						append_pages(line, pages_json.get<std::string>(), line_properties);
					} else if (pages_json.is_array()) {
						for (const auto& page_json : pages_json) {
							if (page_json.is_string()) {
								append_pages(line, page_json.get<std::string>(), line_properties);
							} else if (page_json.is_object()) {
								PTGN_ASSERT(page_json.contains("content"));

								auto page_properties{
									line_properties.InheritProperties(page_json)
								};

								append_pages(
									line, page_json.at("content").get<std::string>(),
									page_properties
								);
							}
						}
					}
				}

				dialogue.lines.emplace_back(std::move(line));
			}
		}

		PTGN_ASSERT(index >= 0, "Index must be greater than or equal to zero");

		if (!dialogue.lines.empty()) {
			if (dialogue.behavior == DialogueBehavior::Sequential &&
				index >= static_cast<int>(dialogue.lines.size())) {
				PTGN_WARN(
					"Dialogue index ", index, " out of range of '", dialogue_name,
					"'; clamping to ", dialogue.lines.size() - 1
				);
			}

			index = std::clamp(index, 0, static_cast<int>(dialogue.lines.size() - 1));
		} else {
			index = 0;
		}

		dialogue.index = static_cast<std::size_t>(index);

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

Key DialogueBox::GetContinueKey() const {
	return Data().continue_key;
}

DialogueBox& DialogueBox::SetContinueKey(Key continue_key) {
	Data().continue_key = continue_key;
	return *this;
}

bool DialogueBox::IsOpen() const {
	return Data().open;
}

DialogueBox& DialogueBox::Open(std::string_view dialogue_name) {
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

	auto dialogue_line_index{ dialogue->GetNewDialogueLine() };

	if (!dialogue_line_index.has_value()) {
		return *this;
	}

	data.current_line = dialogue_line_index.value();
	data.current_page = 0;
	data.open		  = true;

	ApplyCurrentPage();
	StartCurrentPageScroll();

	return *this;
}

DialogueBox& DialogueBox::Close() {
	StopCurrentPageScroll();

	auto& data{ Data() };

	data.open		  = false;
	data.current_line = 0;
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

	if (auto page{ GetCurrentDialoguePage() }; !page) {
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
	auto& data{ Data() };

	PTGN_ASSERT(name.empty() || data.dialogues.contains(name));

	data.current_dialogue = std::string{ name };
	data.current_line	  = 0;
	data.current_page	  = 0;

	return *this;
}

DialogueBox& DialogueBox::SetNextDialogue() {
	auto& data{ Data() };

	const auto* dialogue{ GetCurrentDialogue() };

	if (!dialogue) {
		data.current_line = 0;
		data.current_page = 0;
		return *this;
	}

	if (dialogue->next_dialogue.empty()) {
		data.current_line = 0;
		data.current_page = 0;
		return *this;
	}

	PTGN_ASSERT(data.dialogues.contains(dialogue->next_dialogue));

	data.current_dialogue = dialogue->next_dialogue;
	data.current_line	  = 0;
	data.current_page	  = 0;

	return *this;
}

DialogueEntry* DialogueBox::GetCurrentDialogue() {
	auto& data{ Data() };

	if (data.current_dialogue.empty()) {
		return nullptr;
	}

	auto it{ data.dialogues.find(data.current_dialogue) };

	if (it == data.dialogues.end()) {
		return nullptr;
	}

	return &it->second;
}

DialogueLine* DialogueBox::GetCurrentDialogueLine() {
	const auto& data{ Data() };

	auto* dialogue{ GetCurrentDialogue() };

	if (!dialogue || data.current_line >= dialogue->lines.size()) {
		return nullptr;
	}

	return &dialogue->lines[data.current_line];
}

DialoguePage* DialogueBox::GetCurrentDialoguePage() {
	const auto& data{ Data() };

	auto* line{ GetCurrentDialogueLine() };

	if (!line || data.current_page >= line->pages.size()) {
		return nullptr;
	}

	return &line->pages[data.current_page];
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
		case DialoguePartRole::Text:		 entity.Add<Tag>("Dialogue Text"); break;
		case DialoguePartRole::Background: entity.Add<Tag>("Dialogue Sprite"); break;
		default:							 PTGN_ERROR("Unknown DialoguePartRole: ", std::to_underlying(role));
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

	if (!part.has_value()) {
		return std::nullopt;
	}

	return Text{ part.value() };
}

std::optional<Sprite> DialogueBox::TryBackground() const {
	auto part{ TryPart(DialoguePartRole::Background) };

	if (!part.has_value()) {
		return std::nullopt;
	}

	if (!part.value().HasAny<Texture, TextureKey>()) {
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

	text.Clear()
		.Content(page->styled_text)
		.Box(page->properties.TextAreaRect())
		.Align(page->properties.horizontal_align, page->properties.vertical_align)
		.Wrap(page->properties.wrap_mode)
		.Overflow(page->properties.overflow_mode)
		.Reveal(0);

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

	if (!dialogue->scroll || page->properties.scroll_duration <= 0ms) {
		TextPart().Reveal();
		return;
	}

	ScriptSequence sequence{ "Dialogue Text Reveal" };
	sequence
		.Transient()
		.During(
			static_cast<float>(page->properties.scroll_duration.count()),
			impl::DialogueScrollScript{}
		);

	script_runtime::RunInChannel(
		*this, SequenceChannelKey{ kDialogueScrollChannel }, std::move(sequence),
		ReentryMode::Restart
	);
}

void DialogueBox::StopCurrentPageScroll() {
	StopDialogueScroll(*this);
}

void DialogueBox::PositionTextForPage(const DialoguePageProperties& properties) {
	Rect outer_rect{ GetLocalRect(GetOrDefault<Origin>(), properties.box_size) };
	Rect content_rect{ ApplyPadding(outer_rect, properties.padding) };

	Text text{ TextPart() };

	SetPosition(text, content_rect.min);
	text.Add<Origin>(Origin::TopLeft);

	text.Box(Rect{ {}, content_rect.GetSize() });
}

DialogueBox CreateDialogueBox(Scene& scene, Transform transform, const DialogueDesc& desc) {
	DialogueBox dialogue{ scene.CreateEntity() };

	dialogue.Add<Tag>("Dialogue Box");
	dialogue.Add<DialogueData>();
	dialogue.Add<Transform>(transform);
	dialogue.Add<Origin>(desc.origin);

	if (desc.ui_layer) {
		SetUI(dialogue, true);
	}

	DialoguePageProperties default_properties;
	default_properties.box_size = desc.box_size;

	if (desc.background_texture.has_value()) {
		Sprite background{
			CreateSprite(scene, {}, desc.background_texture.value(), Origin::Center)
		};
		background.Add<Tag>("Dialogue Sprite");
		background.Add<impl::DialoguePart>(DialoguePartRole::Background);
		SetParent(background, dialogue);

		if (auto size{ GetDisplaySize(background) };
			size.has_value() && size.value().IsPositive()) {
			default_properties.box_size = size.value();
		}

		SetPosition(background, GetOffset(desc.origin, default_properties.box_size));
		Hide(background);
	} else if (desc.box_size.IsPositive()) {
		Entity background{ scene.CreateEntity() };

		background.Add<Tag>("Dialogue Background");
		background.Add<Rect>(Rect{ desc.box_size });
		background.Add<impl::DialoguePart>(DialoguePartRole::Background);
		background.Add<Color>(desc.background_color);
		background.Add<Origin>(Origin::Center);
		SetDraw<RectDraw>(background);
		SetPosition(background, GetOffset(desc.origin, desc.box_size));
		SetParent(background, dialogue);
		Hide(background);
	}

	dialogue.TextPart();

	dialogue.Data().LoadFromJson(scene, desc.data, default_properties);

	if (!HasScript<impl::DialogueWaitScript>(dialogue)) {
		AddScript<impl::DialogueWaitScript>(dialogue);
	}

	dialogue.Close();

	return dialogue;
}

void to_json(json& j, const DialoguePageProperties& properties) {
	j = json{
		{ "color", properties.color },
		{ "font", properties.font },
		{ "font_size", properties.font_size },
		{ "box_size", properties.box_size },
		{ "padding_left", properties.padding.min.x },
		{ "padding_right", properties.padding.max.x },
		{ "padding_top", properties.padding.min.y },
		{ "padding_bottom", properties.padding.max.y },
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
	};
}

void from_json(const json& j, DialoguePage& page) {
	page.styled_text = j.value("styled_text", StyledText{});

	if (j.contains("properties")) {
		page.properties = j.at("properties").get<DialoguePageProperties>();
	}
}

void to_json(json& j, const DialogueLine& line) {
	j = json{
		{ "pages", line.pages },
	};
}

void from_json(const json& j, DialogueLine& line) {
	line.pages.clear();

	if (j.contains("pages")) {
		line.pages = j.at("pages").get<std::vector<DialoguePage>>();
	}
}

void to_json(json& j, const DialogueEntry& dialogue) {
	j = json{
		{ "index", dialogue.index },		{ "repeatable", dialogue.repeatable },
		{ "behavior", dialogue.behavior },	{ "scroll", dialogue.scroll },
		{ "next", dialogue.next_dialogue }, { "lines", dialogue.lines },
	};
}

void from_json(const json& j, DialogueEntry& dialogue) {
	dialogue.index		   = j.value("index", 0uz);
	dialogue.repeatable	   = j.value("repeatable", true);
	dialogue.behavior	   = j.value("behavior", DialogueBehavior::Sequential);
	dialogue.scroll		   = j.value("scroll", true);
	dialogue.next_dialogue = j.value("next", std::string{});

	dialogue.lines.clear();

	if (j.contains("lines")) {
		dialogue.lines = j.at("lines").get<std::vector<DialogueLine>>();
	}

	dialogue.used_line_indices.clear();
}

void to_json(json& j, const DialogueData& data) {
	j = json{
		{ "continue_key", data.continue_key },
		{ "current_line", data.current_line },
		{ "current_page", data.current_page },
		{ "current_dialogue", data.current_dialogue },
		{ "open", data.open },
		{ "dialogues", data.dialogues },
	};
}

void from_json(const json& j, DialogueData& data) {
	data.continue_key	  = j.value("continue_key", Key::Enter);
	data.current_line	  = j.value("current_line", 0uz);
	data.current_page	  = j.value("current_page", 0uz);
	data.current_dialogue = j.value("current_dialogue", std::string{});
	data.open			  = j.value("open", false);

	data.dialogues.clear();

	if (j.contains("dialogues")) {
		data.dialogues = j.at("dialogues").get<DialogueMap>();
	}
}

} // namespace ptgn