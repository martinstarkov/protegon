#include <algorithm>
#include <iostream>
#include <string_view>

#include "app/application.h"
#include "core/assert.h"
#include "core/editor.h"
#include "core/graphics/color.h"
#include "core/math/easing.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/graphics.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scripting/script_sequence.h"
#include "serialization/serialize.h"

using namespace ptgn;

namespace {

constexpr std::string_view kMoverTag{ "Script Demo - Mover" };
constexpr std::string_view kSpinnerTag{ "Script Demo - Spinner" };
constexpr std::string_view kPulseTag{ "Script Demo - Pulse" };
constexpr std::string_view kBlinkTag{ "Script Demo - Visibility" };
constexpr std::string_view kSignalResponderTag{ "Script Demo - Signal Responder" };
constexpr std::string_view kFollowTargetTag{ "Script Demo - Follow Target" };
constexpr std::string_view kFollowerTag{ "Script Demo - Follower" };
constexpr std::string_view kControllerTag{ "Script Demo - Controller" };

constexpr std::string_view kMoverCompleteSignal{ "script_demo.mover_complete" };
constexpr std::string_view kResponderCompleteSignal{ "script_demo.responder_complete" };
constexpr std::string_view kStopPulseSignal{ "script_demo.stop_pulse" };

void SetEntityTag(Entity entity, std::string_view tag) {
	PTGN_ASSERT(entity, "Cannot tag an invalid script-demo entity");
	entity.Get<Tag>() = Tag{ tag };
}

[[nodiscard]] NativeScript LogStep(std::string_view message) {
	return NativeScript{ NativeScriptCallbacks{
		.on_start = [text = std::string{ message }](Script&) {
			std::cout << "[ScriptSequenceSceneDemo] " << text << '\n';
		},
	} };
}

} // namespace

/// @brief Scene-backed demonstration of the registry-driven script sequence system.
///
/// The scene creates ordinary engine entities in OnNew and attaches runtime scripts in OnEnter.
/// Select an entity while the scene is playing to inspect its live Scripts component.
class ScriptSequenceSceneDemo final : public Scene {
public:
	ScriptSequenceSceneDemo() = default;

	void OnNew() override {
		SetBackgroundColor(Color{ 18, 20, 27, 255 });

		Entity mover{ CreateRect(
			*this,
			V2_float{ -390.0f, 185.0f },
			V2_float{ 72.0f, 72.0f },
			Color{ 67, 145, 255, 255 }
		) };
		SetEntityTag(mover, kMoverTag);

		Entity spinner{ CreateRect(
			*this,
			V2_float{ -105.0f, 185.0f },
			V2_float{ 82.0f, 82.0f },
			Color{ 250, 171, 64, 255 }
		) };
		SetEntityTag(spinner, kSpinnerTag);

		Entity pulse{ CreateCircle(
			*this,
			V2_float{ 165.0f, 185.0f },
			42.0f,
			Color{ 99, 214, 143, 255 }
		) };
		SetEntityTag(pulse, kPulseTag);

		Entity visibility{ CreateRect(
			*this,
			V2_float{ 390.0f, 185.0f },
			V2_float{ 78.0f, 78.0f },
			Color{ 245, 94, 128, 255 }
		) };
		SetEntityTag(visibility, kBlinkTag);

		Entity signal_responder{ CreateRect(
			*this,
			V2_float{ -285.0f, -135.0f },
			V2_float{ 92.0f, 92.0f },
			Color{ 173, 112, 255, 255 }
		) };
		SetEntityTag(signal_responder, kSignalResponderTag);

		Entity follow_target{ CreateCircle(
			*this,
			V2_float{ 195.0f, -135.0f },
			48.0f,
			Color{ 79, 210, 218, 255 }
		) };
		SetEntityTag(follow_target, kFollowTargetTag);

		Entity follower{ CreateCircle(
			*this,
			V2_float{ -40.0f, -135.0f },
			29.0f,
			Color{ 255, 224, 102, 255 }
		) };
		SetEntityTag(follower, kFollowerTag);

		Entity controller{ CreateEntity("Script Demo Controller") };
		SetEntityTag(controller, kControllerTag);

		std::cout << "[ScriptSequenceSceneDemo] OnNew created the authored demo entities.\n";
	}

