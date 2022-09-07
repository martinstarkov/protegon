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

	const int options{ 9 };
	int option{ 0 };

	virtual void Update(float dt) {
		auto mouse = input::GetMousePosition();

		if (input::KeyDown(Key::T)) {
			option++;
			option = option++ % options;
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
		
		Line<float> line1{ position1, position3 };
		Line<float> line2{ position2, position4 };
		
		//Capsule<float> capsule1{ position1, position3, radius1 };
		//Capsule<float> capsule2{ position2, position4, radius2 };

		if (option == 0) {
			if (overlap::PointLine(position2, line1)) {
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
			if (overlap::LineLine(line2, line1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			line1.Draw(acolor1);
			line2.Draw(acolor2);
		} else if (option == 4) {
			if (overlap::LineCircle(line2, circle1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			line2.Draw(acolor2);
			circle1.Draw(acolor1);
		} else if (option == 5) {
			if (overlap::LineRectangle(line2, aabb1)) {
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

		/*
		} else if (option == 10) {
			if (overlap::PointCapsule(position2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 11) {
			if (overlap::LineCapsule(line2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Line(line2, acolor2);
		} else if (option == 12) {
			if (overlap::CircleCapsule(circle2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Circle(circle2, acolor2);
		}  else if (option == 13) {
			if (overlap::CapsuleCapsule(capsule2, capsule1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Capsule(capsule2, acolor2);
		} else if (option == 14) {
			if (overlap::CapsuleAABB(capsule2, aabb1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule2, acolor2);
			draw::AABB(aabb1, acolor1);
		} 
		*/
	}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Construct("'t' to toggle shapes, 'r' to change line origin", { 600, 600 });
	return 0;
}