#include <memory>
#include <new>
#include <vector>

#include "camera/camera.h"
#include "collision/collider.h"
#include "collision/collision.h"
#include "collision/raycast.h"
#include "common.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/geometry/circle.h"
#include "math/geometry/intersection.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/movement.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/color.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"

class CollisionCallbackTest : public Test {
public:
	ecs::Manager manager;
	ecs::Entity intersect;
	ecs::Entity overlap;
	ecs::Entity sweep;
	ecs::Entity intersect_circle;
	ecs::Entity overlap_circle;
	ecs::Entity sweep_circle;

	// Total number.
	const int move_entities{ 6 };
	// Current move entity.
	int move_entity{ 5 };
	V2_float speed{ 300.0f };

	void Init() override {
		manager.Clear();

		intersect		 = manager.CreateEntity();
		sweep			 = manager.CreateEntity();
		overlap			 = manager.CreateEntity();
		intersect_circle = manager.CreateEntity();
		sweep_circle	 = manager.CreateEntity();
		overlap_circle	 = manager.CreateEntity();

		intersect.Add<Transform>(V2_float{ 100, 100 });
		overlap.Add<Transform>(V2_float{ 200, 200 });
		sweep.Add<Transform>(V2_float{ 300, 300 });
		intersect_circle.Add<Transform>(V2_float{ 400, 400 });
		overlap_circle.Add<Transform>(V2_float{ 500, 500 });
		sweep_circle.Add<Transform>(V2_float{ 300, 600 });

		intersect.Add<RigidBody>();
		overlap.Add<RigidBody>();
		sweep.Add<RigidBody>();
		intersect_circle.Add<RigidBody>();
		overlap_circle.Add<RigidBody>();
		sweep_circle.Add<RigidBody>();

		intersect.Add<BoxCollider>(intersect, V2_float{ 30, 30 });
		overlap.Add<BoxCollider>(overlap, V2_float{ 30, 30 });
		sweep.Add<BoxCollider>(sweep, V2_float{ 30, 30 });
		intersect_circle.Add<CircleCollider>(intersect_circle, 30.0f);
		overlap_circle.Add<CircleCollider>(overlap_circle, 30.0f);
		sweep_circle.Add<CircleCollider>(sweep_circle, 30.0f);

		auto& b1{ intersect.Get<BoxCollider>() };
		auto& b2{ overlap.Get<BoxCollider>() };
		auto& b3{ sweep.Get<BoxCollider>() };
		auto& c1{ intersect_circle.Get<CircleCollider>() };
		auto& c2{ overlap_circle.Get<CircleCollider>() };
		auto& c3{ sweep_circle.Get<CircleCollider>() };

		b2.overlap_only = true;
		b3.continuous	= true;
		c2.overlap_only = true;
		c3.continuous	= true;

		b1.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b1.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b1.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};

		b2.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b2.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b2.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};

		b3.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b3.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		b3.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c1.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c1.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c1.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped intersect collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};

		c2.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c2.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c2.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped overlap collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};

		c3.on_collision_start = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " started sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c3.on_collision = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " continued sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};
		c3.on_collision_stop = [](Collision c) {
			PTGN_LOG(
				"#", c.entity1.GetId(), " stopped sweep collision with #", c.entity2.GetId(),
				", normal: ", c.normal
			);
		};

		CreateObstacle(V2_float{ 50, 50 }, V2_float{ 10, 500 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 600, 200 }, V2_float{ 10, 500 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 50, 650 }, V2_float{ 500, 10 }, Origin::TopLeft);
		CreateObstacle(V2_float{ 100, 70 }, V2_float{ 500, 10 }, Origin::TopLeft);

		manager.Refresh();
	}

	void CreateObstacle(const V2_float& pos, const V2_float& size, Origin origin) {
		auto obstacle = manager.CreateEntity();
		obstacle.Add<Transform>(pos);
		obstacle.Add<BoxCollider>(obstacle, size, origin);
	}

	void Update() override {
		if (game.input.KeyDown(Key::E)) {
			move_entity++;
		}
		if (game.input.KeyDown(Key::E)) {
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

		MoveWASD(*vel, speed * game.physics.dt());

		game.physics.Update(manager);
	}

	void Draw() override {
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			Rect r{ b.GetAbsoluteRect() };
			DrawRect(e, r);
			if (e == intersect) {
				game.draw.Text("Intersect", color::Black, Rect{ r.Center() });
			} else if (e == overlap) {
				game.draw.Text("Overlap", color::Black, Rect{ r.Center() });
			} else if (e == sweep) {
				game.draw.Text("Sweep", color::Black, Rect{ r.Center() });
			}
		}
		for (auto [e, c] : manager.EntitiesWith<CircleCollider>()) {
			Circle circ{ c.GetAbsoluteCircle() };
			DrawCircle(e, circ);
			if (e == intersect_circle) {
				game.draw.Text("Intersect", color::Black, Rect{ circ.Center() });
			} else if (e == overlap_circle) {
				game.draw.Text("Overlap", color::Black, Rect{ circ.Center() });
			} else if (e == sweep_circle) {
				game.draw.Text("Sweep", color::Black, Rect{ circ.Center() });
			}
		}
	}
};

