
#include "core/ecs/components/draw.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/entity.h"
#include "core/app/application.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/geometry/rect.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 800, 800 };

struct PhysicsBoundaryScene : public Scene {
	Entity player;
	V2_float player_size{ 20, 20 };

	BoundaryBehavior behavior{ BoundaryBehavior::ReflectVelocity };

	std::size_t entity_count{ 1000 };

	RNG<float> rngx{ -(float)resolution.x * 0.5f, (float)resolution.x * 0.5f };
	RNG<float> rngy{ -(float)resolution.y * 0.5f, (float)resolution.y * 0.5f };
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
		physics.SetBounds(-resolution * 0.5f, resolution, behavior);
		player = AddEntity({}, player_size, color::Purple, false);
		SetDepth(player, 1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity({ rngx(), rngy() }, { rngsize(), rngsize() }, Color::RandomTransparent());
		}
	}

	void Update() override {
		V2_float pos{ GetPosition(player) };
		MoveWASD(pos, V2_float{ 100.0f } * Application::Get().dt(), false);
		SetPosition(player, pos);

		if (input.KeyDown(Key::Q)) {
			behavior = BoundaryBehavior::StopVelocity;
			ReEnter();
		} else if (input.KeyDown(Key::E)) {
			behavior = BoundaryBehavior::ReflectVelocity;
			ReEnter();
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("PhysicsBoundaryScene: Q/E to switch boundary behavior", resolution);
	Application::Get().scene_.Enter<PhysicsBoundaryScene>("");
	return 0;
}