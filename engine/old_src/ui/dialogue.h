// #pragma once
//
// #include <string>
// #include <unordered_map>
// #include <vector>
//
// #include "ecs/components/generic.h"
// #include "ecs/components/sprite.h"
// #include "ecs/entity.h"
// #include "ecs/game_object.h"
// #include "core/input/key.h"
// #include "core/scripting/script.h"
// #include "core/scripting/script_interfaces.h"
// #include "core/util/file.h"
// #include "core/util/time.h"
// #include "math/vector2.h"
// #include "renderer/api/color.h"
// #include "renderer/text/font.h"
// #include "renderer/text/text.h"
// #include "serialization/json/enum.h"
// #include "serialization/json/fwd.h"
// #include "serialization/json/serializable.h"
// #include "tween/tween.h"
//
// namespace ptgn {
//
// class DialogueComponent;
//
// namespace impl {
//
// struct DialogueWaitScript : public Script<DialogueWaitScript> {
//	DialogueWaitScript() {}
//
//	[[nodiscard]] DialogueComponent& GetDialogueComponent();
//	void OnUpdate() final;
// };
//
// struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript, TweenScript> {
//	DialogueScrollScript() {}
//
//	[[nodiscard]] DialogueComponent& GetDialogueComponent();
//	static void UpdateText(Entity& text_entity, float elapsed_fraction);
//	void OnPointComplete() final;
//	void OnProgress(float elapsed_fraction) final;
// };
//
// } // namespace impl
//
// struct DialoguePageProperties {
//	DialoguePageProperties() = default;
//
//	bool operator==(const DialoguePageProperties&) const = default;
//
//	[[nodiscard]] DialoguePageProperties InheritProperties(const json& j) const;
//
//	void SetPadding(int padding);
//	void SetPadding(const V2_int& padding);
//	void SetPadding(int top, int right, int bottom, int left);
//
//	Color color{ color::White };
//	ResourceHandle font_key;
//	FontSize font_size;
//	V2_float box_size;
//	int padding_left{ 0 };
//	int padding_right{ 0 };
//	int padding_top{ 0 };
//	int padding_bottom{ 0 };
//	milliseconds scroll_duration{ 1000 };
// };
//
// struct DialoguePage {
//	DialoguePage() = default;
//	DialoguePage(const std::string& text_content, const DialoguePageProperties& properties);
//
//	std::string content;
//	DialoguePageProperties properties;
// };
//
// struct DialogueLine {
//	std::vector<DialoguePage> pages;
// };
//
// enum class DialogueBehavior {
//	Sequential,
//	Random
// };
//
// struct Dialogue {
//	std::size_t index{ 0 };
//	bool repeatable{ true };
//	DialogueBehavior behavior{ DialogueBehavior::Sequential };
//	bool scroll{ true };
//	std::string next_dialogue;
//
//	[[nodiscard]] std::size_t PickRandomIndex() const;
//	[[nodiscard]] const DialogueLine* GetCurrentDialogueLine() const;
//	[[nodiscard]] int GetNewDialogueLine();
//
//	std::vector<DialogueLine> lines;
//	std::vector<std::size_t> used_line_indices;
// };
//
// class DialogueComponent {
// public:
//	DialogueComponent() = default;
//	DialogueComponent(Entity parent, const path& json_path, Entity&& background = {});
//
//	[[nodiscard]] Key GetContinueKey() const;
//	void SetContinueKey(Key continue_key);
//
//	[[nodiscard]] bool IsOpen() const;
//
//	void Open(const std::string& dialogue_name = "");
//	void Close();
//	void NextPage();
//	void SetNextDialogue();
//	void SetDialogue(const std::string& name = "");
//
//	[[nodiscard]] const std::unordered_map<std::string, Dialogue>& GetDialogues() const;
//	[[nodiscard]] Dialogue* GetCurrentDialogue();
//	[[nodiscard]] DialogueLine* GetCurrentDialogueLine();
//	[[nodiscard]] DialoguePage* GetCurrentDialoguePage();
//	void IncrementPage();
//	void DrawInfo(const V2_float& position);
//
// private:
//	friend struct impl::DialogueWaitScript;
//
//	void AlignToTopLeft(const DialoguePageProperties& default_properties);
//	void StartDialogueLine(int dialogue_line_index);
//	void LoadFromJson(const json& root, const DialoguePageProperties& default_properties);
//
//	[[nodiscard]] std::vector<DialoguePage> SplitTextWithDuration(
//		const std::string& full_text, const DialoguePageProperties& properties,
//		const std::string& split_end, const std::string& split_begin
//	);
//
//	[[nodiscard]] static std::string JoinLines(const std::vector<std::string>& lines);
//
//	GameObject<Tween> tween_;
//	GameObject<Text> text_;
//	GameObject<Sprite> background_;
//
//	Key continue_key_{ Key::Enter };
//
//	int current_line_{ 0 };
//	int current_page_{ 0 };
//	std::string current_dialogue_;
//
//	std::unordered_map<std::string, Dialogue> dialogues_;
// };
//
// PTGN_SERIALIZER_REGISTER_ENUM(
//	DialogueBehavior,
//	{
//		{ DialogueBehavior::Sequential, "sequential" },
//		{ DialogueBehavior::Random, "random" },
//	}
//)
//
// } // namespace ptgn