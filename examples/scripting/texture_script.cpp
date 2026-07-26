#include "runtime/ui/toggle_button.h"

#include <string>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"

using namespace ptgn;

namespace {

constexpr std::string_view kUseBoxSignal{
	"texture.use_box"
};

constexpr std::string_view kUseCircleSignal{
	"texture.use_circle"
};

void LoadAssets(
	Scene& scene
) {
	scene.ctx().asset.Load(
		{
			{
				"box",
				"assets/box.png"
			},
			{
				"circle",
				"assets/circle.png"
			}
		}
	);
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

json MakeSignalCondition(
	std::string_view signal
) {
	json value = json::object();

	value["signal"] =
		std::string{ signal };

	return value;
}

json MakeToggleCondition(
	bool toggled
) {
	json value = json::object();

	value["toggled"] =
		toggled;

	return value;
}

void RegisterToggleConditionMatcher() {
	SequenceEventRegistrationOptions<
		event::ToggleButtonToggle
	> options;

	options.default_value =
		MakeToggleCondition(false);

	options.matches = [](
		Entity,
		const json& value,
		const event::ToggleButtonToggle& event
	) {
		return value.value(
			"toggled",
			false
		) == event.toggled;
	};

	options.available = [](
		Entity owner
	) {
		return owner.Has<
			impl::ToggleButtonData
		>();
	};

	SequenceEventRegistry::Register<
		event::ToggleButtonToggle
	>(
		std::move(options)
	);
}

ScriptSequence MakeToggleSignalSequence(
	std::string name,
	bool toggled,
	std::string_view signal
) {
	ScriptSequence sequence{
		std::move(name)
	};

	sequence
		.StartOn<
			event::ToggleButtonToggle
		>(
			MakeToggleCondition(
				toggled
			)
		)
		.Then(
			EmitSignalScript{
				SignalKey{
					std::string{
						signal
					}
				}
			}
		);

	return sequence;
}

ScriptSequence MakeTextureSequence(
	std::string name,
	std::string_view signal,
	TextureKey texture
) {
	ScriptSequence sequence{
		std::move(name)
	};

	sequence
		.StartOn<Signal>(
			MakeSignalCondition(
				signal
			)
		)
		.Then(
			SetTextureScript{
				std::move(texture)
			}
		);

	return sequence;
}

} // namespace

class TextureSignalScriptScene :
	public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);

		SetBackgroundColor(
			color::Transparent
		);

		Entity target{
			CreateSprite(
				*this,
				{ 0.0f, -90.0f },
				"box"
			)
		};

		AttachSequence(
			target,
			MakeTextureSequence(
				"Use Box Texture",
				kUseBoxSignal,
				TextureKey{ "box" }
			)
		);

		AttachSequence(
			target,
			MakeTextureSequence(
				"Use Circle Texture",
				kUseCircleSignal,
				TextureKey{ "circle" }
			)
		);

		Transform button_transform;

		button_transform.position = {
			0.0f,
			180.0f
		};

		ToggleButton button{
			CreateToggleButton(
				*this,
				button_transform,
				V2_float{
					280.0f,
					72.0f
				},
				Origin::Center,
				false
			)
		};

		button.Background();

		button
			.Text()
			.Content(
				"Toggle Target Texture"
			);

		AttachSequence(
			button,
			MakeToggleSignalSequence(
				"Request Circle Texture",
				true,
				kUseCircleSignal
			)
		);

		AttachSequence(
			button,
			MakeToggleSignalSequence(
				"Request Box Texture",
				false,
				kUseBoxSignal
			)
		);
	}

	void OnLoad() override {
		LoadAssets(*this);
	}
};

PTGN_REGISTER_SCENE(
	TextureSignalScriptScene,
	"Texture Signal Script Scene"
);

int main(int, char**) {
	Application app{
		"Texture Built-in Script Sequence Test"
	};

	RegisterToggleConditionMatcher();

	PTGN_WITH_EDITOR(
		app,
		true
	);

	app.StartProject<
		TextureSignalScriptScene
	>(
		"TextureSignalScriptProject/"
		"TextureSignalScript.ptgnproj"
	);
}