#include <memory>
#include <new>
#include <string>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };

V2_float ws;

struct CollisionTest {
	virtual ~CollisionTest() = default;

	Manager* manager;

	CollisionTest() {
		manager = &game.scene.Get("");
	}

	virtual void Enter() {}

	virtual void Exit() {}

	virtual void Update() {}

	virtual void Draw() {}
};

struct TestOverlapScript : public Script<TestOverlapScript, OverlapScript> {
	TestOverlapScript() = default;

	explicit TestOverlapScript(const std::string& name) : name{ name } {}

	void OnOverlapStart(Entity other) override {
		PTGN_LOG(name, " started overlap with ", other.GetId());
	}

	void OnOverlap(Entity other) override {
		PTGN_LOG(name, " continued overlap with ", other.GetId());
	}

	void OnOverlapStop(Entity other) override {
		PTGN_LOG(name, " stopped overlap with ", other.GetId());
	}

	std::string name;
};

struct TestIntersectScript : public Script<TestIntersectScript, CollisionScript> {
	TestIntersectScript() = default;

	explicit TestIntersectScript(const std::string& name) : name{ name } {}

	void OnCollision(Collision c) override {
		PTGN_LOG(name, " intersected with ", c.entity.GetId(), ", normal: ", c.normal);
	}

	std::string name;
};

struct TestRaycastScript : public Script<TestRaycastScript, CollisionScript> {
	TestRaycastScript() = default;

	explicit TestRaycastScript(const std::string& name) : name{ name } {}

	void OnCollision(Collision c) override {
		PTGN_LOG(name, " ray collided with ", c.entity.GetId(), ", normal: ", c.normal);
	}

	std::string name;
};

class CollisionCallbackTest : public CollisionTest {
public:
	Entity intersect;
	Entity overlap;
	Entity sweep;
	Entity intersect_circle;
	Entity overlap_circle;
	Entity sweep_circle;

	// Total number.
	const int move_entities{ 6 };
	// Current move entity.
	int move_entity{ 5 };
	V2_float speed{ 300.0f };

	void Enter() override {
		PTGN_ASSERT(manager != nullptr);
		intersect		 = manager->CreateEntity();
		sweep			 = manager->CreateEntity();
		overlap			 = manager->CreateEntity();
		intersect_circle = manager->CreateEntity();
		sweep_circle	 = manager->CreateEntity();
		overlap_circle	 = manager->CreateEntity();

		Show(intersect);
		Show(sweep);
		Show(overlap);
		Show(intersect_circle);
		Show(sweep_circle);
		Show(overlap_circle);

		SetTint(intersect, color::Purple);
		SetTint(intersect_circle, color::Purple);
		SetTint(sweep, color::Cyan);
		SetTint(sweep_circle, color::Cyan);
		SetTint(overlap, color::Orange);
		SetTint(overlap_circle, color::Orange);

		SetPosition(intersect, V2_float{ 100, 100 });
		SetPosition(overlap, V2_float{ 200, 200 });
		SetPosition(sweep, V2_float{ 300, 300 });
		SetPosition(intersect_circle, V2_float{ 400, 400 });
		SetPosition(overlap_circle, V2_float{ 500, 500 });
		SetPosition(sweep_circle, V2_float{ 300, 600 });

		intersect.Add<RigidBody>();
		overlap.Add<RigidBody>();
		sweep.Add<RigidBody>();
		intersect_circle.Add<RigidBody>();
		overlap_circle.Add<RigidBody>();
		sweep_circle.Add<RigidBody>();

		V2_float rect_size{ 30, 30 };
		float circle_radius{ 30.0f };

		intersect.Add<Collider>(Rect{ rect_size });
		overlap.Add<Collider>(Rect{ rect_size });
		sweep.Add<Collider>(Rect{ rect_size });
		intersect.Add<Rect>(rect_size);
		overlap.Add<Rect>(rect_size);
		sweep.Add<Rect>(rect_size);
		SetDraw<Rect>(intersect);
		SetDraw<Rect>(overlap);
		SetDraw<Rect>(sweep);
		intersect_circle.Add<Collider>(Circle{ circle_radius });
		overlap_circle.Add<Collider>(Circle{ circle_radius });
		sweep_circle.Add<Collider>(Circle{ circle_radius });
		intersect_circle.Add<Circle>(circle_radius);
		overlap_circle.Add<Circle>(circle_radius);
		sweep_circle.Add<Circle>(circle_radius);
		SetDraw<Circle>(intersect_circle);
		SetDraw<Circle>(overlap_circle);
		SetDraw<Circle>(sweep_circle);

		auto& b1{ intersect.Get<Collider>() };
		auto& b2{ overlap.Get<Collider>() };
		auto& b3{ sweep.Get<Collider>() };
		auto& c1{ intersect_circle.Get<Collider>() };
		auto& c2{ overlap_circle.Get<Collider>() };
		auto& c3{ sweep_circle.Get<Collider>() };

		b1.SetCollisionMode(CollisionMode::Discrete);
		c1.SetCollisionMode(CollisionMode::Discrete);
		b2.SetCollisionMode(CollisionMode::Overlap);
		c2.SetCollisionMode(CollisionMode::Overlap);
		b3.SetCollisionMode(CollisionMode::Continuous);
		c3.SetCollisionMode(CollisionMode::Continuous);

		AddScript<TestIntersectScript>(intersect, "Intersection Rectangle");
		AddScript<TestIntersectScript>(intersect_circle, "Intersection Circle");
		AddScript<TestOverlapScript>(overlap, "Overlap Rectangle");
		AddScript<TestOverlapScript>(overlap_circle, "Overlap Circle");
		AddScript<TestRaycastScript>(sweep, "Raycast Rectangle");
		AddScript<TestRaycastScript>(sweep_circle, "Raycast Circle");

		/*CreateObstacle(V2_float{ 50, 50 }, V2_float{ 10, 500 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 600, 200 }, V2_float{ 10, 500 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 50, 650 }, V2_float{ 500, 10 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 100, 70 }, V2_float{ 500, 10 }, Origin::TopLeft);*/
	}

	void CreateObstacle(const V2_float& pos, const V2_float& size, Origin origin) {
		PTGN_ASSERT(manager != nullptr);
		auto obstacle = manager->CreateEntity();
		SetPosition(obstacle, pos);
		obstacle.Add<Collider>(Rect{ size });
		SetDrawOrigin(obstacle, origin);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			move_entity++;
		}
		if (game.input.KeyDown(Key::Q)) {
			move_entity--;
		}
		move_entity = Mod(move_entity, move_entities);

		V2_float* vel{ nullptr };

		if (move_entity == 0) {
			vel = &intersect.Get<RigidBody>().velocity;
		} else if (move_entity == 1) {
			vel = &overlap.Get<RigidBody>().velocity;
		} else if (move_entity == 2) {
			vel = &sweep.Get<RigidBody>().velocity;
		} else if (move_entity == 3) {
			vel = &intersect_circle.Get<RigidBody>().velocity;
		} else if (move_entity == 4) {
			vel = &overlap_circle.Get<RigidBody>().velocity;
		} else if (move_entity == 5) {
			vel = &sweep_circle.Get<RigidBody>().velocity;
		}

