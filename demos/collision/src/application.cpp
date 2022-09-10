#include "protegon/protegon.h"

using namespace ptgn;

class OverlapCollisionTest : public Engine {
public:
	virtual void Init() {}

	V2_float position1{ 200, 200 };
	V2_float position3{ 300, 300 };
	V2_float position4{ 200, 300 };

	V2_float size1{ 60, 60 };
	V2_float size2{ 200, 200 };

	float radius1{ 30 };
	float radius2{ 20 };

	Color color1{ color::GREEN };
	Color color2{ color::BLUE };

	int options{ 9 };
	int option{ 0 };

	int type{ 0 };
	int types{ 3 };

	virtual void Update(float dt) {
		auto mouse = input::GetMousePosition();

		if (input::KeyDown(Key::T)) {
			option++;
			option = option++ % options;
		}

		if (input::KeyDown(Key::G)) {
			type++;
			type = type++ % types;
		}

		if (input::KeyDown(Key::R)) {
			position4 = mouse;
		}

		V2_float position2 = mouse;

		auto acolor1 = color1;
		auto acolor2 = color2;

		Rectangle<float> aabb1{ position1, size1 };
		Rectangle<float> aabb2{ position2, size2 };
		
		Circle<float> circle1{ position1, radius1 };
		Circle<float> circle2{ position2, radius2 };
		
		Segment<float> line1{ position1, position3 };
		Segment<float> line2{ position2, position4 };

		if (type == 0) { // overlap
			options = 9;
			if (option == 0) {
				if (overlap::PointSegment(position2, line1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				line1.Draw(acolor1);
				position2.Draw(acolor2);
			} else if (option == 1) {
				if (overlap::PointCircle(position2, circle1)) {
					bool test = overlap::PointCircle(position2, circle1);
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				circle1.Draw(acolor1);
				position2.Draw(acolor2);
			} else if (option == 2) {
				if (overlap::PointRectangle(position2, aabb1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				aabb1.Draw(acolor1);
				position2.Draw(acolor2);
			} else if (option == 3) {
				if (overlap::SegmentSegment(line2, line1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				line1.Draw(acolor1);
				line2.Draw(acolor2);
			} else if (option == 4) {
				if (overlap::SegmentCircle(line2, circle1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				line2.Draw(acolor2);
				circle1.Draw(acolor1);
			} else if (option == 5) {
				if (overlap::SegmentRectangle(line2, aabb1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				line2.Draw(acolor2);
				aabb1.Draw(acolor1);
			} else if (option == 6) {
				if (overlap::CircleCircle(circle2, circle1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				circle2.Draw(acolor2);
				circle1.Draw(acolor1);
			} else if (option == 7) {
				if (overlap::CircleRectangle(circle2, aabb1)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				aabb1.Draw(acolor1);
				circle2.Draw(acolor2);
			} else if (option == 8) {
				aabb2.position = mouse - aabb2.size / 2;
				if (overlap::RectangleRectangle(aabb1, aabb2)) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				aabb2.Draw(acolor2);
				aabb1.Draw(acolor1);
			}
		} else if (type == 1) { // intersect
			options = 3;
			const float slop{ 0.005f };
			intersect::Collision c;
			if (option == 0) {
				//circle2.center = circle1.center;
				bool occured{ intersect::CircleCircle(circle2, circle1, c) };
				if (occured) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				circle2.Draw(acolor2);
				circle1.Draw(acolor1);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop), circle2.radius };
					new_circle.Draw(color2);
					Segment<float> l{ circle2.center, new_circle.center };
					l.Draw(color::GOLD);
					if (overlap::CircleCircle(new_circle, circle1)) {
						occured = intersect::CircleCircle(new_circle, circle1, c);
						bool overlap{ overlap::CircleCircle(new_circle, circle1) };
						if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
						if (occured) PrintLine("Slop insufficient, intersect reoccurs");
					}
				}
			} else if (option == 1) {
				//circle2.center = aabb1.position;
			    //circle2.center = aabb1.Center();
				bool occured{ intersect::CircleRectangle(circle2, aabb1, c) };
				if (occured) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				aabb1.Draw(acolor1);
				circle2.Draw(acolor2);
				if (occured) {
					Circle<float> new_circle{ circle2.center + c.normal * (c.depth + slop), circle2.radius };
					new_circle.Draw(color2);
					Segment<float> l{ circle2.center, new_circle.center };
					l.Draw(color::GOLD);
					if (overlap::CircleRectangle(new_circle, aabb1)) {
						occured = intersect::CircleRectangle(new_circle, aabb1, c);
						bool overlap{ overlap::CircleRectangle(new_circle, aabb1) };
						if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
						if (occured) PrintLine("Slop insufficient, intersect reoccurs");
					}
				}
			} else if (option == 2) {
				aabb2.position = mouse - aabb2.Half();
				//aabb2.position = aabb1.Center() - aabb2.Half();
				bool occured{ intersect::RectangleRectangle(aabb2, aabb1, c) };
				if (occured) {
					acolor1 = color::RED;
					acolor2 = color::RED;
				}
				aabb2.Draw(acolor2);
				aabb1.Draw(acolor1);
				if (occured) {
					Rectangle<float> new_aabb{ aabb2.position + c.normal * (c.depth + slop), aabb2.size };
					new_aabb.Draw(color2);
					Segment<float> l{ aabb2.Center(), new_aabb.Center() };
					l.Draw(color::GOLD);
					if (overlap::RectangleRectangle(new_aabb, aabb1)) {
						occured = intersect::RectangleRectangle(new_aabb, aabb1, c);
						bool overlap{ overlap::RectangleRectangle(new_aabb, aabb1) };
						if (overlap) PrintLine("Slop insufficient, overlap reoccurs");
						if (occured) PrintLine("Slop insufficient, intersect reoccurs");
					}
				}
			}
		} else if (type == 2) { // dynamic

		}
	}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Construct("'t'=shape type, 'g'=mode, 'r'=line origin", { 600, 600 });
	return 0;
}