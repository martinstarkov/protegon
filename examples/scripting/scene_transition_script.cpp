#include "runtime/ui/button.h"

#include <string>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

namespace {

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

ScriptSequence MakeSceneChangeSequence(
	std::string name,
	std::string target_scene_type,
	SceneTransitionStyle transition,
	V2_float direction = {
		1.0f,
		0.0f
	}
) {
	SceneChangeScript change;

	change.action =
		SceneChangeAction::Switch;

	// Reuse the current scene tag so the active scene is replaced.
	change.scene_tag.clear();

	change.scene_type =
		std::move(
			target_scene_type
		);

	change.scene_parameters =
		json::object();

	change.transition =
		transition;

	change.duration_ms =
		700.0f;

	change.delay_ms =
		0.0f;

	change.ease =
		Ease::InOutQuad;

	change.direction =
		direction;

	ScriptSequence sequence{
		std::move(name)
	};

	sequence
		.StartOn<
			event::ButtonPress
		>()
		.Then(
			std::move(change)
		);

	return sequence;
}

} // namespace

class BlueTransitionScene;

class RedTransitionScene :
	public Scene {
public:
	void OnNew() override {
		SetBackgroundColor(
			Color{
				105,
				25,
				35,
				255
			}
		);

		Button button{
			CreateTextButton(
				*this,
				{
					-240.0f,
					100.0f
				},
				"Fade To Blue Scene"
			)
		};

		AttachSequence(
			button,
			MakeSceneChangeSequence(
				"Fade To Blue",
				"BlueTransitionScene",
				SceneTransitionStyle::Fade
			)
		);
	}
};

class BlueTransitionScene :
	public Scene {
public:
	void OnNew() override {
		SetBackgroundColor(
			Color{
				20,
				55,
				120,
				255
			}
		);

		Button button{
			CreateTextButton(
				*this,
				{
					240.0f,
					-100.0f
				},
				"Slide Back To Red Scene"
			)
		};

		AttachSequence(
			button,
			MakeSceneChangeSequence(
				"Slide To Red",
				"RedTransitionScene",
				SceneTransitionStyle::Slide,
				{
					1.0f,
					0.0f
				}
			)
		);
	}
};

PTGN_REGISTER_SCENE(
	RedTransitionScene,
	"Red Transition Scene"
);

PTGN_REGISTER_SCENE(
	BlueTransitionScene,
	"Blue Transition Scene"
);

int main(int, char**) {
	Application app{
		"Scene Transition Built-in Script Sequence Test"
	};

	PTGN_WITH_EDITOR(
		app,
		true
	);

	app.StartProject<
		RedTransitionScene
	>(
		"SceneTransitionScriptProject/"
		"SceneTransitionScript.ptgnproj"
	);
}