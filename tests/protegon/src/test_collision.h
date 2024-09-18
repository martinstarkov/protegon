#include <memory>
#include <new>
#include <vector>

#include "common.h"
#include "components/collider.h"
#include "components/rigid_body.h"
#include "components/transform.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "protegon/circle.h"
#include "protegon/collision.h"
#include "protegon/color.h"
#include "protegon/game.h"
#include "protegon/line.h"
#include "protegon/log.h"
#include "protegon/polygon.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"

class CollisionTest : public Test {
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
	int option{ 0 };

	int type{ 2 };
	int types{ 3 };

	void Init() override {
		game.window.SetTitle("'ESC' (++category), 't' (++shape), 'g' (++mode), 'r' (reset pos)");
	}

	void Update(float dt) final {
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

		V2_float position2 = mouse;

		auto acolor1 = color1;
		auto acolor2 = color2;

		Rectangle<float> aabb1{ position1, size1, Origin::Center };
		Rectangle<float> aabb2{ position2, size2, Origin::Center };

		Circle<float> circle1{ position1, radius1 };
		Circle<float> circle2{ position2, radius2 };

		Segment<float> line1{ position1, position3 };
		Segment<float> line2{ position2, position4 };

		if (type == 0) { // overlap
			options = 9;
			if (option == 0) {
				if (game.collision.overlap.PointSegment(position2, line1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawLine(line1, acolor1);
				game.renderer.DrawPoint(position2, acolor2);
			} else if (option == 1) {
				if (game.collision.overlap.PointCircle(position2, circle1)) {
					bool test = game.collision.overlap.PointCircle(position2, circle1);
					acolor1	  = color::Red;
					acolor2	  = color::Red;
				}
				game.renderer.DrawCircleHollow(circle1, acolor1);
				game.renderer.DrawPoint(position2, acolor2);
			} else if (option == 2) {
				if (game.collision.overlap.PointRectangle(position2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
				game.renderer.DrawPoint(position2, acolor2);
			} else if (option == 3) {
				if (game.collision.overlap.SegmentSegment(line2, line1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawLine(line1, acolor1);
				game.renderer.DrawLine(line2, acolor2);
			} else if (option == 4) {
				if (game.collision.overlap.SegmentCircle(line2, circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawLine(line2, acolor2);
				game.renderer.DrawCircleHollow(circle1, acolor1);
			} else if (option == 5) {
				if (game.collision.overlap.SegmentRectangle(line2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawLine(line2, acolor2);
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
			} else if (option == 6) {
				if (game.collision.overlap.CircleCircle(circle2, circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawCircleHollow(circle2, acolor2);
				game.renderer.DrawCircleHollow(circle1, acolor1);
			} else if (option == 7) {
				if (game.collision.overlap.CircleRectangle(circle2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
				game.renderer.DrawCircleHollow(circle2, acolor2);
			} else if (option == 8) {
				aabb2.pos = mouse;
				if (game.collision.overlap.RectangleRectangle(aabb1, aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb2, acolor2);
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
			}
		} else if (type == 1) { // intersect
			options = 3;
			const float slop{ 0.005f };
			IntersectCollision c;
			if (option == 0) {
				// circle2.center = circle1.center;
				bool occured{ game.collision.intersect.CircleCircle(circle2, circle1, c) };
				if (occured) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawCircleHollow(circle2, acolor2);
				game.renderer.DrawCircleHollow(circle1, acolor1);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop),
											  circle2.radius };
					game.renderer.DrawCircleHollow(new_circle, color2);
					Segment<float> l{ circle2.center, new_circle.center };
					game.renderer.DrawLine(l, color::Gold);
					if (game.collision.overlap.CircleCircle(new_circle, circle1)) {
						occured = game.collision.intersect.CircleCircle(new_circle, circle1, c);
						bool overlap{ game.collision.overlap.CircleCircle(new_circle, circle1) };
						if (overlap) {
							PrintLine("Slop insufficient, overlap reoccurs");
						}
						if (occured) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			} else if (option == 1) {
				// circle2.center = aabb1.position;
				// circle2.center = aabb1.Center();
				bool occured{ game.collision.intersect.CircleRectangle(circle2, aabb1, c) };
				if (occured) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
				game.renderer.DrawCircleHollow(circle2, acolor2);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop),
											  circle2.radius };
					game.renderer.DrawCircleHollow(new_circle, color2);
					Segment<float> l{ circle2.center, new_circle.center };
					game.renderer.DrawLine(l, color::Gold);
					if (game.collision.overlap.CircleRectangle(new_circle, aabb1)) {
						occured = game.collision.intersect.CircleRectangle(new_circle, aabb1, c);
						bool overlap{ game.collision.overlap.CircleRectangle(new_circle, aabb1) };
						if (overlap) {
							PrintLine("Slop insufficient, overlap reoccurs");
						}
						if (occured) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			} else if (option == 2) {
				aabb2.pos = mouse;
				// aabb2.position = aabb1.Center() - aabb2.Half();
				bool occured{ game.collision.intersect.RectangleRectangle(aabb2, aabb1, c) };
				if (occured) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb2, acolor2);
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
				if (occured) {
					Rectangle<float> new_aabb{ aabb2.pos + c.normal * (c.depth + slop), aabb2.size,
											   aabb2.origin };
					game.renderer.DrawRectangleHollow(new_aabb, color2);
					Segment<float> l{ aabb2.Center(), new_aabb.Center() };
					game.renderer.DrawLine(l, color::Gold);
					if (game.collision.overlap.RectangleRectangle(new_aabb, aabb1)) {
						occured = game.collision.intersect.RectangleRectangle(new_aabb, aabb1, c);
						bool overlap{ game.collision.overlap.RectangleRectangle(new_aabb, aabb1) };
						if (overlap) {
							PrintLine("Slop insufficient, overlap reoccurs");
						}
						if (occured) {
							PrintLine("Slop insufficient, intersect reoccurs");
						}
					}
				}
			}
		} else if (type == 2) { // dynamic
			options = 4;
			const float slop{ 0.005f };
			DynamicCollision c;
			if (option == 0) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle<float> potential{ circle2.center + vel, circle2.radius };
				game.renderer.DrawCircleHollow(potential, color::Grey);
				Segment<float> l{ circle2.center, potential.center };
				game.renderer.DrawLine(l, color::Grey);
				bool occured{ game.collision.dynamic.CircleRectangle(circle2, vel, aabb1, c) };
				if (occured) {
					Circle<float> swept{ circle2.center + vel * c.t, circle2.radius };
					Segment<float> normal{ swept.center, swept.center + 50 * c.normal };
					game.renderer.DrawLine(normal, color::Orange);
					game.renderer.DrawCircleHollow(swept, color::Green);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawCircleHollow(circle2, acolor1);
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
			} else if (option == 1) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle<float> potential{ circle2.center + vel, circle2.radius };
				game.renderer.DrawCircleHollow(potential, color::Grey);
				Segment<float> l{ circle2.center, potential.center };
				game.renderer.DrawLine(l, color::Grey);
				bool occured{ game.collision.dynamic.CircleCircle(circle2, vel, circle1, c) };
				if (occured) {
					Circle<float> swept{ circle2.center + vel * c.t, circle2.radius };
					Segment<float> normal{ swept.center, swept.center + 50 * c.normal };
					game.renderer.DrawLine(normal, color::Orange);
					game.renderer.DrawCircleHollow(swept, color::Green);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawCircleHollow(circle2, acolor1);
				game.renderer.DrawCircleHollow(circle1, acolor1);
			} else if (option == 2) {
				V2_float pos = position4;
				V2_float vel{ mouse - pos };
				Segment<float> l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				game.renderer.DrawCircleFilled(pos + vel, point_radius, color::Grey);
				game.renderer.DrawLine(l, color::Grey);
				bool occured = game.collision.dynamic.SegmentRectangle(
					l, { aabb1.Min(), aabb1.size, Origin::TopLeft }, c
				);

				if (occured) {
					V2_float point{ pos + vel * c.t };
					Segment<float> normal{ point, point + 50 * c.normal };
					game.renderer.DrawLine(normal, color::Orange);
					game.renderer.DrawCircleFilled(point, point_radius, color::Green);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
			} else if (option == 3) {
				aabb2.pos = position4;
				V2_float vel{ mouse - aabb2.pos };
				Rectangle<float> potential{ aabb2.pos + vel, aabb2.size, aabb2.origin };
				game.renderer.DrawRectangleHollow(potential, color::Grey);
				Segment<float> l{ aabb2.Center(), potential.Center() };
				game.renderer.DrawLine(l, color::Grey);
				bool occured{ game.collision.dynamic.RectangleRectangle(aabb2, vel, aabb1, c) };
				if (occured) {
					Rectangle<float> swept{ aabb2.pos + vel * c.t, aabb2.size, aabb2.origin };
					Segment<float> normal{ swept.Center(), swept.Center() + 50 * c.normal };
					game.renderer.DrawLine(normal, color::Orange);
					game.renderer.DrawRectangleHollow(swept, color::Green);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.renderer.DrawRectangleHollow(aabb2, acolor1);
				game.renderer.DrawRectangleHollow(aabb1, acolor1);
			}

			/*
			if (option == 0) {
			  // circle2.center = circle1.center;
			  int occured{game.collision.dynamic.SegmentCircle(line2, circle1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.renderer.DrawLine(line2, acolor2);
			  game.renderer.DrawCircleHollow(circle2, acolor2);
			  game.renderer.DrawCircleHollow(circle1, acolor1);
			  if (occured) {
				Circle<float> new_circle{circle2.center + line2.Direction() * c.t,
										 circle2.radius};
				game.renderer.DrawCircleHollow(new_circle,
											   acolor2);
			  }
			}
			if (option == 0) {
			  // circle2.center = circle1.center;
			  bool occured{game.collision.dynamic.CircleCircle(circle2, circle1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.renderer.DrawCircleHollow(circle2, acolor2);
			  game.renderer.DrawCircleHollow(circle1, acolor1);
			  if (occured) {
				Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
										 circle2.radius};
				game.renderer.DrawCircleHollow(new_circle,
											   color2);
				Segment<float> l{circle2.center, new_circle.center};
				game.renderer.DrawLine(l, color::Gold);
				if (game.collision.overlap.CircleCircle(new_circle, circle1)) {
				  occured = game.collision.intersect.CircleCircle(new_circle, circle1, c);
				  bool overlap{game.collision.overlap.CircleCircle(new_circle, circle1)};
				  if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
				  if (occured) PrintLine("Slop insufficient, intersect reoccurs");
				}
			  }
			} else if (option == 1) {
			  // circle2.center = aabb1.position;
			  // circle2.center = aabb1.Center();
			  bool occured{game.collision.intersect.CircleRectangle(circle2, aabb1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.renderer.DrawRectangleHollow(aabb1, acolor1);
			  game.renderer.DrawCircleHollow(circle2, acolor2);
			  if (occured) {
				Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
										 circle2.radius};
				game.renderer.DrawCircleHollow(new_circle,
											   color2);
				Segment<float> l{circle2.center, new_circle.center};
				game.renderer.DrawLine(l, color::Gold);
				if (game.collision.overlap.CircleRectangle(new_circle, aabb1)) {
				  occured = game.collision.intersect.CircleRectangle(new_circle, aabb1, c);
				  bool overlap{game.collision.overlap.CircleRectangle(new_circle, aabb1)};
				  if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
				  if (occured) PrintLine("Slop insufficient, intersect reoccurs");
				}
			  }
			} else if (option == 2) {
			  aabb2.pos = mouse - aabb2.Half();
			  // aabb2.position = aabb1.Center() - aabb2.Half();
			  bool occured{game.collision.intersect.RectangleRectangle(aabb2, aabb1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.renderer.DrawRectangleHollow(aabb2, acolor2);
			  game.renderer.DrawRectangleHollow(aabb1, acolor1);
			  if (occured) {
				Rectangle<float> new_aabb{aabb2.pos + c.normal * (c.depth + slop),
										  aabb2.size, Origin::TopLeft};
				game.renderer.DrawRectangleHollow(new_aabb,
			color2); Segment<float> l{aabb2.Center(), new_aabb.Center()};
				game.renderer.DrawLine(l, color::Gold);
				if (game.collision.overlap.RectangleRectangle(new_aabb, aabb1)) {
				  occured = game.collision.intersect.RectangleRectangle(new_aabb, aabb1, c);
				  bool overlap{game.collision.overlap.RectangleRectangle(new_aabb, aabb1)};
				  if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
				  if (occured) PrintLine("Slop insufficient, intersect reoccurs");
				}
			  }
			}*/
		}
	}
};

struct SweepTest : public Test {
	ecs::Manager manager;

	ecs::Entity player;

	V2_float player_size{ 50, 50 };
	V2_float size{ 50, 50 };

	void Init() override {
		auto add_collision_object = [&](const V2_float& p, const V2_float& s,
										const V2_float& v = {}) {
			ecs::Entity entity = manager.CreateEntity();
			auto& t			   = entity.Add<Transform>();
			t.position		   = p;

			auto& box = entity.Add<BoxCollider>();
			box.size  = s;

			if (!v.IsZero()) {
				auto& rb	= entity.Add<RigidBody>();
				rb.velocity = v;
			}
			return entity;
		};

		V2_float vel{ 1, 1 };

		player = add_collision_object({ 100, 100 }, player_size, vel);

		add_collision_object({ 300, 300 }, size, { 0, 0 });
		add_collision_object({ 250, 300 }, size, { 0, 0 });
		add_collision_object({ 300, 250 }, size, { 0, 0 });

		manager.Refresh();
	}

	float speed{ 1200.0f };

	void Update(float dt) override {
		auto& rb		= player.Get<RigidBody>();
		auto& transform = player.Get<Transform>();

		if (game.input.KeyPressed(Key::A)) {
			rb.velocity.x -= speed * dt;
		}
		if (game.input.KeyPressed(Key::D)) {
			rb.velocity.x += speed * dt;
		}
		if (game.input.KeyPressed(Key::W)) {
			rb.velocity.y -= speed * dt;
		}
		if (game.input.KeyPressed(Key::S)) {
			rb.velocity.y += speed * dt;
		}

		rb.velocity =
			game.collision.dynamic.Sweep(dt, player, manager, DynamicCollisionResponse::Slide);

		transform.position += rb.velocity * dt;
	}

	void Draw() override {
		for (auto [e, p, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			game.renderer.DrawRectangleHollow(p.position, b.size, color::Blue);
		}
	}
};

void TestCollisions() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new SweepTest());
	tests.emplace_back(new CollisionTest());

	AddTests(tests);
}