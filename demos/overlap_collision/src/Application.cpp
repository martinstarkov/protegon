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
	V2_int position4{ 200, 100 };
	V2_int size1{ 60, 60 };
	int radius1{ 30 };
	Color color1{ color::GREEN };
	V2_int size2{ 200, 200 };
	int radius2{ 20 };
	Color color2{ color::BLUE };
	const int options = 25;
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
		if (option == 0) {
			position2 = mouse - size2 / 2;
			if (collision::overlap::AABBvsAABB(position1, size1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 1) {
			position2 = mouse - size2 / 2;
			if (collision::overlap::CirclevsAABB(position1, radius1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 2) {
			if (collision::overlap::CirclevsAABB(position2, radius2, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 3) {
			if (collision::overlap::CirclevsCircle(position2, radius2, position1, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 4) {
			position2 = mouse - size2 / 2;
			if (collision::overlap::LinevsAABB(position1, position3, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (option == 5) {
			if (collision::overlap::LinevsAABB(position2, position4, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 6) {
			if (collision::overlap::LinevsCircle(position2, position4, position1, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 7) {
			if (collision::overlap::LinevsCircle(position1, position3, position2, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 8) {
			if (collision::overlap::LinevsLine(position1, position3, position2, position4)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Line(position2, position4, acolor2);
		} else if (option == 9) {
			if (collision::overlap::PointvsAABB(position2, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 10) {
			position2 = mouse - size2 / 2;
			if (collision::overlap::PointvsAABB(position1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 11) {
			if (collision::overlap::PointvsCircle(position2, position1, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position1, radius1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 12) {
			if (collision::overlap::PointvsCircle(position1, position2, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 13) {
			if (collision::overlap::PointvsLine(position1, position2, position4)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 14) {
			if (collision::overlap::PointvsLine(position2, position1, position3)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 15) {
			if (collision::overlap::PointvsPoint(position2, position1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Point(position1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 16) {
			if (collision::overlap::CapsulevsCapsule(position1, position3, radius1, position2, position4, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Capsule(position2, position4, radius2, acolor2);
		} else if (option == 17) {
			if (collision::overlap::CirclevsCapsule(position1, radius1, position2, position4, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 18) {
			if (collision::overlap::CirclevsCapsule(position2, radius2, position1, position3, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 19) {
			position2 = mouse - size2 / 2;
			if (collision::overlap::CapsulevsAABB(position1, position3, radius1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (option == 20) {
			if (collision::overlap::CapsulevsAABB(position2, position4, radius2, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 21) {
			if (collision::overlap::LinevsCapsule(position1, position3, position2, position4, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Line(position1, position3, acolor1);
		} else if (option == 22) {
			if (collision::overlap::LinevsCapsule(position2, position4, position1, position3, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Line(position2, position4, acolor2);
		} else if (option == 23) {
			if (collision::overlap::PointvsCapsule(position1, position2, position4, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 24) {
			if (collision::overlap::PointvsCapsule(position2, position1, position3, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Point(position2, acolor2);
		}
	}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Start("Overlap Test, 'r' to change origin, 't' to toggle through shapes", { 600, 600 }, true);
	return 0;
}