#include <string>
#include <string_view>
#include <utility>

#include "app/application.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/builtin_scripts.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"

using namespace ptgn;

namespace {

constexpr std::string_view kNextFrameSignal{ "animation.next_frame" };

constexpr std::string_view kPlaySignal{ "animation.play" };

constexpr std::string_view kStopSignal{ "animation.stop" };

constexpr std::string_view kPlayOnceSignal{ "animation.play_once" };

constexpr std::string_view kCompleteSignal{ "animation.complete" };

void LoadAssets(Scene& scene) {
	scene.ctx().asset.Load({ { "animation", "assets/animation.png" } });
}

Button CreateTextButton(Scene& scene, V2_float position, std::string_view label) {
	Transform transform;

	transform.position = position;

	Button button{ CreateButton(scene, transform, V2_float{ 260.0f, 72.0f }, Origin::Center) };

	button.Background();

	button.Text().Content(label);

	return button;
}

void AttachSequence(Entity owner, ScriptSequence sequence) {
	auto& script{ AddScript<Script>(owner) };

	script.sequence = std::move(sequence);
}

json MakeSignalCondition(std::string_view signal) {
	json value = json::object();

	value["signal"] = std::string{ signal };

	return value;
}

ScriptSequence MakeButtonSignalSequence(std::string name, std::string_view signal) {
	ScriptSequence sequence{ std::move(name) };

	sequence.StartOn<event::ButtonPress>().Then(
		EmitSignalScript{ SignalKey{ std::string{ signal } } }
	);

	return sequence;
}

} // namespace

class AnimationScriptScene : public Scene {
public:
	void OnNew() override {
		LoadAssets(*this);

		SetBackgroundColor(color::Transparent);

		Transform animation_transform;

		animation_transform.position = { 0.0f, -100.0f };

		Animation animation{ CreateAnimation(
			*this, animation_transform, "animation", { 4, 500ms, V2_int{ 16, 32 }, 1, { 0, 32 } },
			Origin::Center
		) };

		SetScale(animation, 4.0f);

		animation.Stop(true);

		// Signal -> advance exactly one frame.
		AnimationActionScript next_frame;

		next_frame.action = AnimationAction::NextFrame;

		ScriptSequence next_frame_sequence{ "Advance One Animation Frame" };

		next_frame_sequence.StartOn<Signal>(MakeSignalCondition(kNextFrameSignal))
			.Then(std::move(next_frame));

		AttachSequence(animation, std::move(next_frame_sequence));

		// Signal -> reset and play the full animation.
		AnimationActionScript reset;

		reset.action = AnimationAction::Reset;

		AnimationActionScript play;

		play.action = AnimationAction::Start;

		play.force = true;

		ScriptSequence play_sequence{ "Play Entire Animation" };

		play_sequence.StartOn<Signal>(MakeSignalCondition(kPlaySignal))
			.Then(std::move(reset))
			.Then(std::move(play));

		AttachSequence(animation, std::move(play_sequence));

		// Signal -> stop on the current frame.
		AnimationActionScript stop;

		stop.action = AnimationAction::Stop;

		stop.reset_on_stop = false;

		ScriptSequence stop_sequence{ "Stop Animation" };

		stop_sequence.StartOn<Signal>(MakeSignalCondition(kStopSignal)).Then(std::move(stop));

		AttachSequence(animation, std::move(stop_sequence));

		// Signal -> reset and play exactly once.
		//
		// The animation's configured play_count is 1, so it automatically
		// stops after completing one full sequence.
		AnimationActionScript play_once_reset;

		play_once_reset.action = AnimationAction::Reset;

		AnimationActionScript play_once;

		play_once.action = AnimationAction::Start;

		play_once.force = true;

		ScriptSequence play_once_sequence{ "Play Animation Once" };

		play_once_sequence.StartOn<Signal>(MakeSignalCondition(kPlayOnceSignal))
			.Then(std::move(play_once_reset))
			.Then(std::move(play_once));

		AttachSequence(animation, std::move(play_once_sequence));

		// The real animation completion event emits a global signal.
		ScriptSequence complete_sequence{ "Send Animation Complete Signal" };

		complete_sequence.StartOn<event::AnimationComplete>().Then(
			EmitSignalScript{ SignalKey{ std::string{ kCompleteSignal } } }
		);

		AttachSequence(animation, std::move(complete_sequence));

		Entity completion_indicator{ CreateCircle(*this, { 0.0f, 80.0f }, 34.0f, color::Green) };

		Hide(completion_indicator);

		ScriptSequence show_indicator_sequence{ "Show Completion Indicator" };

		show_indicator_sequence.StartOn<Signal>(MakeSignalCondition(kCompleteSignal))
			.Then(SetVisibleScript{ true });

		AttachSequence(completion_indicator, std::move(show_indicator_sequence));

		// Top-left: advance one frame.
		Button next_frame_button{ CreateTextButton(*this, { -160.0f, 190.0f }, "Next Frame") };

		AttachSequence(
			next_frame_button, MakeButtonSignalSequence("Request Next Frame", kNextFrameSignal)
		);

		// Top-right: restart and play the full animation.
		Button play_button{ CreateTextButton(*this, { 160.0f, 190.0f }, "Play Full Animation") };

		AttachSequence(
			play_button, MakeButtonSignalSequence("Request Full Animation", kPlaySignal)
		);

		// Bottom-left: stop without resetting.
		Button stop_button{ CreateTextButton(*this, { -160.0f, 280.0f }, "Stop Animation") };

		AttachSequence(
			stop_button, MakeButtonSignalSequence("Request Animation Stop", kStopSignal)
		);

		// Bottom-right: reset and play one configured loop.
		Button play_once_button{ CreateTextButton(*this, { 160.0f, 280.0f }, "Play Once") };

		AttachSequence(
			play_once_button, MakeButtonSignalSequence("Request Animation Once", kPlayOnceSignal)
		);
	}

	void OnLoad() override {
		LoadAssets(*this);

		// TODO: Remove these:

		ctx().asset.Load("whirlpool", "assets/shader.glsl");
		ctx().asset.Load("noise", "assets/noise.png");

		CreateCustomShader(*this, {}, "whirlpool", "noise", V2_float{ 150 }, {}, Origin::Center).
				SetMaterialUniform("u_Scale", 0.5f).SetMaterialUniform("u_Opacity", 0.5f).SetMaterialUpdate([](auto entity) mutable {
					float timescale{ 1.0f };
					float time{
						static_cast<float>(entity.GetScene().ctx().TimeSinceStart().count())
					};

					entity.SetMaterialUniform(
						"u_Time", time / 1000.0f * timescale
					);
				});
	}
};

PTGN_REGISTER_SCENE(AnimationScriptScene, "Animation Script Scene");

int main(int, char**) {
	Application app{ "Animation Built-in Script Sequence Test" };

	PTGN_WITH_EDITOR(app, true);

	app.StartProject<AnimationScriptScene>("AnimationScriptProject/"
										   "AnimationScript.ptgnproj");
}