		PTGN_ASSERT(vel != nullptr);

		MoveWASD(*vel, speed * game.scene.Get("").physics.dt());
	}

	void Draw() override {
		constexpr Color text_color{ color::Blue };
		for (auto [e, collider] : game.scene.Get("").EntitiesWith<Collider>()) {
			auto transform{ GetAbsoluteTransform(e) };
			if (collider.mode == CollisionMode::Discrete) {
				DrawDebugText("Intersect", transform.GetPosition(), text_color);
			} else if (collider.mode == CollisionMode::Overlap) {
				DrawDebugText("Overlap", transform.GetPosition(), text_color);
			} else if (collider.mode == CollisionMode::Continuous) {
				DrawDebugText("Sweep", transform.GetPosition(), text_color);
			} else if (collider.mode == CollisionMode::None) {
				DrawDebugText("None", transform.GetPosition(), text_color);
			}
		}
	}
};

/*
class EntityCollisionTest : public CollisionTest {
public:
	ecs::Entity entity;

	V2_float speed{ 300.0f };

	void Enter() override {
		entity = manager.CreateEntity();
		entity.SetPosition(V2_float{ 400, 100 });
		entity.Add<RigidBody>();
		entity.Add<BoxCollider>(V2_float{ 30, 30 });

		CreateObstacle(V2_float{ 400, 400 }, V2_float{ 50, 50 }, Origin::Center);
	}

	void CreateObstacle(const V2_float& pos, const V2_float& size, Origin origin) {
		auto obstacle = manager.CreateEntity();
		obstacle.SetPosition(pos);
		obstacle.Add<BoxCollider>(size, origin);
	}

	void Update() override {
		MoveWASD(
			entity.Get<RigidBody>().velocity,
			speed * game.scene.Get("").physics.dt()
		);

		if (game.input.KeyDown(Key::R)) {
			Enter();
		}
	}

	void Draw() override {
		// TODO: Fix debug drawing.
		/*
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			Rect r{ b.GetAbsoluteRect() };
			DrawRect(e, r);
			if (e == entity) {
				Text{ "Entity", color::Black }.Draw(Rect{ r.Center() });
			}
		}
	}
};

class SweepEntityCollisionTest : public EntityCollisionTest {
public:
	void Enter() override {
		EntityCollisionTest::Enter();
		entity.Get<BoxCollider>().continuous = true;
	}
};
*/