	void OnLoad() override {
		AssertDemoEntity(kMoverTag);
		AssertDemoEntity(kSpinnerTag);
		AssertDemoEntity(kPulseTag);
		AssertDemoEntity(kBlinkTag);
		AssertDemoEntity(kSignalResponderTag);
		AssertDemoEntity(kFollowTargetTag);
		AssertDemoEntity(kFollowerTag);
		AssertDemoEntity(kControllerTag);

		std::cout
			<< "[ScriptSequenceSceneDemo] OnLoad\n"
			<< "  mode: " << (IsRuntime() ? "runtime" : "editor") << '\n'
			<< "  move_duration_ms: " << move_duration_ms_ << '\n'
			<< "  follower_speed: " << follower_speed_ << '\n';
	}

	void OnEnter() override {
		const Entity mover{ RequireEntity(kMoverTag) };
		const Entity spinner{ RequireEntity(kSpinnerTag) };
		const Entity pulse{ RequireEntity(kPulseTag) };
		const Entity visibility{ RequireEntity(kBlinkTag) };
		const Entity signal_responder{ RequireEntity(kSignalResponderTag) };
		const Entity follow_target{ RequireEntity(kFollowTargetTag) };
		const Entity follower{ RequireEntity(kFollowerTag) };
		const Entity controller{ RequireEntity(kControllerTag) };

		AddMoverSequence(mover);
		AddSpinnerSequence(spinner);
		AddPulseSequence(pulse);
		AddVisibilitySequence(visibility);
		AddSignalResponderSequence(signal_responder);
		AddFollowDemo(follow_target, follower);
		AddControllerTimeline(controller);

		Refresh();

		std::cout
			<< "[ScriptSequenceSceneDemo] OnEnter attached live engine scripts.\n"
			<< "  Blue: MoveTo + easing + yoyo + repeat\n"
			<< "  Orange: RotateTo + repeat\n"
			<< "  Green: ScaleTo + infinite yoyo, stopped by signal\n"
			<< "  Pink: Wait + SetVisible\n"
			<< "  Purple: starts when the blue sequence emits a Signal\n"
			<< "  Yellow: root FollowTargetScript following the cyan target\n";
	}

	void OnExit() override {
		std::cout << "[ScriptSequenceSceneDemo] OnExit discarded the runtime scripts.\n";
	}

private:
	void AddMoverSequence(Entity mover) {
		ScriptSequence sequence{ "Mover: Move, Yoyo, Wait, Signal" };
		sequence
			.Channel(SequenceChannelKey{ "transform" })
			.Reentry(ReentryMode::Restart)
			.During(
				move_duration_ms_,
				MoveToScript{ V2_float{ 345.0f, 0.0f }, true }
			)
			.Ease(Ease::InOutQuad)
			.Yoyo()
			.Repeat(1)
			.Wait(250.0f)
			.EmitSignal(SignalKey{ kMoverCompleteSignal })
			.Then(LogStep("Mover completed and emitted script_demo.mover_complete."));

		mover_sequence_ = sequence.Start(mover);
	}

	void AddSpinnerSequence(Entity spinner) {
		ScriptSequence sequence{ "Spinner: Delayed Repeated Rotation" };
		sequence
			.Wait(300.0f)
			.During(
				spin_duration_ms_,
				RotateToScript{ 360.0f, false, true }
			)
			.Ease(Ease::InOutQuad)
			.Repeat(2)
			.Then(LogStep("Spinner completed three rotations."));

		spinner_sequence_ = sequence.Start(spinner);
	}

	void AddPulseSequence(Entity pulse) {
		// This is a resident Script rather than a one-off ScriptSequence handle. Its sequence starts
		// automatically because it has steps and no start trigger. A global signal later stops it.
		auto& script{ AddScript<Script>(pulse) };
		script.sequence = ScriptSequence{ "Pulse: Infinite Scale Until Signal" };
		script.sequence
			.StopOn<Signal>(json{ { "signal", std::string{ kStopPulseSignal } } })
			.During(
				pulse_duration_ms_,
				ScaleToScript{ V2_float{ 1.65f, 1.65f }, false }
			)
			.Ease(Ease::InOutQuad)
			.Yoyo()
			.Infinite();
	}

