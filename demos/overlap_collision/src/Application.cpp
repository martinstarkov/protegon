#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Overlap.h"
#include "renderer/Colors.h"
#include "interface/Input.h"
#include "interface/Draw.h"

using namespace ptgn;

class OverlapCollisionTest : public Engine {
public:
	virtual void Init() {}
	V2_int position1{ 200, 200 };
	V2_int position3{ 300, 300 };
	V2_int position4{ 200, 300 };
	V2_int size1{ 60, 60 };
	int radius1{ 30 };
	Color color1{ color::GREEN };
	V2_int size2{ 200, 200 };
	int radius2{ 20 };
	Color color2{ color::BLUE };
	const int options = 13;
	int option = 0;
	virtual void Update(double dt) {
		auto mouse = input::GetMouseScreenPosition();
		if (input::KeyDown(Key::T)) {
			option++;
			option = option++ % options;
		}
		if (input::KeyDown(Key::R)) {
			position4 = mouse;
		}
		V2_int position2 = mouse;

		auto acolor1 = color1;
		auto acolor2 = color2;

		AABB aabb1{ position1, size1 };
		AABB aabb2{ position2, size2 };
		Circle circle1{ position1, radius1 };
		Circle circle2{ position2, radius2 };
		Line line1{ position1, position3 };
		Line line2{ position2, position4 };
		Capsule capsule1{ position1, position3, radius1 };
		Capsule capsule2{ position2, position4, radius2 };
		
		if (option == 0) {
			if (overlap::PointCircle(position2, circle1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 1) {
			if (overlap::PointCapsule(position2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 2) {
			if (overlap::PointAABB(position2, aabb1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 3) {
			if (overlap::LineLine(line2, line1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line1, acolor1);
			draw::Line(line2, acolor2);
		} else if (option == 4) {
			if (overlap::LineCircle(line2, circle1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line2, acolor2);
			draw::Circle(circle1, acolor1);
		} else if (option == 5) {
			if (overlap::LineCapsule(line2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Line(line2, acolor2);
		} else if (option == 6) {
			if (overlap::LineAABB(line2, aabb1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line2, acolor2);
			draw::AABB(aabb1, acolor1);
		} else if (option == 7) {
			if (overlap::CircleCircle(circle2, circle1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle2, acolor2);
			draw::Circle(circle1, acolor1);
		} else if (option == 8) {
			if (overlap::CircleCapsule(circle2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Circle(circle2, acolor2);
		} else if (option == 9) {
			if (overlap::CircleAABB(circle2, aabb1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Circle(circle2, acolor2);
		} else if (option == 10) {
			if (overlap::CapsuleCapsule(capsule2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Capsule(capsule2, acolor2);
		} else if (option == 11) {
			if (overlap::CapsuleAABB(capsule2, aabb1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule2, acolor2);
			draw::AABB(aabb1, acolor1);
		} else if (option == 12) {
			aabb2.position = mouse - aabb2.size / 2;
			if (overlap::AABBAABB(aabb1, aabb2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb2, acolor2);
			draw::AABB(aabb1, acolor1);
		}

		/*
		// Zero thickness geometry collisions.
		} else if (option == 13) {
			if (overlap::PointPoint(position2, position1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Point(position1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 14) {
			if (overlap::PointLine(position2, line1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line1, acolor1);
			draw::Point(position2, acolor2);
		}*/
	}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Start("Overlap Test, 'r' to change origin, 't' to toggle through shapes", { 600, 600 }, true);
	return 0;
}