#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "core/util/time.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/sprite.h"
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
PTGN_REFLECT_ENUM(DialogueBehavior);

enum class DialoguePartRole : std::uint8_t {
	Background,
	Text
};
PTGN_REFLECT_ENUM(DialoguePartRole);

namespace impl {

struct DialoguePart {
	DialoguePartRole role{ DialoguePartRole::Text };

	PTGN_REFLECT(DialoguePart, role)
};

struct DialogueWaitScript final : Script {
	void OnEvent(Event event) override;

	PTGN_REFLECT_EMPTY(DialogueWaitScript)

private:
	void OnKeyPressed(Key key) const;
};

struct DialogueScrollScript final : Script {
	[[nodiscard]] ScriptStatus OnUpdate() override;
	void OnComplete() override;

	PTGN_REFLECT_EMPTY(DialogueScrollScript)
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
	FontKey font{ kDefaultFont };
	float font_size{ kDefaultFontSize };

	V2_float box_size;
	Rect padding;

	milliseconds scroll_duration{ 1000 };

	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::Word };
	OverflowMode overflow_mode{ OverflowMode::Clip };
};

struct DialoguePage {
	StyledText styled_text;
	DialoguePageProperties properties;

	DialoguePage() = default;

	DialoguePage(StyledText styled_text, const DialoguePageProperties& properties);
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
	const DialogueLine* GetCurrentDialogueLine() const;

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

	std::optional<TextureKey> background_texture;
	Color background_color{ color::Black.WithAlpha(180) };

	bool ui_layer{ true };
};

class DialogueBox : public Entity {
public:
	DialogueBox() = default;
	explicit DialogueBox(Entity entity);

	[[nodiscard]] DialogueData& Data();
	[[nodiscard]] const DialogueData& Data() const;

	Key GetContinueKey() const;
	DialogueBox& SetContinueKey(Key continue_key);

	bool IsOpen() const;

	DialogueBox& Open(std::string_view dialogue_name = {});
	DialogueBox& Close();

	DialogueBox& NextPage();
	DialogueBox& CompletePage();

	DialogueBox& SetDialogue(std::string_view name = {});
	DialogueBox& SetNextDialogue();

	DialogueEntry* GetCurrentDialogue();
	DialogueLine* GetCurrentDialogueLine();
	DialoguePage* GetCurrentDialoguePage();

	Text TextPart();
	[[nodiscard]] std::optional<Text> TryTextPart() const;

	[[nodiscard]] std::optional<Sprite> TryBackground() const;
	[[nodiscard]] std::optional<Entity> TryBackgroundEntity() const;

private:
	friend struct impl::DialogueWaitScript;
	friend struct impl::DialogueScrollScript;

	[[nodiscard]] std::optional<Entity> TryPart(DialoguePartRole role) const;
	Entity Part(DialoguePartRole role);

	void ApplyCurrentPage();
	void StartCurrentPageScroll();
	void StopCurrentPageScroll();
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
