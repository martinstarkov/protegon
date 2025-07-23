
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/resources/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

namespace impl {

struct Dialogue {};

} // namespace impl

class DialogueComponent {
public:
	DialogueComponent(Entity parent, const json& j) {
		text_ = CreateText(parent.GetScene(), "Hello!", color::Pink);
		text_.SetParent(parent);
		PTGN_LOG(j);
	}

	void NextParagraph() {}

	void UpdateDialogue() {}

	void SetDialogue(std::string_view name) {}

	DialogueComponent(DialogueComponent&&) noexcept			   = default;
	DialogueComponent& operator=(DialogueComponent&&) noexcept = default;
	DialogueComponent(const DialogueComponent&)				   = delete;
	DialogueComponent& operator=(const DialogueComponent&)	   = delete;

	~DialogueComponent() {
		text_.Destroy();
	}

private:
	Text text_;
	std::unordered_map<std::string_view, impl::Dialogue> dialogues_;
};

class Dialogue : public Entity {
public:
	Dialogue(const Entity entity) : Entity{ entity } {}
};

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		npc = CreateEntity();
		npc.SetPosition(window_size / 2);
		json j = LoadJson("resources/dialogue.json");
		npc.Add<DialogueComponent>(npc, j);
	}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}