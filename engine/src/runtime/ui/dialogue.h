#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "core/util/time.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"
#include "serialization/serialize.h"

namespace ptgn {

class DialogueBox;
class Scene;

enum class DialogueBehavior {
	Sequential,
	Random
};
PTGN_SERIALIZE_ENUM(DialogueBehavior);

enum class DialoguePartRole : std::uint8_t {
	Background,
	Text,
	Tween,
};
PTGN_SERIALIZE_ENUM(DialoguePartRole);

namespace impl {

struct DialoguePart {
	DialoguePartRole role{ DialoguePartRole::Text };

	PTGN_SERIALIZE(DialoguePart, role)
};

struct DialogueWaitScript : public Script {
	void OnEvent(Event event) override;

private:
	void OnKeyPressed(Key key);
};

struct DialogueScrollScript : public Script {
	void OnEvent(Event event) override;

private:
	void OnPointComplete() const;
	void OnProgress(float elapsed_fraction) const;
};

} // namespace impl

struct DialoguePageProperties {
	DialoguePageProperties() = default;

	bool operator==(const DialoguePageProperties&) const = default;

	[[nodiscard]] DialoguePageProperties InheritProperties(const json& j) const;

	void SetPadding(int padding);
	void SetPadding(V2_int padding);
	void SetPadding(int top, int right, int bottom, int left);

	[[nodiscard]] V2_float TextAreaSize() const;
	[[nodiscard]] Rect TextAreaRect() const;
	[[nodiscard]] TextBox ToTextBox() const;
	[[nodiscard]] TextRunStyle ToTextRunStyle() const;
	void ApplyToText(Text text) const;

	Color color{ color::White };
	std::string font_key;
	float font_size{ kDefaultFontSize };

	V2_float box_size;
	Rect padding;

	milliseconds scroll_duration{ 1000 };

	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::Word };
	// TODO: Fix clipping of side of characters.
	OverflowMode overflow_mode{ OverflowMode::Clip };
};

struct DialoguePage {
	DialoguePage() = default;
	DialoguePage(std::string_view content, const DialoguePageProperties& properties);

	std::string content;
	DialoguePageProperties properties;
};

struct DialogueLine {
	std::vector<DialoguePage> pages;
};

struct DialogueEntry {
	std::size_t index{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next_dialogue;

	std::vector<DialogueLine> lines;
	std::vector<std::size_t> used_line_indices;

	[[nodiscard]] std::size_t PickRandomIndex() const;
	[[nodiscard]] const DialogueLine* GetCurrentDialogueLine() const;

	std::optional<std::size_t> GetNewDialogueLine();
};

using DialogueMap = std::unordered_map<std::string, DialogueEntry, StringHash, std::equal_to<>>;

struct DialogueData {
	Key continue_key{ Key::Enter };

	std::size_t current_line{ 0 };
	std::size_t current_page{ 0 };
	std::string current_dialogue;

	bool open{ false };

	DialogueMap dialogues;

	void ClearRuntimeState();
	void LoadFromJson(
		const Scene& scene, const json& root, const DialoguePageProperties& default_properties
	);
};

struct DialogueDesc {
	Origin origin{ Origin::Center };

	json data;

	/// @brief Used when no background texture is supplied, and as a fallback if texture size cannot
	/// be resolved.
	V2_float box_size;

	std::optional<std::string> background_texture;
	Color background_color{ color::Black.WithAlpha(180) };

	bool ui_layer{ true };
};

class DialogueBox : public Entity {
public:
	DialogueBox() = default;
	explicit DialogueBox(Entity entity);

	[[nodiscard]] DialogueData& Data();
	[[nodiscard]] const DialogueData& Data() const;

	[[nodiscard]] Key GetContinueKey() const;
	DialogueBox& SetContinueKey(Key continue_key);

	[[nodiscard]] bool IsOpen() const;

	DialogueBox& Open(std::string_view dialogue_name = {});
	DialogueBox& Close();

	DialogueBox& NextPage();
	DialogueBox& CompletePage();

	DialogueBox& SetDialogue(std::string_view name = {});
	DialogueBox& SetNextDialogue();

	[[nodiscard]] DialogueEntry* GetCurrentDialogue();
	[[nodiscard]] DialogueLine* GetCurrentDialogueLine();
	[[nodiscard]] DialoguePage* GetCurrentDialoguePage();

	[[nodiscard]] Text TextPart();
	[[nodiscard]] std::optional<Text> TryTextPart() const;

	[[nodiscard]] Tween TweenPart();
	[[nodiscard]] std::optional<Tween> TryTweenPart() const;

	[[nodiscard]] std::optional<Sprite> TryBackground() const;
	[[nodiscard]] std::optional<Entity> TryBackgroundEntity() const;

private:
	friend struct impl::DialogueWaitScript;
	friend struct impl::DialogueScrollScript;

	[[nodiscard]] std::optional<Entity> TryPart(DialoguePartRole role) const;
	[[nodiscard]] Entity Part(DialoguePartRole role);

	void ApplyCurrentPage();
	void StartCurrentPageScroll();
	void PositionTextForPage(const DialoguePageProperties& properties);
};

DialogueBox CreateDialogueBox(Scene& scene, Transform transform, const DialogueDesc& desc);

void to_json(json& j, const DialoguePageProperties& properties);
void from_json(const json& j, DialoguePageProperties& properties);

void to_json(json& j, const DialoguePage& page);
void from_json(const json& j, DialoguePage& page);

void to_json(json& j, const DialogueLine& line);
void from_json(const json& j, DialogueLine& line);

void to_json(json& j, const DialogueEntry& dialogue);
void from_json(const json& j, DialogueEntry& dialogue);

void to_json(json& j, const DialogueData& data);
void from_json(const json& j, DialogueData& data);

} // namespace ptgn