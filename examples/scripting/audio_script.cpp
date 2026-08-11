#include "runtime/ui/button.h"

#include <string_view>
#include <utility>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

namespace {

void LoadAssets(
	Scene& scene
) {
	scene.ctx().asset.Load(
		{
			{
				"press",
				"assets/press.ogg"
			}
		}
	);
}

Button CreateTextButton(
	Scene& scene,
	V2_float position,
	std::string_view label
) {
	Transform transform;

	transform.position =
		position;

	Button button{
		CreateButton(
			scene,
			transform,
			V2_float{
				300.0f,
				80.0f
			},
			Origin::Center
		)
	};

	button.Background();

	button
		.Text()
		.Content(label);

	return button;
}

void AttachSequence(
	Entity owner,
	ScriptSequence sequence
) {
	auto& script{
		AddScript<Script>(owner)
	};

	script.sequence =
		std::move(sequence);
}

} // namespace

class PlayAudioScriptScene :
	public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);

		SetBackgroundColor(
			color::Transparent
		);

		Button button{
			CreateTextButton(
				*this,
				{ 0.0f, 0.0f },
				"Play press.ogg"
			)
		};

		ScriptSequence sequence{
			"Play Button Audio"
		};

		sequence
			.StartOn<
				event::ButtonPress
			>()
			.Then(
				PlaySoundScript{
					AudioKey{
						"press"
					},
					1.0f,
					0
				}
			);

		AttachSequence(
			button,
			std::move(sequence)
		);
	}

	void OnLoad() override {
		LoadAssets(*this);
	}
};

PTGN_REGISTER_SCENE(
	PlayAudioScriptScene,
	"Play Audio Script Scene"
);

int main(int, char**) {
	Application app{
		"Audio Built-in Script Sequence Test"
	};

	PTGN_WITH_EDITOR(
		app,
		true
	);

	app.StartProject<
		PlayAudioScriptScene
	>(
		"PlayAudioScriptProject/"
		"PlayAudioScript.ptgnproj"
	);
}