class ShapeCollisionTest : public Test {
public:
	V2_float p0{ 10, 10 };
	V2_float p1{ 20, 20 };

	Line l1{ { 3, 3 }, { 3, 10 } };
	Line l2{ { 3, 3 }, { 10, 3 } };
	Line l3{ { 3, 3 }, { 10, 10 } };

	Circle c1{ { 15, 9 }, 7 };
	Rect r1{ { 15, 40 }, { 20, 15 } };

	Capsule ca1{ { 15, 30 }, { 15, 50 }, 7 };
	Capsule ca2{ { 15, 30 }, { 50, 15 }, 7 };
	Capsule ca3{ { 15, 30 }, { 50, 50 }, 7 };

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 100.0f, 100.0f });
	}

	void Update() override {
		if (game.input.MouseDown(Mouse::Left)) {
			p0 = game.input.GetMousePosition();
		}
		if (game.input.MouseDown(Mouse::Right)) {
			p1 = game.input.GetMousePosition();
		}
	}
};

class PointOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		p1 = game.input.GetMousePosition();

		V2_float c0{ p1 };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s2.Overlaps(s1)) {
				s1.Draw(color::Red);
				s2.Draw(color::Red);
			} else {
				s2.Draw(color::Green);
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
		overlap(c0, ca2);
		overlap(c0, ca3);
	}
};

class LineOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		ShapeCollisionTest::Update();

		Line c0{ p0, p1 };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s1.Overlaps(s2)) {
				s1.Draw(color::Red);
				s2.Draw(color::Red);
			} else {
				s2.Draw(color::Green);
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
		overlap(c0, ca2);
		overlap(c0, ca3);
	}
};

class CircleOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		p1 = game.input.GetMousePosition();

		Circle c0{ p1, 10.0f };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s1.Overlaps(s2)) {
				s1.Draw(color::Red);
				s2.Draw(color::Red);
			} else {
				s2.Draw(color::Green);
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
		overlap(c0, ca2);
		overlap(c0, ca3);
	}
};

class RectOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		p1 = game.input.GetMousePosition();

		Rect c0{ p1, { 20, 20 }, Origin::Center, 0.0f };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s1.Overlaps(s2)) {
				s1.Draw(color::Red);
				s2.Draw(color::Red);
			} else {
				s2.Draw(color::Green);
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
		overlap(c0, ca2);
		overlap(c0, ca3);
	}
};

class CapsuleOverlapTest : public ShapeCollisionTest {
public:
	void Update() override {
		ShapeCollisionTest::Update();

		Capsule c0{ p0, p1, 10.0f };
		c0.Draw(color::Green);

		const auto overlap = [](auto s1, auto s2) {
			if (s1.Overlaps(s2)) {
				s1.Draw(color::Red);
				s2.Draw(color::Red);
			} else {
				s2.Draw(color::Green);
			}
		};

		overlap(c0, l1);
		overlap(c0, l2);
		overlap(c0, l3);
		overlap(c0, c1);
		overlap(c0, r1);
		overlap(c0, ca1);
		overlap(c0, ca2);
		overlap(c0, ca3);
	}
};