/*
class ShapeCollisionTest : public CollisionTest {
public:
	V2_float p0{ 11, 16 };
	V2_float p1{ 14, 13 };

	// Horizontal lines
	Line l1{ { 3, 1 }, { 27, 1 } };
	Line l2{ { 3, 29 }, { 27, 29 } };
	// Vertical lines
	Line l3{ { 1, 3 }, { 1, 27 } };
	Line l4{ { 29, 3 }, { 29, 27 } };
	// Diagonal lines
	Line l5{ { 3, 7 }, { 7, 3 } };
	Line l6{ { 23, 3 }, { 27, 7 } };
	Line l7{ { 27, 23 }, { 23, 27 } };
	Line l8{ { 7, 27 }, { 3, 23 } };

	Circle c1{ { 15, 7 }, 4 };
	Rect r1{ { 4, 11 }, { 6, 10 }, Origin::TopLeft };

	Capsule ca1{ { 15, 23 }, { 23, 15 }, 4 };

	V2_float rect_size{ 4.0f, 4.0f };
	float circle_radius{ 4.0f };
	float capsule_radius{ 2.0f };

	V2_int size{ 31, 31 };

	void Enter() override {
		camera.GetPrimary().CenterOnArea(size);
	}

	void Update() override {
		if (game.input.MousePressed(Mouse::Left)) {
			p1 = V2_int{ input.GetMousePosition() };
		}
		if (game.input.MousePressed(Mouse::Right)) {
			p0 = V2_int{ input.GetMousePosition() };
		}
	}

	void DrawGrid() const {
		V2_float tile_size{ 1, 1 };
		for (int i = 0; i < size.x; i++) {
			for (int j = 0; j < size.y; j++) {
				Rect r{ V2_int{ i, j } * tile_size, tile_size, Origin::TopLeft };
				r.Draw(color::Black, 1.0f);
			}
		}
	}
};

class PointOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		p1 = V2_int{ input.GetMousePosition() };

		V2_float c0{ p1 };
		c0.Draw(color::Green, 1.0f);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red, 1.0f);
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Red, 1.0f);
				} else {
					s2.Draw(color::Red, -1.0f);
				}
			} else {
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Green, 1.0f);
				} else {
					s2.Draw(color::Green, -1.0f);
				}
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, l4);
		overlap(c0, l5);
		overlap(c0, l6);
		overlap(c0, l7);
		overlap(c0, l8);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
	}
};

class LineOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		ShapeCollisionTest::Update();

		Line c0{ p0, p1 };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red);
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Red, 1.0f);
				} else {
					s2.Draw(color::Red, -1.0f);
				}
			} else {
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Green, 1.0f);
				} else {
					s2.Draw(color::Green, -1.0f);
				}
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, l4);
		overlap(c0, l5);
		overlap(c0, l6);
		overlap(c0, l7);
		overlap(c0, l8);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
	}
};

class CircleOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		p1 = V2_int{ game.input.GetMousePosition() };

		Circle c0{ p1, circle_radius };
		c0.Draw(color::Green, -1.0f);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red, -1.0f);
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Red, 1.0f);
				} else {
					s2.Draw(color::Red, -1.0f);
				}
			} else {
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Green, 1.0f);
				} else {
					s2.Draw(color::Green, -1.0f);
				}
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, l4);
		overlap(c0, l5);
		overlap(c0, l6);
		overlap(c0, l7);
		overlap(c0, l8);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
	}
};

class RectOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		p1 = V2_int{ game.input.GetMousePosition() };

		Rect c0{ p1, rect_size, Origin::Center, 0.0f };
		c0.Draw(color::Green, -1.0f);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red, -1.0f);
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Red, 1.0f);
				} else {
					s2.Draw(color::Red, -1.0f);
				}
			} else {
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Green, 1.0f);
				} else {
					s2.Draw(color::Green, -1.0f);
				}
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, l4);
		overlap(c0, l5);
		overlap(c0, l6);
		overlap(c0, l7);
		overlap(c0, l8);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
	}
};

class CapsuleOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		ShapeCollisionTest::Update();

		Capsule c0{ p0, p1, capsule_radius };
		c0.Draw(color::Green, -1.0f);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red, -1.0f);
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Red, 1.0f);
				} else {
					s2.Draw(color::Red, -1.0f);
				}
			} else {
				if constexpr (std::is_same_v<decltype(s2), Line>) {
					s2.Draw(color::Green, 1.0f);
				} else {
					s2.Draw(color::Green, -1.0f);
				}
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, l4);
		overlap(c0, l5);
		overlap(c0, l6);
		overlap(c0, l7);
		overlap(c0, l8);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
	}
};

class RectangleSweepTest : public ShapeCollisionTest {
public:
	void Update() override {
		DrawGrid();
		ShapeCollisionTest::Update();

		V2_float vel{ p1 - p0 };

		Rect c0{ p0, rect_size };
		c0.Draw(color::Green, -1.0f);

		const auto sweep = [&](auto s1, auto s2) {
			Raycast raycast{ s1.Raycast(vel, s2) };
			if (raycast.Occurred()) {
				Rect c1{ p0 + vel * raycast.t, rect_size };
				s1.Draw(color::Red, -1.0f);
				s2.Draw(color::Red, -1.0f);
				c1.Draw(color::Purple, -1.0f);
			} else {
				s1.Draw(color::Green, -1.0f);
				s2.Draw(color::Green, -1.0f);
			}
		};

		sweep(c0, c1);
		sweep(c0, r1);
		Line{ p0, p1 }.Draw(color::Black);
		// TODO: Add
		// sweep(c0, ca1);
	}
};

class GeneralCollisionTest : public CollisionTest {
public:
	V2_float position1{ 200, 200 };
	V2_float position3{ 300, 300 };
	V2_float position4{ 200, 300 };

	V2_float size1{ 130, 130 };
	V2_float size2{ 30, 30 };

	float radius1{ 30 };
	float radius2{ 20 };

	Color color1{ color::Green };
	Color color2{ color::Blue };

	int options{ 9 };
	int types{ 3 };

	int option{ 4 };
	int type{ 2 };

	const float line_thickness{ 3.0f };

	// rotation of rectangles in radians.
	float rot_1{ DegToRad(45.0f) };
	float rot_2{ DegToRad(0.0f) };

	float rot_speed{ 1.0f };

	void Enter() override {}

	void Update() override {
		auto mouse = game.input.GetMousePosition();

		if (game.input.KeyDown(Key::T)) {
			option++;
			option = option++ % options;
		}

		if (game.input.KeyDown(Key::G)) {
			type++;
			type = type++ % types;
		}

		if (game.input.KeyDown(Key::R)) {
			position4 = mouse;
		}

		if (game.input.KeyPressed(Key::Q)) {
			rot_1 -= rot_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::E)) {
			rot_1 += rot_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::Z)) {
			rot_2 -= rot_speed * game.dt();
		}
		if (game.input.KeyPressed(Key::C)) {
			rot_2 += rot_speed * game.dt();
		}

		V2_float position2 = mouse;

		Color acolor1 = color1;
		Color acolor2 = color2;

		Rect aabb1{ position1, size1, Origin::Center, rot_1 };
		Rect aabb2{ position2, size2, Origin::Center, rot_2 };

		Circle circle1{ position1, radius1 };
		Circle circle2{ position2, radius2 };

		Line line1{ position1, position3 };
		Line line2{ position2, position4 };

		if (type == 0) { // overlap
			options = 10;
			if (option == 0) {
			} else if (option == 1) {
				if (circle1.Overlaps(position2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle1.Draw(acolor1, line_thickness);
				position2.Draw(acolor2);
			} else if (option == 2) {
				if (aabb1.Overlaps(position2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				position2.Draw(acolor2);
			} else if (option == 3) {
				if (line2.Overlaps(line1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				line1.Draw(acolor1);
				line2.Draw(acolor2);
			} else if (option == 4) {
				if (line2.Overlaps(circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				line2.Draw(acolor2);
				circle1.Draw(acolor1, line_thickness);
			} else if (option == 5) {
				if (line2.Overlaps(aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				line2.Draw(acolor2);
				aabb1.Draw(acolor1, line_thickness);
			} else if (option == 6) {
				if (circle2.Overlaps(circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				circle1.Draw(acolor1, line_thickness);
			} else if (option == 7) {
				if (circle2.Overlaps(aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				circle2.Draw(acolor2, line_thickness);
			} else if (option == 8) {
				aabb2.position = mouse;
				// no rotation case uses different overlap code.
				aabb1.rotation = 0.0f;
				aabb2.rotation = 0.0f;
				if (aabb1.Overlaps(aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb2.Draw(acolor2, line_thickness);
				aabb1.Draw(acolor1, line_thickness);
			} else if (option == 9) {
				aabb2.position = mouse;
				if (aabb1.Overlaps(aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb2.Draw(acolor2, line_thickness);
				aabb1.Draw(acolor1, line_thickness);
			}
		} else if (type == 1) { // intersect
			options = 4;
			const float slop{ 0.005f };
			Intersection c;
			if (option == 0) {
				// circle2.center = circle1.center;
				c = circle2.Intersects(circle1);
				if (c.Occurred()) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				circle1.Draw(acolor1, line_thickness);
				if (c.Occurred()) {
					Circle new_circle{ circle2.center + c.normal * (c.depth + slop),
									   circle2.radius };
					new_circle.Draw(color2, line_thickness);
					Line l{ circle2.center, new_circle.center };
					l.Draw(color::Gold);
					if (new_circle.Overlaps(circle1)) {
						c = new_circle.Intersects(circle1);
						PrintLine("Slop insufficient, overlap reoccurs");
						if (c.Occurred()) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			} else if (option == 1) {
				// circle2.center = aabb1.position;
				// circle2.center = aabb1.Center();
				c = circle2.Intersects(aabb1);
				if (c.Occurred()) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				circle2.Draw(acolor2, line_thickness);
				if (c.Occurred()) {
					Circle new_circle{ circle2.center + c.normal * (c.depth + slop),
									   circle2.radius };
					new_circle.Draw(color2, line_thickness);
					Line l{ circle2.center, new_circle.center };
					l.Draw(color::Gold);
					if (new_circle.Overlaps(aabb1)) {
						c = new_circle.Intersects(aabb1);
						PrintLine("Slop insufficient, overlap reoccurs");
						if (c.Occurred()) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			} else if (option == 2) {
				aabb2.position = mouse;
				// no rotation case uses different intersection code.
				aabb1.rotation = 0.0f;
				aabb2.rotation = 0.0f;
				// aabb2.position = aabb1.Center() - aabb2.Half();
				c = aabb2.Intersects(aabb1);
				if (c.Occurred()) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				aabb2.Draw(acolor2, line_thickness);
				if (c.Occurred()) {
					Rect new_aabb{ aabb2.position + c.normal * (c.depth + slop), aabb2.size,
								   aabb2.origin };
					new_aabb.Draw(color2, line_thickness);
					Line l{ aabb2.Center(), new_aabb.Center() };
					l.Draw(color::Gold);
					if (new_aabb.Overlaps(aabb1)) {
						c = new_aabb.Intersects(aabb1);
						PrintLine("Slop insufficient, overlap reoccurs");
						if (c.Occurred()) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			} else if (option == 3) {
				aabb2.position = mouse;
				// aabb2.position = aabb1.Center() - aabb2.Half();
				c = aabb2.Intersects(aabb1);
				if (c.Occurred()) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				aabb2.Draw(acolor2, line_thickness);
				if (c.Occurred()) {
					Rect new_aabb{ aabb2.position + c.normal * (c.depth + slop), aabb2.size,
								   aabb2.origin, rot_2 };
					new_aabb.Draw(color2, line_thickness);
					Line l{ aabb2.Center(), new_aabb.Center() };
					l.Draw(color::Gold);
				}
			}
		} else if (type == 2) { // dynamic
			aabb1.rotation = 0.0f;
			aabb2.rotation = 0.0f;
			options		   = 7;
			Raycast c;
			if (option == 0) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				potential.Draw(color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				l.Draw(color::Gray);
				c = circle2.Raycast(vel, aabb1);
				if (c.Occurred()) {
					Lerp(circle2.center, circle2.center + vel, c.t).Draw(color::Black, 3.0f);
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					normal.Draw(color::Orange);
					swept.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				aabb1.Draw(acolor1, line_thickness);
			} else if (option == 1) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				potential.Draw(color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				l.Draw(color::Gray);
				c = circle2.Raycast(vel, circle1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					normal.Draw(color::Orange);
					swept.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				circle1.Draw(acolor1, line_thickness);
			} else if (option == 2) {
				V2_float pos = position4;
				V2_float vel{ mouse - pos };
				Line l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				Circle c2{ pos + vel, point_radius };
				c2.Draw(color::Gray, line_thickness);
				l.Draw(color::Gray);
				c = l.Raycast(Rect{ aabb1.Min(), aabb1.size, Origin::TopLeft });
				if (c.Occurred()) {
					V2_float point{ pos + vel * c.t };
					Line normal{ point, point + 50 * c.normal };
					normal.Draw(color::Orange);
					Circle c1{ point, point_radius };
					c1.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
			} else if (option == 3) {
				aabb2.position = position4;
				V2_float vel{ mouse - aabb2.position };
				Rect potential{ aabb2.position + vel, aabb2.size, aabb2.origin };
				potential.Draw(color::Gray, line_thickness);
				Line l{ aabb2.Center(), potential.Center() };
				l.Draw(color::Gray);
				c = aabb2.Raycast(vel, aabb1);
				if (c.Occurred()) {
					Rect swept{ aabb2.position + vel * c.t, aabb2.size, aabb2.origin };
					Line normal{ swept.Center(), swept.Center() + 50 * c.normal };
					normal.Draw(color::Orange);
					swept.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				aabb1.Draw(acolor1, line_thickness);
				aabb2.Draw(acolor2, line_thickness);
			} else if (option == 4) {
				V2_float pos = position4;
				V2_float vel{ mouse - pos };
				Line l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				Circle c1{ pos + vel, point_radius };
				c1.Draw(color::Gray, line_thickness);
				l.Draw(color::Gray);
				c = l.Raycast(line1);
				if (c.Occurred()) {
					V2_float point{ pos + vel * c.t };
					Line normal{ point, point + 50 * c.normal };
					normal.Draw(color::Orange);
					Circle c2{ point, point_radius };
					c2.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				line1.Draw(acolor1, line_thickness);
			} else if (option == 5) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				potential.Draw(color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				l.Draw(color::Gray);
				c = circle2.Raycast(vel, line1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					normal.Draw(color::Orange);
					swept.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				line1.Draw(acolor1, line_thickness);
			} else if (option == 6) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				potential.Draw(color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				l.Draw(color::Gray);
				c = circle2.Raycast(vel, line1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					normal.Draw(color::Orange);
					swept.Draw(color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				circle2.Draw(acolor2, line_thickness);
				line1.Draw(acolor1, line_thickness);
			}
		}
		option = option % options;
	}
};

struct SegmentRectOverlapTest : public CollisionTest {
	void Enter() override {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rect aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };

	void LineOverlap(V2_float p1, V2_float p2, Color color) const {
		Line l1{ p1, p2 };
		Color c;
		if (l1.Overlaps(aabb)) {
			c = color;
		} else {
			c = color::Gray;
		}
		l1.Draw(c);
	}

	void Update() override {
		aabb.Draw(color::Cyan);

		// Lines which are inside the rectangle.

		LineOverlap({ 40.0f, 10.0f }, { 70.0f, 40.0f }, color::Green);	// top left corner
		LineOverlap({ 110.0f, 10.0f }, { 80.0f, 40.0f }, color::Green); // top right corner
		LineOverlap({ 40.0f, 80.0f }, { 70.0f, 50.0f }, color::Green);	// bottom left corner
		LineOverlap({ 110.0f, 80.0f }, { 80.0f, 50.0f }, color::Green); // bottom right corner
		LineOverlap({ 30.0f, 31.0f }, { 70.0f, 31.0f }, color::Green);	// top left to right
		LineOverlap({ 30.0f, 59.0f }, { 70.0f, 59.0f }, color::Green);	// bottom left to right
		LineOverlap({ 120.0f, 31.0f }, { 80.0f, 31.0f }, color::Green); // top right to left
		LineOverlap({ 120.0f, 59.0f }, { 80.0f, 59.0f }, color::Green); // bottom right to left
		LineOverlap({ 61.0f, 10.0f }, { 61.0f, 40.0f }, color::Green);	// top left to bottom
		LineOverlap({ 61.0f, 80.0f }, { 61.0f, 50.0f }, color::Green);	// bottom left to top
		LineOverlap({ 89.0f, 10.0f }, { 89.0f, 40.0f }, color::Green);	// top right to bottom
		LineOverlap({ 89.0f, 80.0f }, { 89.0f, 50.0f }, color::Green);	// bottom right to top

		// Lines which overlap the edges of the rectangle.

		LineOverlap(
			{ 40.0f, 10.0f }, { 60.0f, 30.0f }, color::Red
		); // top left corner - overlapping
		LineOverlap(
			{ 110.0f, 10.0f }, { 90.0f, 30.0f }, color::Red
		); // top right corner - overlapping
		LineOverlap(
			{ 40.0f, 80.0f }, { 60.0f, 60.0f }, color::Red
		); // bottom left corner - overlapping
		LineOverlap(
			{ 110.0f, 80.0f }, { 90.0f, 60.0f }, color::Red
		); // bottom right corner - overlapping
		LineOverlap(
			{ 30.0f, 30.0f }, { 70.0f, 30.0f }, color::Red
		); // top left to right - overlapping
		LineOverlap(
			{ 30.0f, 60.0f }, { 70.0f, 60.0f }, color::Red
		); // bottom left to right - overlapping
		LineOverlap(
			{ 120.0f, 30.0f }, { 80.0f, 30.0f }, color::Red
		); // top right to left - overlapping
		LineOverlap(
			{ 120.0f, 60.0f }, { 80.0f, 60.0f }, color::Red
		); // bottom right to left - overlapping
		LineOverlap(
			{ 60.0f, 10.0f }, { 60.0f, 40.0f }, color::Red
		); // top left to bottom - overlapping
		LineOverlap(
			{ 60.0f, 80.0f }, { 60.0f, 50.0f }, color::Red
		); // bottom left to top - overlapping
		LineOverlap(
			{ 90.0f, 10.0f }, { 90.0f, 40.0f }, color::Red
		); // top right to bottom - overlapping
		LineOverlap(
			{ 90.0f, 80.0f }, { 90.0f, 50.0f }, color::Red
		); // bottom right to top - overlapping
	}
};

struct SegmentRectDynamicTest : public CollisionTest {
	void Enter() override {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rect aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };

	void LineSweep(V2_float p1, V2_float p2, Color color) const {
		Line l1{ p1, p2 };
		l1.Draw(color::Gray);
		auto c{ l1.Raycast(aabb) };
		if (c.Occurred()) {
			V2_float point = l1.a + c.t * l1.Direction();
			point.Draw(color, 2.0f);
		}
	}

	void Update() {
		aabb.Draw(color::Cyan);

		// Lines which are inside the rectangle.

		LineSweep({ 40.0f, 10.0f }, { 70.0f, 40.0f }, color::Green);  // top left corner
		LineSweep({ 110.0f, 10.0f }, { 80.0f, 40.0f }, color::Green); // top right corner
		LineSweep({ 40.0f, 80.0f }, { 70.0f, 50.0f }, color::Green);  // bottom left corner
		LineSweep({ 110.0f, 80.0f }, { 80.0f, 50.0f }, color::Green); // bottom right corner
		LineSweep({ 30.0f, 31.0f }, { 70.0f, 31.0f }, color::Green);  // top left to right
		LineSweep({ 30.0f, 59.0f }, { 70.0f, 59.0f }, color::Green);  // bottom left to right
		LineSweep({ 120.0f, 31.0f }, { 80.0f, 31.0f }, color::Green); // top right to left
		LineSweep({ 120.0f, 59.0f }, { 80.0f, 59.0f }, color::Green); // bottom right to left
		LineSweep({ 61.0f, 10.0f }, { 61.0f, 40.0f }, color::Green);  // top left to bottom
		LineSweep({ 61.0f, 80.0f }, { 61.0f, 50.0f }, color::Green);  // bottom left to top
		LineSweep({ 89.0f, 10.0f }, { 89.0f, 40.0f }, color::Green);  // top right to bottom
		LineSweep({ 89.0f, 80.0f }, { 89.0f, 50.0f }, color::Green);  // bottom right to top

		// LineSweeps which overlap the edges of the rectangle.

		LineSweep({ 40.0f, 10.0f }, { 60.0f, 30.0f }, color::Red); // top left corner - overlapping
		LineSweep(
			{ 110.0f, 10.0f }, { 90.0f, 30.0f }, color::Red
		); // top right corner - overlapping
		LineSweep(
			{ 40.0f, 80.0f }, { 60.0f, 60.0f }, color::Red
		); // bottom left corner - overlapping
		LineSweep(
			{ 110.0f, 80.0f }, { 90.0f, 60.0f }, color::Red
		); // bottom right corner - overlapping
		LineSweep(
			{ 30.0f, 30.0f }, { 70.0f, 30.0f }, color::Red
		); // top left to right - overlapping
		LineSweep(
			{ 30.0f, 60.0f }, { 70.0f, 60.0f }, color::Red
		); // bottom left to right - overlapping
		LineSweep(
			{ 120.0f, 30.0f }, { 80.0f, 30.0f }, color::Red
		); // top right to left - overlapping
		LineSweep(
			{ 120.0f, 60.0f }, { 80.0f, 60.0f }, color::Red
		); // bottom right to left - overlapping
		LineSweep(
			{ 60.0f, 10.0f }, { 60.0f, 40.0f }, color::Red
		); // top left to bottom - overlapping
		LineSweep(
			{ 60.0f, 80.0f }, { 60.0f, 50.0f }, color::Red
		); // bottom left to top - overlapping
		LineSweep(
			{ 90.0f, 10.0f }, { 90.0f, 40.0f }, color::Red
		); // top right to bottom - overlapping
		LineSweep(
			{ 90.0f, 80.0f }, { 90.0f, 50.0f }, color::Red
		); // bottom right to top - overlapping
	}
};

struct RectRectDynamicTest : public CollisionTest {
	void Enter() override {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rect aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };
	Rect target{ {}, { 10.0f, 10.0f }, Origin::Center };

	void RectSweep(V2_float p1, V2_float p2, Color color, bool debug_flag = false) {
		target.position = p1;
		target.Draw(color::Gray);
		Line l1{ p1, p2 };
		l1.Draw(color::Gray);
		V2_float vel = p2 - p1;
		auto c{ target.Raycast(vel, aabb) };
		if (c.Occurred()) {
			Rect new_rect	  = target;
			new_rect.position = p1 + c.t * vel;
			new_rect.Draw(color);
			if (new_rect.Overlaps(aabb)) {
				PTGN_LOG("still overlapping");
			}
		} else {
			Rect new_rect	  = target;
			new_rect.position = p1 + vel;
			new_rect.Draw(color::Gray);
		}
	}

	void Update() override {
		aabb.Draw(color::Cyan);

		// Rects which are inside the rectangle.

		RectSweep({ 40.0f, 10.0f }, { 70.0f, 40.0f }, color::Green);  // top left corner
		RectSweep({ 110.0f, 10.0f }, { 80.0f, 40.0f }, color::Green); // top right corner
		RectSweep({ 40.0f, 80.0f }, { 70.0f, 50.0f }, color::Green);  // bottom left corner
		RectSweep({ 110.0f, 80.0f }, { 80.0f, 50.0f }, color::Green); // bottom right corner
		RectSweep({ 30.0f, 31.0f }, { 70.0f, 31.0f }, color::Green);  // top left to right
		RectSweep({ 30.0f, 59.0f }, { 70.0f, 59.0f }, color::Green);  // bottom left to right
		RectSweep({ 120.0f, 31.0f }, { 80.0f, 31.0f }, color::Green); // top right to left
		RectSweep({ 120.0f, 59.0f }, { 80.0f, 59.0f }, color::Green); // bottom right to left
		RectSweep({ 61.0f, 10.0f }, { 61.0f, 40.0f }, color::Green);  // top left to bottom
		RectSweep({ 61.0f, 80.0f }, { 61.0f, 50.0f }, color::Green);  // bottom left to top
		RectSweep({ 89.0f, 10.0f }, { 89.0f, 40.0f }, color::Green);  // top right to bottom
		RectSweep({ 89.0f, 80.0f }, { 89.0f, 50.0f }, color::Green);  // bottom right to top

		// RectSweeps which overlap the edges of the rectangle.

		RectSweep(
			{ 40.0f, 10.0f }, V2_float{ 60.0f, 30.0f } - target.Half(), color::Red
		); // top left corner - overlapping
		RectSweep(
			{ 110.0f, 10.0f },
			V2_float{ 90.0f, 30.0f } + V2_float{ target.Half().x, -target.Half().y }, color::Red
		); // top right corner - overlapping
		RectSweep(
			{ 40.0f, 80.0f },
			V2_float{ 60.0f, 60.0f } + V2_float{ -target.Half().x, target.Half().y }, color::Red
		); // bottom left corner - overlapping
		RectSweep(
			{ 110.0f, 80.0f }, V2_float{ 90.0f, 60.0f } + target.Half(), color::Red
		); // bottom right corner - overlapping
		RectSweep(
			{ 30.0f, 30.0f - target.Half().y }, V2_float{ 70.0f, 30.0f - target.Half().y },
			color::Red
		); // top left to right - overlapping
		RectSweep(
			{ 30.0f, 60.0f + target.Half().y }, V2_float{ 70.0f, 60.0f + target.Half().y },
			color::Red
		); // bottom left to right - overlapping
		RectSweep(
			{ 120.0f, 30.0f - target.Half().y }, V2_float{ 80.0f, 30.0f - target.Half().y },
			color::Red
		); // top right to left - overlapping
		RectSweep(
			{ 120.0f, 60.0f + target.Half().y }, V2_float{ 80.0f, 60.0f + target.Half().y },
			color::Red
		); // bottom right to left - overlapping
		RectSweep(
			{ 60.0f - target.Half().x, 10.0f }, V2_float{ 60.0f - target.Half().x, 40.0f },
			color::Red
		); // top left to bottom - overlapping
		RectSweep(
			{ 60.0f - target.Half().x, 80.0f }, V2_float{ 60.0f - target.Half().x, 50.0f },
			color::Red
		); // bottom left to top - overlapping
		RectSweep(
			{ 90.0f + target.Half().x, 10.0f }, V2_float{ 90.0f + target.Half().x, 40.0f },
			color::Red
		); // top right to bottom - overlapping
		RectSweep(
			{ 90.0f + target.Half().x, 80.0f }, V2_float{ 90.0f + target.Half().x, 50.0f },
			color::Red
		); // bottom right to top - overlapping
	}
};

struct SweepTest : public CollisionTest {
	ecs::Entity player;
	V2_float player_start_pos;
	V2_float player_velocity;
	V2_float fixed_velocity;

	V2_float size;

	// For circles, radius is set to s.x. Don't laugh.
	ecs::Entity AddCollisionObject(
		const V2_float& p, const V2_float& s = {}, const V2_float& v = {},
		Origin o = Origin::Center, bool is_circle = false
	) {
		ecs::Entity entity = manager.CreateEntity();
		auto& t			   = entity.SetPosition({});
		t.position		   = p;

		if (!is_circle) {
			auto& box = entity.Add<BoxCollider>(entity);
			if (s.IsZero()) {
				box.size = size;
			} else {
				box.size = s;
			}
			box.origin = o;
		} else {
			auto& circle = entity.Add<CircleCollider>(entity);
			if (s.IsZero()) {
				circle.radius = size.x;
			} else {
				circle.radius = s.x;
			}
		}

		if (!v.IsZero()) {
			auto& rb	= entity.Add<RigidBody>();
			rb.velocity = v;
		}

		return entity;
	}

	void AddPlayer(
		const V2_float& player_vel, const V2_float& player_size = { 50, 50 },
		const V2_float& player_pos = { 0, 0 }, const V2_float& obstacle_size = { 50, 50 },
		const V2_float& fixed_vel = {}, Origin origin = Origin::Center,
		bool player_is_circle = false
	) {
		this->player_velocity  = player_vel;
		this->size			   = obstacle_size;
		this->fixed_velocity   = fixed_vel;
		this->player_start_pos = player_pos;
		this->player =
			AddCollisionObject(player_pos, player_size, player_vel, origin, player_is_circle);
	}

	void Enter() override {
		PTGN_ASSERT(player.Has<Transform>());
		auto& t	   = player.GetTransform();
		t.position = player_start_pos;

		if (player.Has<BoxCollider>()) {
			auto& box		 = player.Get<BoxCollider>();
			box.response	 = CollisionResponse::Slide;
			box.overlap_only = false;
			box.continuous	 = true;
		} else if (player.Has<CircleCollider>()) {
			auto& circle		= player.Get<CircleCollider>();
			circle.response		= CollisionResponse::Slide;
			circle.overlap_only = false;
			circle.continuous	= true;
		}


	}

	void Update() override {
		auto& rb		= player.Get<RigidBody>();
		auto& transform = player.GetTransform();

		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			Rect r{ GetPosition(e), b.size, b.origin };
			if (e == player) {
				r.Draw(color::Green);
			} else {
				r.Draw(color::Blue);
			}
		}
		for (auto [e, c] : manager.EntitiesWith<CircleCollider>()) {
			Circle circle{ GetPosition(e), c.radius };
			if (e == player) {
				circle.Draw(color::Green);
			} else {
				circle.Draw(color::Blue);
			}
		}

		if (player.Has<BoxCollider>()) {
			auto& box = player.Get<BoxCollider>();
			Rect{ transform.position + rb.velocity * game.dt(), box.size, box.origin }.Draw(
				color::DarkGreen
			);
		} else if (player.Has<CircleCollider>()) {
			auto& circle = player.Get<CircleCollider>();
			Circle{ transform.position + rb.velocity * game.dt(), circle.radius }.Draw(
				color::DarkGreen, 1.0f
			);
		}

		if (!fixed_velocity.IsZero() && !game.input.KeyPressed(Key::A) &&
			!game.input.KeyPressed(Key::D) && !game.input.KeyPressed(Key::S) &&
			!game.input.KeyPressed(Key::W)) {
			rb.velocity = fixed_velocity;
		} else {
			rb.velocity = {};
		}

		if (game.input.KeyPressed(Key::A)) {
			rb.velocity.x = -player_velocity.x;
		}
		if (game.input.KeyPressed(Key::D)) {
			rb.velocity.x = player_velocity.x;
		}
		if (game.input.KeyPressed(Key::W)) {
			rb.velocity.y = -player_velocity.y;
		}
		if (game.input.KeyPressed(Key::S)) {
			rb.velocity.y = player_velocity.y;
		}

		auto boxes	 = manager.EntitiesWith<BoxCollider>();
		auto circles = manager.EntitiesWith<CircleCollider>();

		if (player.Has<BoxCollider>()) {
			auto& collider{ player.Get<BoxCollider>() };
			game.collision.Sweep(player, collider, boxes, circles, true);
			game.collision.Intersect(player, collider, boxes, circles);
		} else if (player.Has<CircleCollider>()) {
			auto& collider{ player.Get<CircleCollider>() };
			game.collision.Sweep(player, collider, boxes, circles, true);
			game.collision.Intersect(player, collider, boxes, circles);
		}

		if (game.input.KeyDown(Key::Space)) {
			transform.position += rb.velocity * game.dt();
		}

		const auto edge_exclusive_overlap = [](const Rect& a, const Rect& b) {
			const V2_float a_max{ a.Max() };
			const V2_float a_min{ a.Min() };
			const V2_float b_max{ b.Max() };
			const V2_float b_min{ b.Min() };

			if (a_max.x <= b_min.x || a_min.x >= b_max.x) {
				return false;
			}
			if (a_max.y <= b_min.y || a_min.y >= b_max.y) {
				return false;
			}
			return true;
		};

		if (game.input.KeyPressed(Key::R)) {
			transform.position = {};
			rb.velocity		   = {};
		}
	}

	void Draw() override {

		V2_int grid_size = game.window.GetSize() / size;

		for (std::size_t i = 0; i < grid_size.x; i++) {
			for (std::size_t j = 0; j < grid_size.y; j++) {
				V2_float pos{ i * size.x, j * size.y };
				game.draw.Rect(pos, size, color::Black, Origin::Center, 1.0f);
			}
		}
}
}
;

struct RectCollisionTest : public SweepTest {
	void Enter() override {

		AddPlayer({ 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 45.0f, 84.5f });
		AddCollisionObject({ 150.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 150.0f, 150.0f }, { 75.0f, 20.0f });
		AddCollisionObject({ 170.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 190.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 110.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 50.0f, 130.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 20.0f, 90.0f }, { 20.0f, 90.0f });
		AddCollisionObject({ 50.0f, 150.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 50.0f, 170.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
		AddCollisionObject({ 200.0f, 100.0f }, { 20.0f, 60.0f });
		AddCollisionObject({ 50.0f, 200.0f }, { 40.0f, 20.0f });
		AddCollisionObject({ 50.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 200.0f, 10.0f }, { 20.0f, 20.0f });
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Enter();
	}
};

struct RectCollisionTest1 : public SweepTest {
	void Enter() override {

		AddPlayer(
			{ 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 45.0f, 84.5f }, { 50, 50 },
			{ 100000.0f, 100000.0f }
		);
		AddCollisionObject({ 150.0f, 150.0f }, { 75.0f, 20.0f });
		AddCollisionObject({ 50.0f, 130.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
		AddCollisionObject({ 200.0f, 100.0f }, { 20.0f, 60.0f });
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Enter();
	}
};

struct RectCollisionTest2 : public SweepTest {
	void Enter() override {

		AddPlayer(
			{ 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 25.0f, 30.0f }, { 50, 50 },
			{ -100000.0f, 100000.0f }
		);
		AddCollisionObject({ 20.0f, 90.0f }, { 20.0f, 90.0f });
		AddCollisionObject({ 50.0f, 50.0f }, { 20.0f, 20.0f });
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Enter();
	}
};

struct RectCollisionTest3 : public SweepTest {
	void Enter() override {

		AddPlayer(
			{ 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 175.0f, 75.0f }, { 50, 50 },
			{ -100000.0f, 100000.0f }
		);
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Enter();
	}
};

struct RectCollisionTest4 : public SweepTest {
	void Enter() override {

		AddPlayer(
			{ 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 97.5000000f, 74.9999924f }, { 50, 50 },
			{ 100000.0f, -100000.0f }
		);
		AddCollisionObject({ 150.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 110.0f, 50.0f }, { 20.0f, 20.0f });
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Enter();
	}
};

struct CircleRectCollisionTest1 : public SweepTest {
	void Enter() override {

		AddPlayer(
			{ 10000.0f, 10000.0f }, { 30.0f, 30.0f }, { 563.608337f, 623.264038f },
			{ 50.0f, 50.0f }, { 0.00000000f, 10000.0f }, Origin::Center, true
		);
		AddCollisionObject(V2_float{ 50, 650 }, V2_float{ 500, 10 }, {}, Origin::TopLeft);
		game.camera.GetPrimary().CenterOnArea({ 800, 800 });
		SweepTest::Enter();
	}
};

struct DynamicRectCollisionTest : public CollisionTest {
	float speed{ 0.0f };

	struct DynamicData {
		V2_float position;
		V2_float size;
		Origin origin;
		V2_float velocity;
	};

	std::vector<DynamicData> entity_data;

	using Id = std::size_t;

	explicit DynamicRectCollisionTest(float speed) : speed{ speed } {
		ws = game.window.GetSize();
	}

	void CreateDynamicEntity(
		const V2_float& pos, const V2_float& size, Origin origin, const V2_float& velocity_direction
	) {
		DynamicData data;
		data.position = pos;
		data.size	  = size;
		data.origin	  = origin;
		data.velocity = velocity_direction * speed;
		entity_data.push_back(data);
	}

	using NextVel = V2_float;

	void Enter() override {


		for (std::size_t i = 0; i < entity_data.size(); ++i) {
			ecs::Entity entity = manager.CreateEntity();
			const auto& data   = entity_data[i];
			auto& t			   = entity.SetPosition({});
			t.position		   = data.position;

			auto& box		 = entity.Add<BoxCollider>(entity);
			box.size		 = data.size;
			box.origin		 = data.origin;
			box.continuous	 = true;
			box.overlap_only = false;
			box.response	 = CollisionResponse::Slide;

			auto& rb	= entity.Add<RigidBody>();
			rb.velocity = data.velocity;

			entity.Add<NextVel>();

			entity.Add<Id>(i);
		}

	}

	void Update() override {
		bool space_down = game.input.KeyDown(Key::Space);
		for (auto [e, rb, id] : manager.EntitiesWith<RigidBody, Id>()) {
			PTGN_ASSERT(id < entity_data.size());
			rb.velocity = entity_data[id].velocity;
		}

		auto boxes	 = manager.EntitiesWith<BoxCollider>();
		auto circles = manager.EntitiesWith<CircleCollider>();

		for (auto [e, b, rb, id, nv] :
			 manager.EntitiesWith<BoxCollider, RigidBody, Id, NextVel>()) {
			game.collision.Sweep(e, b, boxes, circles, true);
			game.collision.Intersect(e, b, boxes, circles);
		}

		for (auto [e, b, rb, id, nv] :
			 manager.EntitiesWith<BoxCollider, RigidBody, Id, NextVel>()) {
			 auto& t{ GetTransform(e) };
			if (space_down) {
				t.position += rb.velocity * game.dt();
			}
			for (auto [e2, b2, rb2] :
				 manager.EntitiesWith<BoxCollider, RigidBody>()) {
				if (e2 == e) {
					continue;
				}
				auto& t2{ GetTransform(e2) };
				Rect r1{ t.position + b.offset, b.size, b.origin };
				Rect r2{ t2.position + b2.offset, b2.size, b2.origin };
				Intersection c{ r1.Intersects(r2) };
				if (c.Occurred()) {
					t.position += c.normal * c.depth;
				}
				if (r1.Overlaps(r2)) {
					PTGN_LOG("Intersection after sweep | normal: ", c.normal, ", depth: ", c.depth);
				}
			}
		}
	}

	void Draw() override {
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			Rect{ GetPosition(e) + b.offset, b.size, b.origin }.Draw(color::Green);
		}
	}
};

struct HeadOnDynamicRectTest1 : public DynamicRectCollisionTest {
	HeadOnDynamicRectTest1(float speed) : DynamicRectCollisionTest{ speed } {
		CreateDynamicEntity(
			{ 0, game.window.GetCenter().y }, { 40.0f, 40.0f }, Origin::CenterLeft, { 1.0f, 0.0f }
		);
		CreateDynamicEntity(
			{ ws.x, game.window.GetCenter().y }, { 40.0f, 40.0f }, Origin::CenterRight,
			{ -1.0f, 0.0f }
		);
	}
};

struct HeadOnDynamicRectTest2 : public DynamicRectCollisionTest {
	HeadOnDynamicRectTest2(float speed) : DynamicRectCollisionTest{ speed } {
		CreateDynamicEntity(
			{ game.window.GetCenter().x, 0 }, { 40.0f, 40.0f }, Origin::CenterTop, { 0, 1.0f }
		);
		CreateDynamicEntity(
			{ game.window.GetCenter().x, ws.y }, { 40.0f, 40.0f }, Origin::CenterBottom,
			{ 0, -1.0f }
		);
		CreateDynamicEntity(
			{ 0, game.window.GetCenter().y }, { 40.0f, 40.0f }, Origin::CenterLeft, { 1.0f, 0.0f }
		);
		CreateDynamicEntity(
			{ ws.x, game.window.GetCenter().y }, { 40.0f, 40.0f }, Origin::CenterRight,
			{ -1.0f, 0.0f }
		);
	}
};

struct SweepCornerTest1 : public SweepTest {
	V2_float player_vel;

	SweepCornerTest1(const V2_float& player_vel) : player_vel{ player_vel } {}

	void Enter() override {

		AddPlayer(player_vel);
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 300, 250 });
		SweepTest::Enter();
	}
};

struct SweepCornerTest2 : public SweepTest {
	V2_float player_vel;

	SweepCornerTest2(const V2_float& player_vel) : player_vel{ player_vel } {}

	void Enter() override {

		AddPlayer(player_vel);
		AddCollisionObject({ 300 - 10, 300 });
		AddCollisionObject({ 250 - 10, 300 });
		AddCollisionObject({ 300 - 10, 250 });
		SweepTest::Enter();
	}
};

struct SweepCornerTest3 : public SweepTest {
	V2_float player_vel;

	SweepCornerTest3(const V2_float& player_vel) : player_vel{ player_vel } {}

	void Enter() override {

		AddPlayer(player_vel);
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 250, 250 });
		SweepTest::Enter();
	}
};

struct SweepTunnelTest1 : public SweepTest {
	V2_float player_vel;

	SweepTunnelTest1(const V2_float& player_vel) : player_vel{ player_vel } {}

	void Enter() override {

		AddPlayer(player_vel);
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 300, 250 });
		AddCollisionObject({ 200, 350 });
		AddCollisionObject({ 300, 350 });
		AddCollisionObject({ 250, 400 });
		AddCollisionObject({ 200, 400 });
		AddCollisionObject({ 300, 400 });
		SweepTest::Enter();
	}
};

struct SweepTunnelTest2 : public SweepTest {
	V2_float player_vel;

	SweepTunnelTest2(const V2_float& player_vel) : player_vel{ player_vel } {}

	void Enter() override {

		AddPlayer(player_vel);
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 300, 200 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 350, 300 });
		AddCollisionObject({ 350, 200 });
		AddCollisionObject({ 400, 200 });
		AddCollisionObject({ 400, 250 });
		AddCollisionObject({ 400, 300 });
		SweepTest::Enter();
	}
};
*/

