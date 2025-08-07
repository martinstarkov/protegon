
#include "components/draw.h"
#include "components/movement.h"
#include "core/entity.h"
#include "core/game.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/geometry/rect.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

struct PhysicsBoundaryScene : public Scene {
	Entity player;
	V2_float player_size{ 20, 20 };

	BoundaryBehavior behavior{ BoundaryBehavior::ReflectVelocity };

	std::size_t entity_count{ 1000 };

	RNG<float> rngx{ 0.0f, (float)window_size.x };
	RNG<float> rngy{ 0.0f, (float)window_size.y };
	RNG<float> rngsize{ 5.0f, 10.0f };

	Entity AddEntity(
		const V2_float& center, const V2_float& size, const Color& color,
		bool set_random_velocity = true
	) {
		Entity entity		  = CreateRect(*this, center, size, color);
		const auto random_vel = []() {
			V2_float dir{ V2_float::Random(-0.5f, 0.5f) };
			float speed = 60.0f;

			if (dir.x != 0 || dir.y != 0) {
				return dir.Normalized() * speed;
			} else {
				return V2_float{ speed, 0.0f };
			}
		};
		auto& rb{ entity.Add<RigidBody>() };
		if (set_random_velocity) {
			rb.velocity = random_vel();
		}
		return entity;
	}

	void Enter() override {
		physics.SetBounds({}, window_size, behavior);
		player = AddEntity(window_size * 0.5f, player_size, color::Purple, false);
		SetDepth(player, 1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity({ rngx(), rngy() }, { rngsize(), rngsize() }, Color::RandomTransparent());
		}
	}

	void Update() override {
		MoveWASD(GetPosition(player), V2_float{ 100.0f } * game.dt(), false);

		if (game.input.KeyDown(Key::Q)) {
			behavior = BoundaryBehavior::StopAtBounds;
			ReEnter();
		} else if (game.input.KeyDown(Key::E)) {
			behavior = BoundaryBehavior::ReflectVelocity;
			ReEnter();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("PhysicsBoundaryScene: Q/E to switch boundary behavior", window_size);
	game.scene.Enter<PhysicsBoundaryScene>("");
	return 0;
}