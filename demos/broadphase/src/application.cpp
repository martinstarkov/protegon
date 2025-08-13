#include <vector>

#include "components/draw.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "debug/profiling.h"
#include "input/input_handler.h"
#include "math/geometry/rect.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "physics/collision/bounding_aabb.h"
#include "physics/collision/broadphase.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

// TODO: Move all of this into the collision system.

[[nodiscard]] BoundingAABB GetBoundingAABB(const Entity& entity) {
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

	RNG<float> rngx{ 0.0f, (float)window_size.x };
	RNG<float> rngy{ 0.0f, (float)window_size.y };
	RNG<float> rngsize{ 5.0f, 30.0f };

	void Enter() override {
		physics.SetBounds({}, window_size, BoundaryBehavior::ReflectVelocity);

		player = AddEntity(*this, window_size * 0.5f, player_size, color::Purple, false);
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

	void Update() override {
		MoveWASD(GetPosition(player), V2_float{ 100.0f } * game.dt(), false);

		for (auto [e, tint] : EntitiesWith<Tint>()) {
			tint = color::Green;
		}

		SetTint(player, color::Purple);

		auto player_volume{ GetBoundingAABB(player) };

#ifdef KDTREE

		if (KDTREE) {
			PTGN_PROFILE_FUNCTION();
			// Check only collisions with relevant k-d tree nodes.

			// TODO: Only update if player moved.
			tree.UpdateBoundingAABB(player, GetBoundingAABB(player));

			// for (auto [e, rect] : EntitiesWith<Rect>()) {
			//	// TODO: Only update if entity moved.
			//	tree.UpdateBoundingAABB(e, GetBoundingAABB(e));
			// }
			tree.EndFrameUpdate();
		} else {
			PTGN_PROFILE_FUNCTION();
			std::vector<impl::KDObject> objects;
			objects.reserve(Size());
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
		auto mouse_pos{ game.input.GetMousePosition() };
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

		DrawDebugLine(player_pos, mouse_pos, color::Gold, 2.0f);

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

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BroadphaseScene", window_size);
	game.scene.Enter<BroadphaseScene>("");
	return 0;
}