class CollisionScene : public Scene {
public:
	int current_test{ 0 };

	V2_float velocity{ 100000.0f };
	float speed{ 7000.0f };

	std::vector<std::shared_ptr<CollisionTest>> tests;

	void Enter() override {
		ws = game.window.GetSize();

		// TODO: Rework this whole test thing.
		tests.emplace_back(new CollisionCallbackTest());
		/*
		tests.emplace_back(new SweepEntityCollisionTest());
		tests.emplace_back(new RectangleSweepTest());
		tests.emplace_back(new GeneralCollisionTest());
		tests.emplace_back(new PointOverlapTest());
		tests.emplace_back(new LineOverlapTest());
		tests.emplace_back(new CircleOverlapTest());
		tests.emplace_back(new RectOverlapTest());
		tests.emplace_back(new CapsuleOverlapTest());
		tests.emplace_back(new CircleRectCollisionTest1());
		tests.emplace_back(new RectCollisionTest4());
		tests.emplace_back(new RectCollisionTest3());
		tests.emplace_back(new HeadOnDynamicRectTest1(speed));
		tests.emplace_back(new HeadOnDynamicRectTest2(speed));
		tests.emplace_back(new RectCollisionTest());
		tests.emplace_back(new RectCollisionTest1());
		tests.emplace_back(new RectCollisionTest2());
		tests.emplace_back(new SegmentRectOverlapTest());
		tests.emplace_back(new RectRectDynamicTest());
		tests.emplace_back(new SegmentRectDynamicTest());
		tests.emplace_back(new SweepTunnelTest2(velocity));
		tests.emplace_back(new SweepTunnelTest1(velocity));
		tests.emplace_back(new SweepCornerTest3(velocity));
		tests.emplace_back(new SweepCornerTest2(velocity));
		tests.emplace_back(new SweepCornerTest1(velocity));
		*/

		tests[static_cast<std::size_t>(current_test)]->Enter();
	}

	void Update() override {
		ws = game.window.GetSize();
		if (game.input.KeyDown(Key::Left)) {
			tests[static_cast<std::size_t>(current_test)]->Exit();
			current_test--;
			current_test = Mod(current_test, static_cast<int>(tests.size()));
			tests[static_cast<std::size_t>(current_test)]->Enter();
		} else if (game.input.KeyDown(Key::Right)) {
			tests[static_cast<std::size_t>(current_test)]->Exit();
			current_test++;
			current_test = Mod(current_test, static_cast<int>(tests.size()));
			tests[static_cast<std::size_t>(current_test)]->Enter();
		}
		tests[static_cast<std::size_t>(current_test)]->Update();
		tests[static_cast<std::size_t>(current_test)]->Draw();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("CollisionScene:  Arrow keys to flip between tests", window_size);
	game.scene.Enter<CollisionScene>("");
	return 0;
}