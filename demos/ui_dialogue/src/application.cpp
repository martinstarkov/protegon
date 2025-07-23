
#include "core/entity.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "math/vector2.h"
#include "rendering/resources/text.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

namespace ptgn {

namespace impl {

struct DialogueLine {};

struct Dialogue {
	std::unordered_map<std::string_view, DialogueLine> lines;
};

struct DialogueScrollScript : public ptgn::Script<DialogueScrollScript> {
	DialogueScrollScript() {}

	void OnRepeatUpdate(int repeat);
};

} // namespace impl

class DialogueComponent {
public:
	DialogueComponent(Entity parent, const json& j) {
		text_ = CreateText(parent.GetScene(), "Hello!", color::Pink);
		text_.SetParent(parent);
		PTGN_LOG(j);
	}

	void Open() {
		// TODO: Pull from JSON and possibly use default, which can also be from JSON.
		milliseconds scroll_duration{ 1000 };
		int characters{ 1000 };
		milliseconds execution_delay{ scroll_duration / characters };
		text_.AddRepeatScript<impl::DialogueScrollScript>(execution_delay, characters, false);
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
};

namespace impl {

void DialogueScrollScript::OnRepeatUpdate(int repeat) {
	auto dialogue_entity = entity.GetParent();

	PTGN_ASSERT(dialogue_entity);

	PTGN_ASSERT(dialogue_entity.Has<DialogueComponent>());

	auto& dialogue{ dialogue_entity.Get<DialogueComponent>() };

	const auto& repeat_info{ entity.GetRepeatScriptInfo<DialogueScrollScript>() };

	PTGN_LOG("Scroll repeat: ", repeat);
}

} // namespace impl

} // namespace ptgn

struct DialogueScene : public Scene {
	Entity npc;

	void Enter() override {
		npc = CreateEntity();
		npc.SetPosition(window_size / 2);
		json j		   = LoadJson("resources/dialogue.json");
		auto& dialogue = npc.Add<DialogueComponent>(npc, j);
		dialogue.Open();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("DialogueScene", window_size);
	game.scene.Enter<DialogueScene>("");
	return 0;
}