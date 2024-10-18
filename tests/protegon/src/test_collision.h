#include <array>
#include <cmath>
#include <memory>
#include <new>
#include <utility>
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

	const float line_thickness{ 3.0f };

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
				game.draw.Line(line1.a, line1.b, acolor1);
				game.draw.Point(position2, acolor2);
			} else if (option == 1) {
				if (game.collision.overlap.PointCircle(position2, circle1)) {
					bool test = game.collision.overlap.PointCircle(position2, circle1);
					acolor1	  = color::Red;
					acolor2	  = color::Red;
				}
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
				game.draw.Point(position2, acolor2);
			} else if (option == 2) {
				if (game.collision.overlap.PointRectangle(position2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Point(position2, acolor2);
			} else if (option == 3) {
				if (game.collision.overlap.SegmentSegment(line2, line1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line1.a, line1.b, acolor1);
				game.draw.Line(line2.a, line2.b, acolor2);
			} else if (option == 4) {
				if (game.collision.overlap.SegmentCircle(line2, circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line2.a, line2.b, acolor2);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			} else if (option == 5) {
				if (game.collision.overlap.SegmentRectangle(line2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Line(line2.a, line2.b, acolor2);
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 6) {
				if (game.collision.overlap.CircleCircle(circle2, circle1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			} else if (option == 7) {
				if (game.collision.overlap.CircleRectangle(circle2, aabb1)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
			} else if (option == 8) {
				aabb2.pos = mouse;
				if (game.collision.overlap.RectangleRectangle(aabb1, aabb2)) {
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rectangle(aabb2.pos, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
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
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop),
											  circle2.radius };
					game.draw.Circle(new_circle.center, new_circle.radius, color2, line_thickness);
					Segment<float> l{ circle2.center, new_circle.center };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop),
											  circle2.radius };
					game.draw.Circle(new_circle.center, new_circle.radius, color2, line_thickness);
					Segment<float> l{ circle2.center, new_circle.center };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Rectangle(aabb2.pos, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
				if (occured) {
					Rectangle<float> new_aabb{ aabb2.pos + c.normal * (c.depth + slop), aabb2.size,
											   aabb2.origin };
					game.draw.Rectangle(
						new_aabb.pos, new_aabb.size, color2, new_aabb.origin, line_thickness
					);
					Segment<float> l{ aabb2.Center(), new_aabb.Center() };
					game.draw.Line(l.a, l.b, color::Gold);
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
				game.draw.Circle(potential.center, potential.radius, color::Grey, line_thickness);
				Segment<float> l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Grey);
				bool occured{ game.collision.dynamic.CircleRectangle(circle2, vel, aabb1, c) };
				if (occured) {
					Circle<float> swept{ circle2.center + vel * c.t, circle2.radius };
					Segment<float> normal{ swept.center, swept.center + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(swept.center, swept.radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 1) {
				circle2.center = position4;
				V2_float vel{ mouse - circle2.center };
				Circle<float> potential{ circle2.center + vel, circle2.radius };
				game.draw.Circle(potential.center, potential.radius, color::Grey, line_thickness);
				Segment<float> l{ circle2.center, potential.center };
				game.draw.Line(l.a, l.b, color::Grey);
				bool occured{ game.collision.dynamic.CircleCircle(circle2, vel, circle1, c) };
				if (occured) {
					Circle<float> swept{ circle2.center + vel * c.t, circle2.radius };
					Segment<float> normal{ swept.center, swept.center + 50 * c.normal };
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
				Segment<float> l{ pos, pos + vel };
				const float point_radius{ 5.0f };
				game.draw.Circle(pos + vel, point_radius, color::Grey, line_thickness);
				game.draw.Line(l.a, l.b, color::Grey);
				bool occured = game.collision.dynamic.SegmentRectangle(
					l, { aabb1.Min(), aabb1.size, Origin::TopLeft }, c
				);
				if (occured) {
					V2_float point{ pos + vel * c.t };
					Segment<float> normal{ point, point + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Circle(point, point_radius, color::Green, line_thickness);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			} else if (option == 3) {
				aabb2.pos = position4;
				V2_float vel{ mouse - aabb2.pos };
				Rectangle<float> potential{ aabb2.pos + vel, aabb2.size, aabb2.origin };
				game.draw.Rectangle(
					potential.pos, potential.size, color::Grey, potential.origin, line_thickness
				);
				Segment<float> l{ aabb2.Center(), potential.Center() };
				game.draw.Line(l.a, l.b, color::Grey);
				bool occured{ game.collision.dynamic.RectangleRectangle(aabb2, vel, aabb1, c) };
				if (occured) {
					Rectangle<float> swept{ aabb2.pos + vel * c.t, aabb2.size, aabb2.origin };
					Segment<float> normal{ swept.Center(), swept.Center() + 50 * c.normal };
					game.draw.Line(normal.a, normal.b, color::Orange);
					game.draw.Rectangle(
						swept.pos, swept.size, color::Green, swept.origin, line_thickness
					);
					acolor1 = color::Red;
					acolor2 = color::Red;
				}
				game.draw.Rectangle(aabb2.pos, aabb2.size, acolor2, aabb2.origin, line_thickness);
				game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			}

			/*
			if (option == 0) {
			  // circle2.center = circle1.center;
			  int occured{game.collision.dynamic.SegmentCircle(line2, circle1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.draw.Line(l.a, l.bine2, acolor2);
			  game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
			  game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			  if (occured) {
				Circle<float> new_circle{circle2.center + line2.Direction() * c.t,
										 circle2.radius};
				game.draw.Circle(new_circle.center, new_circle.radius,
											   acolor2, line_thickness);
			  }
			}
			if (option == 0) {
			  // circle2.center = circle1.center;
			  bool occured{game.collision.dynamic.CircleCircle(circle2, circle1, c)};
			  if (occured) {
				acolor1 = color::Red;
				acolor2 = color::Red;
			  }
			  game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
			  game.draw.Circle(circle1.center, circle1.radius, acolor1, line_thickness);
			  if (occured) {
				Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
										 circle2.radius};
				game.draw.Circle(new_circle.center, new_circle.radius,
											   color2, line_thickness);
				Segment<float> l{circle2.center, new_circle.center};
				game.draw.Line(l.a, l.b, color::Gold);
				if (game.collision.overlap.CircleCircle(new_circle.center, new_circle.radius,
			circle1)) { occured = game.collision.intersect.CircleCircle(new_circle.center,
			new_circle.radius, circle1, c); bool
			overlap{game.collision.overlap.CircleCircle(new_circle.center, new_circle.radius,
			circle1)}; if (overlap) PrintLine("Slop insufficient, overlap reoccurs"); if (occured)
			PrintLine("Slop insufficient, intersect reoccurs");
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
			  game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			  game.draw.Circle(circle2.center, circle2.radius, acolor2, line_thickness);
			  if (occured) {
				Circle<float> new_circle{circle2.center + c.normal * (c.depth + slop),
										 circle2.radius};
				game.draw.Circle(new_circle.center, new_circle.radius,
											   color2);
				Segment<float> l{circle2.center, new_circle.center};
				game.draw.Line(l.a, l.b, color::Gold);
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
			  game.draw.Rectangle(aabb2.pos, aabb2.size, acolor2, aabb2.origin, line_thickness);
			  game.draw.Rectangle(aabb1.pos, aabb1.size, acolor1, aabb1.origin, line_thickness);
			  if (occured) {
				Rectangle<float> new_aabb{aabb2.pos + c.normal * (c.depth + slop),
										  aabb2.size, Origin::TopLeft};
				game.draw.Rectangle(new_aabb.pos, new_aabb.size,
			color2, new_aabb.origin, line_thickness); Segment<float> l{aabb2.Center(),
			new_aabb.Center()}; game.draw.Line(l.a, l.b, color::Gold); if
			(game.collision.overlap.RectangleRectangle(new_aabb, aabb1)) { occured =
			game.collision.intersect.RectangleRectangle(new_aabb, aabb1, c); bool
			overlap{game.collision.overlap.RectangleRectangle(new_aabb, aabb1)}; if (overlap)
			PrintLine("Slop insufficient, overlap reoccurs"); if (occured) PrintLine("Slop
			insufficient, intersect reoccurs");
				}
			  }
			}*/
		}
	}
};

struct SegmentRectangleOverlapTest : public Test {
	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rectangle<float> aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };

	void LineOverlap(V2_float p1, V2_float p2, Color color) {
		Segment<float> l1{ p1, p2 };
		Color c;
		bool occured{ game.collision.overlap.SegmentRectangle(l1, aabb) };
		if (occured) {
			c = color;
		} else {
			c = color::Grey;
		}
		game.draw.Line(l1.a, l1.b, c);
	}

	void Update() {
		game.draw.Rectangle(aabb.pos, aabb.size, color::Cyan, aabb.origin, 1.0f);

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

struct SegmentRectangleDynamicTest : public Test {
	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rectangle<float> aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };

	void LineSweep(V2_float p1, V2_float p2, Color color) {
		Segment<float> l1{ p1, p2 };
		game.draw.Line(l1.a, l1.b, color::Grey);
		DynamicCollision c;
		bool occured{ game.collision.dynamic.SegmentRectangle(l1, aabb, c) };
		if (occured) {
			V2_float point = l1.a + c.t * l1.Direction();
			game.draw.Point(point, color, 2.0f);
		}
	}

	void Update() {
		game.draw.Rectangle(aabb.pos, aabb.size, color::Cyan, aabb.origin, 1.0f);

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

struct RectangleRectangleDynamicTest : public Test {
	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 200.0f, 200.0f });
	}

	Rectangle<float> aabb{ { 60.0f, 30.0f }, { 30.0f, 30.0f }, Origin::TopLeft };
	Rectangle<float> target{ {}, { 10.0f, 10.0f }, Origin::Center };

	void RectangleSweep(V2_float p1, V2_float p2, Color color, bool debug_flag = false) {
		target.pos = p1;
		game.draw.Rectangle(target.pos, target.size, color::Grey, target.origin, 1.0f);
		game.draw.Line(p1, p2, color::Grey);
		DynamicCollision c;
		V2_float vel = p2 - p1;
		bool occured{ game.collision.dynamic.RectangleRectangle(target, vel, aabb, c) };
		if (occured) {
			Rectangle<float> new_rect = target;
			new_rect.pos			  = p1 + c.t * vel;
			game.draw.Rectangle(new_rect.pos, new_rect.size, color, new_rect.origin, 1.0f);
			if (game.collision.overlap.RectangleRectangle(new_rect, aabb)) {
				PTGN_LOG("still overlapping");
			}
		} else {
			Rectangle<float> new_rect = target;
			new_rect.pos			  = p1 + vel;
			game.draw.Rectangle(new_rect.pos, new_rect.size, color::Grey, new_rect.origin, 1.0f);
		}
		if (debug_flag) {
			PTGN_LOG("t: ", c.t);
		}
	}

	void Update() {
		game.draw.Rectangle(aabb.pos, aabb.size, color::Cyan, aabb.origin, 1.0f);

		// Rectangles which are inside the rectangle.

		RectangleSweep({ 40.0f, 10.0f }, { 70.0f, 40.0f }, color::Green);  // top left corner
		RectangleSweep({ 110.0f, 10.0f }, { 80.0f, 40.0f }, color::Green); // top right corner
		RectangleSweep({ 40.0f, 80.0f }, { 70.0f, 50.0f }, color::Green);  // bottom left corner
		RectangleSweep({ 110.0f, 80.0f }, { 80.0f, 50.0f }, color::Green); // bottom right corner
		RectangleSweep({ 30.0f, 31.0f }, { 70.0f, 31.0f }, color::Green);  // top left to right
		RectangleSweep({ 30.0f, 59.0f }, { 70.0f, 59.0f }, color::Green);  // bottom left to right
		RectangleSweep({ 120.0f, 31.0f }, { 80.0f, 31.0f }, color::Green); // top right to left
		RectangleSweep({ 120.0f, 59.0f }, { 80.0f, 59.0f }, color::Green); // bottom right to left
		RectangleSweep({ 61.0f, 10.0f }, { 61.0f, 40.0f }, color::Green);  // top left to bottom
		RectangleSweep({ 61.0f, 80.0f }, { 61.0f, 50.0f }, color::Green);  // bottom left to top
		RectangleSweep({ 89.0f, 10.0f }, { 89.0f, 40.0f }, color::Green);  // top right to bottom
		RectangleSweep({ 89.0f, 80.0f }, { 89.0f, 50.0f }, color::Green);  // bottom right to top

		// RectangleSweeps which overlap the edges of the rectangle.

		RectangleSweep(
			{ 40.0f, 10.0f }, V2_float{ 60.0f, 30.0f } - target.Half(), color::Red
		); // top left corner - overlapping
		RectangleSweep(
			{ 110.0f, 10.0f },
			V2_float{ 90.0f, 30.0f } + V2_float{ target.Half().x, -target.Half().y }, color::Red
		); // top right corner - overlapping
		RectangleSweep(
			{ 40.0f, 80.0f },
			V2_float{ 60.0f, 60.0f } + V2_float{ -target.Half().x, target.Half().y }, color::Red
		); // bottom left corner - overlapping
		RectangleSweep(
			{ 110.0f, 80.0f }, V2_float{ 90.0f, 60.0f } + target.Half(), color::Red
		); // bottom right corner - overlapping
		RectangleSweep(
			{ 30.0f, 30.0f - target.Half().y }, V2_float{ 70.0f, 30.0f - target.Half().y },
			color::Red
		); // top left to right - overlapping
		RectangleSweep(
			{ 30.0f, 60.0f + target.Half().y }, V2_float{ 70.0f, 60.0f + target.Half().y },
			color::Red
		); // bottom left to right - overlapping
		RectangleSweep(
			{ 120.0f, 30.0f - target.Half().y }, V2_float{ 80.0f, 30.0f - target.Half().y },
			color::Red
		); // top right to left - overlapping
		RectangleSweep(
			{ 120.0f, 60.0f + target.Half().y }, V2_float{ 80.0f, 60.0f + target.Half().y },
			color::Red
		); // bottom right to left - overlapping
		RectangleSweep(
			{ 60.0f - target.Half().x, 10.0f }, V2_float{ 60.0f - target.Half().x, 40.0f },
			color::Red
		); // top left to bottom - overlapping
		RectangleSweep(
			{ 60.0f - target.Half().x, 80.0f }, V2_float{ 60.0f - target.Half().x, 50.0f },
			color::Red
		); // bottom left to top - overlapping
		RectangleSweep(
			{ 90.0f + target.Half().x, 10.0f }, V2_float{ 90.0f + target.Half().x, 40.0f },
			color::Red
		); // top right to bottom - overlapping
		RectangleSweep(
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

	ecs::Entity AddCollisionObject(
		const V2_float& p, const V2_float& s = {}, const V2_float& v = {}
	) {
		ecs::Entity entity = manager.CreateEntity();
		auto& t			   = entity.Add<Transform>();
		t.position		   = p;

		auto& box = entity.Add<BoxCollider>();
		if (s.IsZero()) {
			box.size = size;
		} else {
			box.size = s;
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
		const V2_float& fixed_velocity = {}
	) :
		player_velocity{ player_vel },
		size{ obstacle_size },
		fixed_velocity{ fixed_velocity },
		player_start_pos{ player_pos } {
		player = AddCollisionObject(player_pos, player_size, player_vel);
	}

	void Init() override {
		PTGN_ASSERT(player.Has<Transform>());
		auto& t	   = player.Get<Transform>();
		t.position = player_start_pos;
		manager.Refresh();
	}

	void Update() override {
		auto& rb		= player.Get<RigidBody>();
		auto& transform = player.Get<Transform>();
		auto& box		= player.Get<BoxCollider>();

		for (auto [e, p, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			if (e == player) {
				PTGN_LOG_PRECISE(7, false, p.position);
				game.draw.Rectangle(p.position, b.size, color::Green, Origin::Center, 1.0f);
			} else {
				game.draw.Rectangle(p.position, b.size, color::Blue, Origin::Center, 1.0f);
			}
		}
		game.draw.Rectangle(
			transform.position + rb.velocity * dt, box.size, color::DarkGreen, Origin::Center, 1.0f
		);

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

		rb.velocity =
			game.collision.dynamic.Sweep(player, manager, DynamicCollisionResponse::Slide, true);

		if (game.input.KeyDown(Key::SPACE)) {
			transform.position += rb.velocity * dt;
		}

		const auto edge_exclusive_overlap = [](const Rectangle<float>& a,
											   const Rectangle<float>& b) {
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

		for (auto [e, p, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			if (e == player) {
				continue;
			} else {
				if (edge_exclusive_overlap(
						{ player.Get<Transform>().position, player.Get<BoxCollider>().size,
						  player.Get<BoxCollider>().origin },
						{ p.position, b.size, b.origin }
					)) {
					// PTGN_LOG("Dynamic sweep collision resolution failed");
				}
			}
		}

		/*PTGN_ASSERT(transform.position.x >= 0.0f);
		PTGN_ASSERT(transform.position.y >= 0.0f);
		PTGN_ASSERT(transform.position.x <= game.window.GetSize().x);
		PTGN_ASSERT(transform.position.y <= game.window.GetSize().y);*/

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
				game.draw.Rectangle(pos, size, color::Black, Origin::Center, 1.0f);
			}
		}*/
	}
};

struct RectangleCollisionTest : public SweepTest {
	RectangleCollisionTest() :
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

	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectangleCollisionTest1 : public SweepTest {
	RectangleCollisionTest1() :
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

	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectangleCollisionTest2 : public SweepTest {
	RectangleCollisionTest2() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 25.0f, 30.0f },
				   { 50, 50 },
				   { -100000.0f, 100000.0f } } {
		AddCollisionObject({ 20.0f, 90.0f }, { 20.0f, 90.0f });
		AddCollisionObject({ 50.0f, 50.0f }, { 20.0f, 20.0f });
	}

	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectangleCollisionTest3 : public SweepTest {
	RectangleCollisionTest3() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 175.0f, 75.0f },
				   { 50, 50 },
				   { -100000.0f, 100000.0f } } {
		AddCollisionObject({ 150.0f, 100.0f }, { 10.0f, 1.0f });
	}

	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct RectangleCollisionTest4 : public SweepTest {
	RectangleCollisionTest4() :
		SweepTest{ { 100000.0f, 100000.0f },
				   { 30.0f, 30.0f },
				   { 97.5000000f, 74.9999924f },
				   { 50, 50 },
				   { 100000.0f, -100000.0f } } {
		AddCollisionObject({ 150.0f, 50.0f }, { 20.0f, 20.0f });
		AddCollisionObject({ 110.0f, 50.0f }, { 20.0f, 20.0f });
	}

	void Init() {
		game.camera.GetPrimary().CenterOnArea({ 256, 240 });
		SweepTest::Init();
	}
};

struct DynamicRectangleCollisionTest : public Test {
	ecs::Manager manager;

	float speed{ 0.0f };

	struct DynamicData {
		V2_float pos;
		V2_float size;
		Origin origin;
		V2_float velocity;
	};

	std::vector<DynamicData> entity_data;

	using Id = std::size_t;

	DynamicRectangleCollisionTest(float speed) : speed{ speed } {
		game.window.SetSize({ 800, 800 });
		ws	   = game.window.GetSize();
		center = game.window.GetCenter();
	}

	void CreateDynamicEntity(
		const V2_float& pos, const V2_float& size, Origin origin, const V2_float& velocity_direction
	) {
		DynamicData data;
		data.pos	  = pos;
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
			auto& data		   = entity_data[i];
			auto& t			   = entity.Add<Transform>();
			t.position		   = data.pos;

			auto& box  = entity.Add<BoxCollider>();
			box.size   = data.size;
			box.origin = data.origin;

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
		for (auto [e, t, b, rb, id, nv] :
			 manager.EntitiesWith<Transform, BoxCollider, RigidBody, Id, NextVel>()) {
			nv = game.collision.dynamic.Sweep(e, manager, DynamicCollisionResponse::Slide, true);
		}

		for (auto [e, t, b, rb, id, nv] :
			 manager.EntitiesWith<Transform, BoxCollider, RigidBody, Id, NextVel>()) {
			rb.velocity = nv;
			if (space_down) {
				t.position += rb.velocity * dt;
			}
			for (auto [e2, t2, b2, rb2] :
				 manager.EntitiesWith<Transform, BoxCollider, RigidBody>()) {
				if (e2 == e) {
					continue;
				}
				Rectangle<float> r1{ t.position + b.offset, b.size, b.origin };
				Rectangle<float> r2{ t2.position + b2.offset, b2.size, b2.origin };
				IntersectCollision c;
				if (game.collision.intersect.RectangleRectangle(r1, r2, c)) {
					t.position += c.normal * c.depth;
				}
				if (game.collision.overlap.RectangleRectangle(r1, r2)) {
					PTGN_LOG("Failed to resolve dynamic collision");
				}
			}
		}
	}

	void Draw() override {
		for (auto [e, t, b] : manager.EntitiesWith<Transform, BoxCollider>()) {
			game.draw.Rectangle(t.position + b.offset, b.size, color::Green, b.origin, 1.0f);
		}
	}
};

struct HeadOnDynamicRectangleTest1 : public DynamicRectangleCollisionTest {
	HeadOnDynamicRectangleTest1(float speed) : DynamicRectangleCollisionTest{ speed } {
		CreateDynamicEntity({ 0, center.y }, { 40.0f, 40.0f }, Origin::CenterLeft, { 1.0f, 0.0f });
		CreateDynamicEntity(
			{ ws.x, center.y }, { 40.0f, 40.0f }, Origin::CenterRight, { -1.0f, 0.0f }
		);
	}
};

struct HeadOnDynamicRectangleTest2 : public DynamicRectangleCollisionTest {
	HeadOnDynamicRectangleTest2(float speed) : DynamicRectangleCollisionTest{ speed } {
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

	tests.emplace_back(new RectangleCollisionTest4());
	tests.emplace_back(new RectangleCollisionTest3());
	tests.emplace_back(new HeadOnDynamicRectangleTest1(speed));
	tests.emplace_back(new HeadOnDynamicRectangleTest2(speed));
	tests.emplace_back(new RectangleCollisionTest());
	tests.emplace_back(new RectangleCollisionTest1());
	tests.emplace_back(new RectangleCollisionTest2());
	tests.emplace_back(new SegmentRectangleOverlapTest());
	tests.emplace_back(new RectangleRectangleDynamicTest());
	tests.emplace_back(new SegmentRectangleDynamicTest());
	tests.emplace_back(new SweepTunnelTest2(velocity));
	tests.emplace_back(new SweepTunnelTest1(velocity));
	tests.emplace_back(new SweepCornerTest3(velocity));
	tests.emplace_back(new SweepCornerTest2(velocity));
	tests.emplace_back(new SweepCornerTest1(velocity));
	tests.emplace_back(new CollisionTest());

	AddTests(tests);
}