/*
class CollisionTest2 : public Test {
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

	void Init() override {
		game.window.SetTitle("'ESC' (++category), 't' (++shape), 'g' (++mode), 'r' (reset pos)");
	}

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
			rot_1 -= rot_speed * dt;
		}
		if (game.input.KeyPressed(Key::E)) {
			rot_1 += rot_speed * dt;
		}
		if (game.input.KeyPressed(Key::Z)) {
			rot_2 -= rot_speed * dt;
		}
		if (game.input.KeyPressed(Key::C)) {
			rot_2 += rot_speed * dt;
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
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
				game.draw.Point(position2, acolor2);
			} else if (option == 2) {
				if (aabb1.Overlaps(position2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Point(position2, acolor2);
			} else if (option == 3) {
				if (line2.Overlaps(line1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line1.a, line1.b, acolor1);
				game.draw.Line(line2.a, line2.b, acolor2);
			} else if (option == 4) {
				if (line2.Overlaps(circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line2.a, line2.b, acolor2);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			} else if (option == 5) {
				if (line2.Overlaps(aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line2.a, line2.b, acolor2);
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 6) {
				if (circle2.Overlaps(circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			} else if (option == 7) {
				if (circle2.Overlaps(aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
			} else if (option == 8) {
				aabb2.position = mouse;
				// no rotation case uses different overlap code.
				aabb1.rotation = 0.0f;
				aabb2.rotation = 0.0f;
				if (aabb1.Overlaps(aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(aabb2.position, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 9) {
				aabb2.position = mouse;
				if (aabb1.Overlaps(aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(
					aabb2.position, aabb2.size, acolor2, aabb2.origin, line_thickness, rot_2
				);
				game.draw.Rect(
					aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness, rot_1
				);
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
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
				if (c.Occurred()) {
					Circle new_circle{ circle2.center + c.normal * (c.depth + slop),
									   circle2.radius };
					game.draw.Circle(new_circle.center, new_circle.radius, color2, line_thickness);
					Line l{ circle2.center, new_circle.center };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				if (c.Occurred()) {
					Circle new_circle{ circle2.center + c.normal * (c.depth + slop),
									   circle2.radius };
					game.draw.Circle(new_circle.center, new_circle.radius, color2, line_thickness);
					Line l{ circle2.center, new_circle.center };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Rect(aabb2.position, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
				if (c.Occurred()) {
					Rect new_aabb{ aabb2.position + c.normal * (c.depth + slop), aabb2.size,
								   aabb2.origin };
					game.draw.Rect(
						new_aabb.position, new_aabb.size, color2, new_aabb.origin, line_thickness
					);
					Line l{ aabb2.Center(), new_aabb.Center() };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Rect(
					aabb2.position, aabb2.size, acolor2, aabb2.origin, line_thickness, rot_2
				);
				game.draw.Rect(
					aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness, rot_1
				);
				if (c.Occurred()) {
					Rect new_aabb{ aabb2.position + c.normal * (c.depth + slop), aabb2.size,
								   aabb2.origin };
					game.draw.Rect(
						new_aabb.position, new_aabb.size, color2, new_aabb.origin, line_thickness,
						rot_2
					);
					Line l{ aabb2.Center(), new_aabb.Center() };
					game.draw.Line(l.a, l.b, color::Gold);
				}
			}
		} else if (type == 2) { // dynamic
			options = 7;
			Raycast c;
			if (option == 0) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				game.draw.Circle(potential.center, potential.radius, color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Gray);
				c = circle2.Raycast(vel, aabb1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(swept.center, swept.radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 1) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				game.draw.Circle(potential.center, potential.radius, color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Gray);
				c = circle2.Raycast(vel, circle1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(swept.center, swept.radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			} else if (option == 2) {
				V2_float pos = position4;
				V2_float vel{ mouse - pos };
				Line l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				game.draw.Circle(pos + vel, point_radius, color::Gray, line_thickness);
				game.draw.Line(l.a, l.b, color::Gray);
				c = l.Raycast(Rect{ aabb1.Min(), aabb1.size, Origin::TopLeft });
				if (c.Occurred()) {
					V2_float point{ pos + vel * c.t };
					Line normal{ point, point + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(point, point_radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 3) {
				aabb2.position = position4;
				V2_float vel{ mouse - aabb2.position };
				Rect potential{ aabb2.position + vel, aabb2.size, aabb2.origin };
				game.draw.Rect(
					potential.position, potential.size, color::Gray, potential.origin,
					line_thickness
				);
				Line l{ aabb2.Center(), potential.Center() };
				game.draw.Line(l.a, l.b, color::Gray);
				c = aabb2.Raycast(vel, aabb1);
				if (c.Occurred()) {
					Rect swept{ aabb2.position + vel * c.t, aabb2.size, aabb2.origin };
					Line normal{ swept.Center(), swept.Center() + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Rect(
						swept.position, swept.size, color::Green, swept.origin, line_thickness
					);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rect(aabb2.position, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rect(aabb1.position, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 4) {
				V2_float pos = position4;
				V2_float vel{ mouse - pos };
				Line l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				game.draw.Circle(pos + vel, point_radius, color::Gray, line_thickness);
				game.draw.Line(l.a, l.b, color::Gray);
				c = l.Raycast(line1);
				if (c.Occurred()) {
					V2_float point{ pos + vel * c.t };
					Line normal{ point, point + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(point, point_radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line1.a, line1.b, acolor1, line_thickness);
			} else if (option == 5) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				game.draw.Circle(potential.center, potential.radius, color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Gray);
				c = circle2.Raycast(vel, line1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(swept.center, swept.radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Line(line1.a, line1.b, acolor1, line_thickness);
			} else if (option == 6) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle potential{ circle2.center + vel, circle2.radius };
				game.draw.Circle(potential.center, potential.radius, color::Gray, line_thickness);
				Line l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Gray);
				c = circle2.Raycast(vel, line1);
				if (c.Occurred()) {
					Circle swept{ circle2.center + vel * c.t, circle2.radius };
					Line normal{ swept.center, swept.center + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(swept.center, swept.radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Line(line1.a, line1.b, acolor1, line_thickness);
			}
		}
		option = option % options;
	}
};
*/

