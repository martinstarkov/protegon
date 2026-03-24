#include "runtime/physics/broadphase.h"

#include <vector>

#include "app/application.h"
#include "core/math/geometry/rect.h"
#include "core/math/rng.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_manager.h"
#include "tools/debug/profiling.h"

using namespace ptgn;

constexpr V2_float game_size{ 800, 600 };

// TODO: Move all of this into the collision system.

BoundingAABB GetBoundingAABB(const Entity& entity) {
	return GetBoundingAABB(entity.Get<Rect>(), GetTransform(entity));
}

Entity AddEntity(
	Scene& scene, const V2_float& center, const V2_float& size, const Color& color,
	bool induce_random_velocity = true
) {
	Entity entity = CreateRect(scene, center, size, color);
	if (induce_random_velocity) {
		auto& rb{ entity.Add<RigidBody>() };
		V2_float dir{ V2_float::RandomNormalized(-0.5f, 0.5f) };
		float speed = 60.0f;
		rb.velocity = dir * speed;
	}
	return entity;
}

#define KDTREE 0

struct BroadphaseScene : public Scene {
	impl::KDTree tree{ 64 };

	std::size_t entity_count{ 10000 };

	Entity player;
	V2_float player_size{ 20, 20 };

	RNG<float> rngx{ -(float)game_size.x * 0.5f, (float)game_size.x * 0.5f };
	RNG<float> rngy{ -(float)game_size.y * 0.5f, (float)game_size.y * 0.5f };
	RNG<float> rngsize{ 5.0f, 30.0f };

	void OnEnter() override {
		ctx().physics.SetBounds(Bounds{ V2_float{}, game_size, BoundaryBehavior::ReflectVelocity });

		player = AddEntity(*this, {}, player_size, color::Purple, false);
		SetDepth(player, 1);

		for (std::size_t i{ 0 }; i < entity_count; ++i) {
			AddEntity(
				*this, { rngx(), rngy() }, { rngsize(), rngsize() }, color::Green,
				FlipCoin() // false
			);
		}
		Refresh();
		for (auto [e, rect] : EntitiesWith<Rect>()) {
			// TODO: Only update if entity moved.
			tree.UpdateBoundingAABB(e, GetBoundingAABB(e));
		}
		tree.EndFrameUpdate();
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 200.0f };
		float dt{ ctx().dt().count() };
		V2_float pos{ GetPosition(player) };
		MoveWASD(*this, pos, speed * dt, false);
		SetPosition(player, pos);

		for (auto [e, tint] : EntitiesWith<impl::Tint>()) {
			SetTint(e, color::Green);
		}

		SetTint(player, color::Purple);

		auto player_volume{ GetBoundingAABB(player) };

#ifdef KDTREE

		if (KDTREE) {
			// TOOD: Fix.
			// PTGN_PROFILE_FUNCTION();
			// Check only collisions with relevant k-d tree nodes.

			// TODO: Only update if player moved.
			tree.UpdateBoundingAABB(player, GetBoundingAABB(player));

			// for (auto [e, rect] : EntitiesWith<Rect>()) {
			//	// TODO: Only update if entity moved.
			//	tree.UpdateBoundingAABB(e, GetBoundingAABB(e));
			// }
			tree.EndFrameUpdate();
		} else {
			// TOOD: Fix.
			// PTGN_PROFILE_FUNCTION();
			std::vector<impl::KDObject> objects;
			objects.reserve(GetEntityCount());
			for (auto [e, rect] : EntitiesWith<Rect>()) {
				objects.emplace_back(e, GetBoundingAABB(e));
			}

			tree.Build(objects);
		}

		// For overlap / trigger tests:

		// PTGN_LOG("---------------------");
		// for (auto [e1, rect1] : EntitiesWith<Rect>()) {
		//	auto b1{ GetBoundingAABB(e1) };
		//	Rect rectb1{ b1.min, b1.max };
		//	auto candidates = tree.Query(b1);
		//	// PTGN_LOG(candidates.size());
		//	for (auto& e2 : candidates) {
		//		if (e1 == e2) {
		//			continue;
		//		}
		//		auto bounding{ GetBoundingAABB(e2) };
		//		Rect rectb2{ bounding.min, bounding.max };
		//		if (Overlap(Transform{}, rectb1, Transform{}, rectb2)) {
		//			SetTint(e1, color::Red);
		//			SetTint(e2, color::Red);
		//		}
		//	}
		//}

		// For full raycasts:

		auto player_pos{ GetPosition(player) };
		auto mouse_pos{ ctx().input.GetMousePosition() };
		auto dir{ mouse_pos - player_pos };

		auto player_rect{ GetBoundingAABB(player) };

		auto candidates = tree.Raycast(player, dir, player_rect);
		for (auto& candidate : candidates) {
			if (candidate && candidate != player) {
				SetTint(candidate, color::Orange);
			}
		}

		// For first only raycasts:

		auto candidate = tree.RaycastFirst(player, dir, player_rect);
		if (candidate && candidate != player) {
			SetTint(candidate, color::Red);
		}

		ctx().renderer.DrawShape(
			Line{ player_pos, mouse_pos }, Transform{}, color::Gold, FillStyle::Hollow(2.0f),
			Origin::Center, Depth{}, BlendMode::Blend
		);

#else
		PTGN_PROFILE_FUNCTION();
		for (auto [e1, rect1] : EntitiesWith<Rect>()) {
			auto b1{ GetBoundingAABB(e1) };
			for (auto [e2, rect2] : EntitiesWith<Rect>()) {
				if (e1 == e2) {
					continue;
				}
				if (Overlap(Transform{}, b1, Transform{}, GetBoundingAABB(e2)) {
					SetTint(e1, color::Red);
					SetTint(e2, color::Red);
				}
			}
		}
#endif
	}
};

int main(int, char**) {
	Application app{ "BroadphaseScene", game_size };
	app.StartWith<BroadphaseScene>();
}