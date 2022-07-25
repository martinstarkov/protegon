#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Overlap.h"
#include "renderer/Colors.h"
#include "interface/Input.h"
#include "interface/Draw.h"

using namespace ptgn;

enum Type {
	POINT,
	AABB,
	CIRCLE,
	LINE
};

class OverlapCollisionTest : public Engine {
public:
	virtual void Init() {}
	V2_int position1{ 200, 200 };
	V2_int position3{ 300, 300 };
	V2_int position4{ 200, 300 };
	V2_int size1{ 60, 60 };
	int radius1{ 30 };
	Color color1{ color::GREEN };
	V2_int size2{ 40, 40 };
	int radius2{ 20 };
	Color color2{ color::BLUE };
	const int options = 6;
	int option = 0;
	Type player;
	Type target;
	virtual void Update(double dt) {
		if (input::KeyDown(Key::T)) {
			option++;
			option = option++ % options;
		}
		if (option == 0) {
			player = AABB;
			target = AABB;
		} else if (option == 1) {
			player = AABB;
			target = CIRCLE;
		} else if (option == 2) {
			player = CIRCLE;
			target = AABB;
		} else if (option == 3) {
			player = CIRCLE;
			target = CIRCLE;
		} else if (option == 4) {
			player = AABB;
			target = LINE;
		} else if (option == 5) {
			player = LINE;
			target = AABB;
		}
		auto mouse = input::GetMouseScreenPosition();
		V2_int position2;
		if (player == CIRCLE) {
			position2 = mouse;
		} else if (player == AABB) {
			position2 = mouse - size2 / 2;
		} else if (player == LINE) {
			position2 = mouse;
		}

		auto acolor1 = color1;
		auto acolor2 = color2;
		if (target == AABB && player == AABB) {
			if (collision::overlap::AABBvsAABB(position1, size1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (target == CIRCLE && player == AABB) {
			if (collision::overlap::CirclevsAABB(position1, radius1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (target == AABB && player == CIRCLE) {
			if (collision::overlap::CirclevsAABB(position2, radius2, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (target == CIRCLE && player == CIRCLE) {
			if (collision::overlap::CirclevsCircle(position2, radius2, position1, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (target == LINE && player == AABB) {
			if (collision::overlap::LinevsAABB(position1, position3, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position1, position3, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (target == AABB && player == LINE) {
			if (collision::overlap::LinevsAABB(position2, position4, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(position2, position4, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		}
	}
};

int main(int c, char** v) {
	OverlapCollisionTest test;
	test.Start("Overlap Collision Test", { 600, 600 }, true);
	return 0;
}