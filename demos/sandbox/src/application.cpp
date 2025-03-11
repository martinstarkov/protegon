
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/collision/collider.h"
#include "physics/movement.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "serialization/file_stream_reader.h"
#include "serialization/file_stream_writer.h"
#include "serialization/stream_reader.h"
#include "serialization/stream_writer.h"
#include "utility/file.h"
#include "utility/log.h"

using namespace ptgn;

class Sandbox : public Scene {
public:
	struct OnCollision {
		virtual ~OnCollision() = default;

		virtual void Start(Collision c) {}

		virtual void Continue(Collision c) {}

		virtual void Stop(Collision c) {}
	};

	struct CollisionCallback {
		template <typename T>
		CollisionCallback(T&& t) : ptr{ std::make_unique<T>(t) } {}

		std::unique_ptr<OnCollision> ptr;
	};

	struct OnPlayerCollision : public OnCollision {
		OnPlayerCollision(ecs::Entity player) : player{ player } {}

		ecs::Entity player;

		void Start(Collision c) override {
			PTGN_LOG("Start | Player id: ", player.GetId());
		}

		void Continue(Collision c) override {
			PTGN_LOG("Continue | Player id: ", player.GetId());
		}

		void Stop(Collision c) override {
			PTGN_LOG("Stop | Player id: ", player.GetId());
		}

		static void Serialize(StreamWriter* w, const OnPlayerCollision& p) {
			w->Write(p.player.GetId());
		}

		static void Deserialize(StreamReader* r, OnPlayerCollision& p) {
			ecs::Index player_index;
			r->Read(player_index);
			PTGN_LOG("Deserialized player index: ", player_index);
		}
	};

	struct TargetPosition {
		V2_float start;
		V2_float stop;
	};

	ecs::Entity CreateSheep(ecs::Manager& m, const V2_float& position) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<TextureKey>("sheep");
		e.Add<TargetPosition>();
		V2_float ws{ game.window.GetSize() };
		std::array<V2_float, 4> coordinates{
			V2_float{},
			V2_float{ ws.x, 0.0f },
			ws,
			V2_float{ 0.0f, ws.y },
		};
		e.Add<Tween>()
			.During(milliseconds{ 4000 })
			.Repeat(-1)
			.OnRepeat([e, coordinates]() {
				auto index = e.Get<Tween>().GetRepeats() % coordinates.size();
				PTGN_ASSERT(index < coordinates.size());
				e.Get<TargetPosition>().start = e.Get<Transform>().position;
				e.Get<TargetPosition>().stop  = coordinates[index];
			})
			.OnUpdate([e](float f) {
				e.Get<Transform>().position =
					Lerp(e.Get<TargetPosition>().start, e.Get<TargetPosition>().stop, f);
			})
			.Start();
		return e;
	}

	ecs::Entity sheep;

	void Exit() override {
		FileStreamWriter w{ "resources/sheep.bin" };
		w.WriteEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
	}

	void Enter() override {
		camera.primary.SetPosition(game.window.GetCenter());

		game.texture.Load("sheep", "resources/test.png");

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		if (FileExists("resources/sheep.bin")) {
			FileStreamReader r{ "resources/sheep.bin" };
			r.ReadEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Sandbox", { 1280, 720 }, color::Transparent);
	game.scene.Enter<Sandbox>("sandbox");
	return 0;
}