	void AddVisibilitySequence(Entity visibility) {
		ScriptSequence sequence{ "Visibility: Blink Three Times" };
		for (int i{ 0 }; i < 3; ++i) {
			sequence
				.Wait(500.0f)
				.Then(SetVisibleScript{ false })
				.Wait(260.0f)
				.Then(SetVisibleScript{ true });
		}
		sequence.Then(LogStep("Visibility sequence completed three blink cycles."));

		visibility_sequence_ = sequence.Start(visibility);
	}

	void AddSignalResponderSequence(Entity signal_responder) {
		// This resident sequence stays idle until the mover emits the matching global Signal.
		auto& script{ AddScript<Script>(signal_responder) };
		script.sequence = ScriptSequence{ "Signal Responder: Start On Mover Complete" };
		script.sequence
			.Reentry(ReentryMode::Restart)
			.StartOn<Signal>(
				json{ { "signal", std::string{ kMoverCompleteSignal } } }
			)
			.During(
				700.0f,
				MoveToScript{ V2_float{ 0.0f, 145.0f }, true }
			)
			.Ease(Ease::OutBack)
			.During(
				650.0f,
				RotateToScript{ 180.0f, true, true }
			)
			.Ease(Ease::InOutQuad)
			.During(
				700.0f,
				MoveToScript{ V2_float{ 0.0f, -145.0f }, true }
			)
			.Ease(Ease::InOutQuad)
			.EmitSignal(SignalKey{ kResponderCompleteSignal })
			.Then(LogStep("Signal responder completed its event-triggered sequence."));
	}

	void AddFollowDemo(Entity follow_target, Entity follower) {
		ScriptSequence target_sequence{ "Follow Target: Move Away And Return" };
		target_sequence
			.Wait(200.0f)
			.During(
				follow_target_duration_ms_,
				MoveToScript{ V2_float{ 300.0f, 0.0f }, true }
			)
			.Ease(Ease::InOutQuad)
			.Yoyo()
			.Repeat(1)
			.Then(LogStep("Follow target returned to its authored position."));

		follow_target_sequence_ = target_sequence.Start(follow_target);

		// FollowTargetScript is attached as a root script. It disables itself after reaching the
		// target within the requested stopping distance.
		(void)AddScript<FollowTargetScript>(
			follower,
			follow_target,
			std::max(0.0f, follower_speed_),
			6.0f
		);
	}

	void AddControllerTimeline(Entity controller) {
		ScriptSequence sequence{ "Controller: Stop Pulse After Delay" };
		sequence
			.Wait(stop_pulse_after_ms_)
			.EmitSignal(SignalKey{ kStopPulseSignal })
			.Then(LogStep("Controller emitted script_demo.stop_pulse."))
			.Wait(500.0f)
			.Then(LogStep("Demo timeline completed. Stop Play to restore authored transforms."));

		controller_sequence_ = sequence.Start(controller);
	}

	[[nodiscard]] Entity RequireEntity(std::string_view tag) {
		Entity entity{ GetEntity(Tag{ tag }) };
		PTGN_ASSERT(entity, "Required script-demo entity is missing");
		return entity;
	}

	void AssertDemoEntity(std::string_view tag) {
		(void)RequireEntity(tag);
	}

	// Scene-file parameters. These can be edited in the scene's parameters object.
	float move_duration_ms_{ 1250.0f };
	float spin_duration_ms_{ 850.0f };
	float pulse_duration_ms_{ 450.0f };
	float follow_target_duration_ms_{ 1600.0f };
	float follower_speed_{ 185.0f };
	float stop_pulse_after_ms_{ 5200.0f };

	// Runtime-only handles are deliberately omitted from reflection.
	SequenceHandle mover_sequence_;
	SequenceHandle spinner_sequence_;
	SequenceHandle visibility_sequence_;
	SequenceHandle follow_target_sequence_;
	SequenceHandle controller_sequence_;

	PTGN_REFLECT(
		ScriptSequenceSceneDemo,
		move_duration_ms_,
		spin_duration_ms_,
		pulse_duration_ms_,
		follow_target_duration_ms_,
		follower_speed_,
		stop_pulse_after_ms_
	)
};

PTGN_REGISTER_SCENE(
	ScriptSequenceSceneDemo,
	"Script Sequence Scene Demo"
);

int main() {
	Application app{ "Protegon Script Sequence Scene Demo" };
	PTGN_WITH_EDITOR(app, true);
	app.StartProject<ScriptSequenceSceneDemo>(
		"ScriptSequenceSceneDemoProject/ScriptSequenceSceneDemo.ptgnproj"
	);
}
