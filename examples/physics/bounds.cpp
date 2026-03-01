
#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int game_size{ 800, 800 };

struct PhysicsBoundaryScene : public Scene {
	Entity player;
	V2_float player_size{ 20, 20 };

	BoundaryBehavior behavior{ BoundaryBehavior::ReflectVelocity };

	std::size_t entity_count{ 1000 };

	RNG<float> rngx{ -(float)game_size.x * 0.5f, (float)game_size.x * 0.5f };
	RNG<float> rngy{ -(float)game_size.y * 0.5f, (float)game_size.y * 0.5f };
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

	void OnEnter() override {
		physics.SetBounds(-game_size * 0.5f, game_size, behavior);
		player = AddEntity({}, player_size, color::Purple, false);
		SetDepth(player, 1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity({ rngx(), rngy() }, { rngsize(), rngsize() }, Color::RandomTransparent());
		}
	}

	void OnUpdate() override {
		V2_float pos{ GetPosition(player) };
		MoveWASD(pos, V2_float{ 100.0f } * app().DeltaTime(), false);
		SetPosition(player, pos);

		if (input.KeyPressed(Key::Q)) {
			behavior = BoundaryBehavior::StopVelocity;
			ReEnter();
		} else if (input.KeyPressed(Key::E)) {
			behavior = BoundaryBehavior::ReflectVelocity;
			ReEnter();
		}
	}
};

int main(int, char**) {
	Application app{ "PhysicsBoundaryScene: Q/E to switch boundary behavior", game_size };
	app.StartWith<PhysicsBoundaryScene>();
}