#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/entity.h"
#include "core/time.h"
#include "input/key.h"
#include "math/vector2.h"
#include "renderer/font.h"
#include "renderer/text.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class DialogueComponent;

struct DialoguePageProperties {
	DialoguePageProperties() = default;

	friend bool operator==(const DialoguePageProperties& a, const DialoguePageProperties& b);
	friend bool operator!=(const DialoguePageProperties& a, const DialoguePageProperties& b);

	[[nodiscard]] DialoguePageProperties InheritProperties(const json& j) const;

	void SetPadding(int padding);
	void SetPadding(const V2_int& padding);
	void SetPadding(int top, int right, int bottom, int left);

	Color color{ color::White };
	ResourceHandle font_key;
	FontSize font_size;
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

struct Dialogue {
	std::size_t index{ 0 };
	bool repeatable{ true };
	DialogueBehavior behavior{ DialogueBehavior::Sequential };
	bool scroll{ true };
	std::string next_dialogue;

	[[nodiscard]] std::size_t PickRandomIndex();
	[[nodiscard]] const DialogueLine* GetCurrentDialogueLine() const;
	[[nodiscard]] int GetNewDialogueLine();

	std::vector<DialogueLine> lines;
	std::vector<std::size_t> used_line_indices;
};

namespace impl {

struct DialogueWaitScript : public ptgn::Script<DialogueWaitScript> {
	DialogueWaitScript() {}

	[[nodiscard]] DialogueComponent& GetDialogueComponent();
	void OnUpdate(float dt) final;
};

struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript> {
	DialogueScrollScript() {}

	[[nodiscard]] DialogueComponent& GetDialogueComponent();
	void UpdateText(float elapsed_fraction);
	bool OnTimerStop() final;
	void OnTimerUpdate(float elapsed_fraction) final;
};

} // namespace impl

class DialogueComponent {
public:
	DialogueComponent();
	DialogueComponent(DialogueComponent&&) noexcept;
	DialogueComponent& operator=(DialogueComponent&&) noexcept;
	DialogueComponent(const DialogueComponent&)			   = delete;
	DialogueComponent& operator=(const DialogueComponent&) = delete;

	DialogueComponent(Entity parent, const path& json_path, Entity&& background = {});
	~DialogueComponent();

	[[nodiscard]] Key GetContinueKey() const;
	void SetContinueKey(Key continue_key);

	[[nodiscard]] const Text& GetText() const;
	[[nodiscard]] Text& GetText();
	[[nodiscard]] bool IsOpen() const;

	void Open(const std::string& dialogue_name = "");
	void Close();
	void NextPage();
	void SetNextDialogue();
	void SetDialogue(const std::string& name = "");

	[[nodiscard]] const std::unordered_map<std::string, Dialogue>& GetDialogues() const;
	[[nodiscard]] Dialogue* GetCurrentDialogue();
	[[nodiscard]] DialogueLine* GetCurrentDialogueLine();
	[[nodiscard]] DialoguePage* GetCurrentDialoguePage();
	void IncrementPage();
	void DrawInfo();

private:
	void AlignToTopLeft(const DialoguePageProperties& default_properties);
	void StartDialogueLine(int dialogue_line_index);
	void LoadFromJson(const json& root, const DialoguePageProperties& default_properties);

	[[nodiscard]] std::vector<DialoguePage> SplitTextWithDuration(
		const std::string& full_text, const DialoguePageProperties& properties,
		const std::string& split_end, const std::string& split_begin
	);

	[[nodiscard]] static std::string JoinLines(const std::vector<std::string>& lines);

	Text text_;
	Entity background_;
	Key continue_key_{ Key::Enter };
	int current_line_{ 0 };
	int current_page_{ 0 };
	std::string current_dialogue_;
	std::unordered_map<std::string, Dialogue> dialogues_;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	DialogueBehavior, {
						  { DialogueBehavior::Sequential, "sequential" },
						  { DialogueBehavior::Random, "random" },
					  }
)

} // namespace ptgn