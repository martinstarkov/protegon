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
			if (overlap::AABBAABB(AABB{ position1, size1 }, AABB{ position2, size2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 1) {
			position2 = mouse - size2 / 2;
			if (overlap::CircleAABB(Circle{ position1, radius1 }, AABB{ position2, size2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 2) {
			if (overlap::CircleAABB(Circle{ position2, radius2 }, AABB{ position1, size1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 3) {
			if (overlap::CircleCircle(Circle{ position2, radius2 }, Circle{ position1, radius1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 4) {
			position2 = mouse - size2 / 2;
			if (overlap::LineAABB(Line{ position1, position3 }, AABB{ position2, size2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (option == 5) {
			if (overlap::LineAABB(Line{ position2, position4 }, AABB{ position1, size1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 6) {
			if (overlap::LineCircle(Line{ position2, position4 }, Circle{ position1, radius1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 7) {
			if (overlap::LineCircle(Line{ position1, position3 }, Circle{ position2, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 8) {
			if (overlap::LineLine(Line{ position1, position3 }, Line{ position2, position4 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Line(position2, position4, acolor2);
		} else if (option == 9) {
			if (overlap::PointAABB(Point{ position2 }, AABB{ position1, size1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 10) {
			position2 = mouse - size2 / 2;
			if (overlap::PointAABB(Point{ position1 }, AABB{ position2, size2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 11) {
			if (overlap::PointCircle(Point{ position2 }, Circle{ position1, radius1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position1, radius1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 12) {
			if (overlap::PointCircle(Point{ position1 }, Circle{ position2, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 13) {
			if (overlap::PointLine(Point{ position1 }, Line{ position2, position4 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 14) {
			if (overlap::PointLine(Point{ position2 }, Line{ position1, position3 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 15) {
			if (overlap::PointPoint(Point{ position2 }, Point{ position1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Point(position1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 16) {
			if (overlap::CapsuleCapsule(Capsule{ position1, position3, radius1 },
										Capsule{ position2, position4, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Capsule(position2, position4, radius2, acolor2);
		} else if (option == 17) {
			if (overlap::CircleCapsule(Circle{ position1, radius1 },
									   Capsule{ position2, position4, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 18) {
			if (overlap::CircleCapsule(Circle{ position2, radius2 },
									   Capsule{ position1, position3, radius1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 19) {
			position2 = mouse - size2 / 2;
			if (overlap::CapsuleAABB(Capsule{ position1, position3, radius1 },
									 AABB{ position2, size2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (option == 20) {
			if (overlap::CapsuleAABB(Capsule{ position2, position4, radius2 },
									 AABB{ position1, size1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 21) {
			if (overlap::LineCapsule(Line{ position1, position3 },
									 Capsule{ position2, position4, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Line(position1, position3, acolor1);
		} else if (option == 22) {
			if (overlap::LineCapsule(Line{ position2, position4 },
									 Capsule{ position1, position3, radius1 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position1, position3, radius1, acolor1);
			draw::Line(position2, position4, acolor2);
		} else if (option == 23) {
			if (overlap::PointCapsule(Point{ position1 },
									  Capsule{ position2, position4, radius2 })) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(position2, position4, radius2, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 24) {
			if (overlap::PointCapsule(Point{ position2 },
									  Capsule{ position1, position3, radius1 })) {
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