#include <variant>
#include <vector>

#include "components/draw.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "input/mouse.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_input.h"
#include "scene/scene_manager.h"
#include "tweens/follow_config.h"
#include "tweens/tween_effects.h"

using namespace ptgn;

struct FollowEffectScene : public Scene {
	Sprite mouse;

	Sprite entity1;
	Sprite entity2;
	Sprite entity3;
	Sprite entity4;
	Sprite entity5;

	TargetFollowConfig config1;
	TargetFollowConfig config2;
	TargetFollowConfig config3;
	PathFollowConfig config4;
	PathFollowConfig config5;

	std::vector<V2_float> waypoints;

	Entity CreateFollower(const Color& color, const V2_float& start_position) {
		auto follower{ CreateSprite(*this, "smile", start_position) };
		SetTint(follower, color);
		return follower;
	}

	void Enter() override {
		game.window.SetResizable();
		SetBackgroundColor(color::DarkGray);

		LoadResource("smile", "resources/smile.png");

		mouse = CreateEntity();
		SetPosition(mouse, {});

		entity1 = CreateFollower(color::Red, { -300, -300 });
		entity2 = CreateFollower(color::Green, { -200, -200 });
		entity3 = CreateFollower(color::Blue, { -100, -100 });
		entity4 = CreateFollower(color::Yellow, { 0, 0 });
		entity5 = CreateFollower(color::Magenta, { 100, 100 });

		// Target follow (snap).
		config1.lerp = { 1.0f, 1.0f };

		// Target follow (lerp).
		config2.lerp = { 0.6f, 0.6f };

		// Target follow (velocity).
		config3.move_mode = MoveMode::Velocity;

		// Path follow (lerp).
		config4.move_mode	  = MoveMode::Lerp;
		config4.lerp		  = { 0.6f, 0.6f };
		config4.loop_path	  = true;
		config4.stop_distance = 40.0f;

		// Path follow (velocity).
		config5.loop_path	  = true;
		config5.stop_distance = 40.0f;
		config5.move_mode	  = MoveMode::Velocity;

		V2_float game_size{ game.renderer.GetGameSize() };
		V2_float half{ game_size * 0.5f };
		V2_float center{ 0, 0 };

		waypoints = { -half, center, V2_float{ half.x, -half.y }, center,
					  half,	 center, V2_float{ -half.x, half.y }, center };

		Start();
	}

	void Start() {
		// config1.teleport_on_start = true;
		// config2.teleport_on_start = true;
		// config3.teleport_on_start = true;
		// config4.teleport_on_start = true;
		// config5.teleport_on_start = true;

		StartFollow(entity1, mouse, config1);
		StartFollow(entity2, mouse, config2);
		StartFollow(entity3, mouse, config3);
		StartFollow(entity4, waypoints, config4);
		StartFollow(entity5, waypoints, config5);
	}

	void Stop() {
		StopFollow(entity1);
		StopFollow(entity2);
		StopFollow(entity3);
		StopFollow(entity4);
		StopFollow(entity5);
	}

	void Update() override {
		SetPosition(mouse, input.GetMousePosition());
		if (input.MouseDown(Mouse::Left)) {
			Stop();
		} else if (input.MouseDown(Mouse::Right)) {
			Start();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("FollowEffectScene: Left/Right: Stop/Start Follow");
	game.scene.Enter<FollowEffectScene>("");
	return 0;
}