struct SegmentRectOverlapTest : public Test {
	void Init() override {
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
		game.draw.Line(l1.a, l1.b, c);
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

struct SegmentRectDynamicTest : public Test {
	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rect aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };

	void LineSweep(V2_float p1, V2_float p2, Color color) const {
		Line l1{ p1, p2 };
		game.draw.Line(l1.a, l1.b, color::Gray);
		auto c{ l1.Raycast(aabb) };
		if (c.Occurred()) {
			V2_float point = l1.a + c.t * l1.Direction();
			game.draw.Point(point, color, 2.0f);
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

struct RectRectDynamicTest : public Test {
	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rect aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };
	Rect target{ {}, { 10.0f, 10.0f }, Origin::Center };

	void RectSweep(V2_float p1, V2_float p2, Color color, bool debug_flag = false) {
		target.position = p1;
		target.Draw(color::Gray);
		game.draw.Line(p1, p2, color::Gray);
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

struct SweepTest : public Test {
	ecs::Manager manager;

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
		auto& t			   = entity.Add<Transform>();
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

	SweepTest(
		const V2_float& player_vel, const V2_float& player_size = { 50, 50 },
		const V2_float& player_pos = { 0, 0 }, const V2_float& obstacle_size = { 50, 50 },
		const V2_float& fixed_velocity = {}, Origin origin = Origin::Center,
		bool player_is_circle = false
	) :
		player_velocity{ player_vel },
		size{ obstacle_size },
		fixed_velocity{ fixed_velocity },
		player_start_pos{ player_pos } {
		player = AddCollisionObject(player_pos, player_size, player_vel, origin, player_is_circle);
	}

	void Init() override {
		PTGN_ASSERT(player.Has<Transform>());
		auto& t	   = player.Get<Transform>();
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

		manager.Refresh();
	}

	void Update() override {
		auto& rb		= player.Get<RigidBody>();
		auto& transform = player.Get<Transform>();

		for (auto [e, p, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			if (e == player) {
				game.draw.Rect({ p.position, b.size, b.origin }, color::Green);
			} else {
				game.draw.Rect({ p.position, b.size, b.origin }, color::Blue);
			}
		}
		for (auto [e, p, c] : manager.EntitiesWith<Transform, CircleCollider>()) {
			if (e == player) {
				game.draw.Circle(p.position, c.radius, color::Green);
			} else {
				game.draw.Circle(p.position, c.radius, color::Blue);
			}
		}

		if (player.Has<BoxCollider>()) {
			auto& box = player.Get<BoxCollider>();
			game.draw.Rect(
				{ transform.position + rb.velocity * dt, box.size, box.origin }, color::DarkGreen
			);
		} else if (player.Has<CircleCollider>()) {
			auto& circle = player.Get<CircleCollider>();
			game.draw.Circle(
				transform.position + rb.velocity * dt, circle.radius, color::DarkGreen, 1.0f
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
			game.collision.Sweep(player, player.Get<BoxCollider>(), boxes, circles, true);
		} else if (player.Has<CircleCollider>()) {
			game.collision.Sweep(player, player.Get<CircleCollider>(), boxes, circles, true);
		}

		if (game.input.KeyDown(Key::SPACE)) {
			transform.position += rb.velocity * dt;
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
		/*
		V2_int grid_size = game.window.GetSize() / size;

		for (std::size_t i = 0; i < grid_size.x; i++) {
			for (std::size_t j = 0; j < grid_size.y; j++) {
				V2_float pos{ i * size.x, j * size.y };
				game.draw.Rect(pos, size, color::Black, Origin::Center, 1.0f);
			}
		}*/
	}
};

struct RectCollisionTest : public SweepTest {
	RectCollisionTest() :
		SweepTest{ { 100000.0f, 100000.0f }, { 30.0f, 30.0f }, { 45.0f, 84.5f } } {
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
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectCollisionTest1 : public SweepTest {
	RectCollisionTest1() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 45.0f, 84.5f },
				   { 50, 50 },
				   { 100000.0f, 100000.0f } } {
		AddCollisionObject({ 150.0f, 150.0f }, { 75.0f, 20.0f });
		AddCollisionObject({ 50.0f, 130.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
		AddCollisionObject({ 200.0f, 100.0f }, { 20.0f, 60.0f });
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectCollisionTest2 : public SweepTest {
	RectCollisionTest2() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 25.0f, 30.0f },
				   { 50, 50 },
				   { -100000.0f, 100000.0f } } {
		AddCollisionObject({ 20.0f, 90.0f }, { 20.0f, 90.0f });
		AddCollisionObject({ 50.0f, 50.0f }, { 20.0f, 20.0f });
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectCollisionTest3 : public SweepTest {
	RectCollisionTest3() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 175.0f, 75.0f },
				   { 50, 50 },
				   { -100000.0f, 100000.0f } } {
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectCollisionTest4 : public SweepTest {
	RectCollisionTest4() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 97.5000000f, 74.9999924f },
				   { 50, 50 },
				   { 100000.0f, -100000.0f } } {
		AddCollisionObject({ 150.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 110.0f, 50.0f }, { 20.0f, 20.0f });
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct CircleRectCollisionTest1 : public SweepTest {
	CircleRectCollisionTest1() :
		SweepTest{ { 10000.0f, 10000.0f },
				   { 30.0f, 30.0f },
				   { 563.608337f, 623.264038f },
				   { 50.0f, 50.0f },
				   { 0.00000000f, 10000.0f },
				   Origin::Center,
				   true } {
		AddCollisionObject(V2_float{ 50, 650 }, V2_float{ 500, 10 }, {}, Origin::TopLeft);
	}

	void Init() override {
		game.camera.GetPrimary().CenterOnArea({ 800, 800 });
		SweepTest::Init();
	}
};

struct DynamicRectCollisionTest : public Test {
	ecs::Manager manager;

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
		game.window.SetSize({ 800, 800 });
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
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

	void Init() override {
		manager.Clear();
		for (std::size_t i = 0; i < entity_data.size(); ++i) {
			ecs::Entity entity = manager.CreateEntity();
			const auto& data   = entity_data[i];
			auto& t			   = entity.Add<Transform>();
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
		manager.Refresh();
	}

	void Update() override {
		bool space_down = game.input.KeyDown(Key::SPACE);
		for (auto [e, rb, id] : manager.EntitiesWith<RigidBody, Id>()) {
			PTGN_ASSERT(id < entity_data.size());
			rb.velocity = entity_data[id].velocity;
		}

		auto boxes	 = manager.EntitiesWith<BoxCollider>();
		auto circles = manager.EntitiesWith<CircleCollider>();

		for (auto [e, t, b, rb, id, nv] :
			 manager.EntitiesWith<Transform, BoxCollider, RigidBody, Id, NextVel>()) {
			game.collision.Sweep(e, b, boxes, circles, true);
		}

		for (auto [e, t, b, rb, id, nv] :
			 manager.EntitiesWith<Transform, BoxCollider, RigidBody, Id, NextVel>()) {
			if (space_down) {
				t.position += rb.velocity * dt;
			}
			for (auto [e2, t2, b2, rb2] :
				 manager.EntitiesWith<Transform, BoxCollider, RigidBody>()) {
				if (e2 == e) {
					continue;
				}
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
		for (auto [e, t, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			game.draw.Rect({ t.position + b.offset, b.size, b.origin }, color::Green);
		}
	}
};

struct HeadOnDynamicRectTest1 : public DynamicRectCollisionTest {
	HeadOnDynamicRectTest1(float speed) : DynamicRectCollisionTest{ speed } {
		CreateDynamicEntity({ 0, center.y }, { 40.0f, 40.0f }, Origin::CenterLeft, { 1.0f, 0.0f });
		CreateDynamicEntity(
			{ ws.x, center.y }, { 40.0f, 40.0f }, Origin::CenterRight, { -1.0f, 0.0f }
		);
	}
};

struct HeadOnDynamicRectTest2 : public DynamicRectCollisionTest {
	HeadOnDynamicRectTest2(float speed) : DynamicRectCollisionTest{ speed } {
		CreateDynamicEntity({ center.x, 0 }, { 40.0f, 40.0f }, Origin::CenterTop, { 0, 1.0f });
		CreateDynamicEntity(
			{ center.x, ws.y }, { 40.0f, 40.0f }, Origin::CenterBottom, { 0, -1.0f }
		);
		CreateDynamicEntity({ 0, center.y }, { 40.0f, 40.0f }, Origin::CenterLeft, { 1.0f, 0.0f });
		CreateDynamicEntity(
			{ ws.x, center.y }, { 40.0f, 40.0f }, Origin::CenterRight, { -1.0f, 0.0f }
		);
	}
};

struct SweepCornerTest1 : public SweepTest {
	SweepCornerTest1(const V2_float& player_vel) : SweepTest{ player_vel } {
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 300, 250 });
	}
};

struct SweepCornerTest2 : public SweepTest {
	SweepCornerTest2(const V2_float& player_vel) : SweepTest{ player_vel } {
		AddCollisionObject({ 300 - 10, 300 });
		AddCollisionObject({ 250 - 10, 300 });
		AddCollisionObject({ 300 - 10, 250 });
	}
};

struct SweepCornerTest3 : public SweepTest {
	SweepCornerTest3(const V2_float& player_vel) : SweepTest{ player_vel } {
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 250, 250 });
	}
};

struct SweepTunnelTest1 : public SweepTest {
	SweepTunnelTest1(const V2_float& player_vel) : SweepTest{ player_vel } {
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 300, 250 });
		AddCollisionObject({ 200, 350 });
		AddCollisionObject({ 300, 350 });
		AddCollisionObject({ 250, 400 });
		AddCollisionObject({ 200, 400 });
		AddCollisionObject({ 300, 400 });
	}
};

struct SweepTunnelTest2 : public SweepTest {
	SweepTunnelTest2(const V2_float& player_vel) : SweepTest{ player_vel } {
		AddCollisionObject({ 300, 300 });
		AddCollisionObject({ 300, 200 });
		AddCollisionObject({ 200, 300 });
		AddCollisionObject({ 250, 300 });
		AddCollisionObject({ 350, 300 });
		AddCollisionObject({ 350, 200 });
		AddCollisionObject({ 400, 200 });
		AddCollisionObject({ 400, 250 });
		AddCollisionObject({ 400, 300 });
	}
};

void TestCollisions() {
	std::vector<std::shared_ptr<Test>> tests;

	V2_float velocity{ 100000.0f };
	float speed{ 7000.0f };

	tests.emplace_back(new PointOverlapTest());
	tests.emplace_back(new LineOverlapTest());
	tests.emplace_back(new CircleOverlapTest());
	tests.emplace_back(new RectOverlapTest());
	tests.emplace_back(new CapsuleOverlapTest());
	tests.emplace_back(new CircleRectCollisionTest1());
	tests.emplace_back(new CollisionCallbackTest());
	// tests.emplace_back(new CollisionTest2());
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

	AddTests(tests);
}