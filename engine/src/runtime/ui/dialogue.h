#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/input/key.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scripting/script.h"
#include "serialization/json/fwd.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class DialogueComponent;

namespace impl {

struct DialogueWaitScript : public Script {
	DialogueComponent& GetDialogueComponent();

	void OnEvent(Event event) override;

	void OnKeyPressed(Key key);
};

struct DialogueScrollScript : public Script {
	DialogueComponent& GetDialogueComponent();

	static void UpdateText(Entity text_entity, float elapsed_fraction);

	void OnEvent(Event event) override;

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

	Color color{ color::White };
	std::string font_key;
	float font_size{ kDefaultFontSize };
	V2_float box_size;
	int padding_left{ 0 };
	int padding_right{ 0 };
	int padding_top{ 0 };
	int padding_bottom{ 0 };
	milliseconds scroll_duration{ 1000 };
};

struct DialoguePage {
	DialoguePage() = default;
	DialoguePage(const std::string& text_content, const DialoguePageProperties& properties);

	std::string content;
	DialoguePageProperties properties;
};

struct DialogueLine {
	std::vector<DialoguePage> pages;
};

enum class DialogueBehavior {
	Sequential,
	Random
};
PTGN_REFLECT_ENUM(DialogueBehavior);

struct Dialogue {
	std::size_t index{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next_dialogue;

	[[nodiscard]] std::size_t PickRandomIndex() const;
	const DialogueLine* GetCurrentDialogueLine() const;

	std::optional<int> GetNewDialogueLine();

	std::vector<DialogueLine> lines;
	std::vector<std::size_t> used_line_indices;
};

class DialogueComponent {
public:
	DialogueComponent() = default;
	/// @param background Either the sprite that is used as the background or the size of the
	/// background.
	DialogueComponent(
		Entity parent, const json& json, std::variant<GameObject<Sprite>, V2_float> background
	);

	Key GetContinueKey() const;
	void SetContinueKey(Key continue_key);

	[[nodiscard]] bool IsOpen() const;

	void Open(const std::string& dialogue_name = "");
	void Close();
	void NextPage();
	void SetNextDialogue();
	void SetDialogue(const std::string& name = "");

	const std::unordered_map<std::string, Dialogue>& GetDialogues() const;
	Dialogue* GetCurrentDialogue();
	DialogueLine* GetCurrentDialogueLine();
	DialoguePage* GetCurrentDialoguePage();
	void IncrementPage();
	void DrawInfo(Scene& scene, V2_float position);

private:
	friend struct impl::DialogueWaitScript;

	void AlignToTopLeft(const DialoguePageProperties& default_properties) const;
	void StartDialogueLine(int dialogue_line_index);
	void LoadFromJson(
		const Scene& scene, const json& root, const DialoguePageProperties& default_properties
	);

	[[nodiscard]] std::vector<DialoguePage> SplitTextWithDuration(
		const Scene& scene, const std::string& full_text, const DialoguePageProperties& properties,
		const std::string& split_end, const std::string& split_begin
	);

	[[nodiscard]] static std::string JoinLines(const std::vector<std::string>& lines);

	GameObject<Tween> tween_;
	GameObject<Text> text_;
	std::optional<GameObject<Sprite>> background_;

	Key continue_key_{ Key::Enter };

	int current_line_{ 0 };
	int current_page_{ 0 };
	std::string current_dialogue_;

	std::unordered_map<std::string, Dialogue> dialogues_;
};

} // namespace ptgn