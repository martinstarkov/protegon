#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Fixed.h"
#include "renderer/Colors.h"
#include "interface/Input.h"
#include "interface/Draw.h"

using namespace ptgn;

class FixedCollisionTest : public Engine {
public:
	virtual void Init() {}
	V2_int position1{ 200, 200 };
	V2_int position3{ 300, 300 };
	V2_int position4{ 200, 100 };
	V2_int size1{ 60, 60 };
	int radius1{ 30 };
	Color color1{ color::GREEN };
	V2_int size2{ 200, 200 };
	int radius2{ 200 };
	Color color2{ color::BLUE };
	const int options = 6;
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
			auto collision{ collision::fixed::CirclevsCircle(position2, radius2, position1, radius1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Circle(position1, radius1, acolor1);
			if (collision.Occured()) {
				draw::Line(collision.point, position2, color::DARK_GREEN);
				draw::Circle(collision.point, radius2, color::GREEN);
			}
		} else if (option == 1) {
			auto collision{ collision::fixed::PointvsCircle(position2, position1, radius1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position1, radius1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.Occured()) {
				draw::Line(collision.point, position2, color::DARK_GREEN);
				draw::Circle(collision.point, 3, color::GREEN);
			}
		} else if (option == 2) {
			auto collision{ collision::fixed::PointvsCircle(position1, position2, radius2) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(position2, radius2, acolor2);
			draw::Point(position1, acolor1);
			if (collision.Occured()) {
				draw::Line(collision.point, position1, color::DARK_GREEN);
				draw::Circle(collision.point, 3, color::GREEN);
			}
		} else if (option == 3) {
			position2 = mouse - size2 / 2;
			auto collision{ collision::fixed::AABBvsAABB(position2, size2, position1, size1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Rectangle(position1, size1, acolor1);
			if (collision.Occured()) {
				draw::Line(collision.point, position2, color::DARK_GREEN);
				draw::Rectangle(collision.point, size2, color::GREEN);
			}
		} else if (option == 4) {
			auto collision{ collision::fixed::PointvsAABB(position2, position1, size1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position1, size1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.Occured()) {
				draw::Line(collision.point, position2, color::DARK_GREEN);
				draw::SolidCircle(collision.point, 3, color::GREEN);
			}
		} else if (option == 5) {
			position2 = mouse - size2 / 2;
			auto collision{ collision::fixed::PointvsAABB(position1, position2, size2) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Rectangle(position2, size2, acolor2);
			draw::Point(position1, acolor1);
			if (collision.Occured()) {
				draw::Line(collision.point, position1, color::DARK_GREEN);
				draw::SolidCircle(collision.point, 3, color::GREEN);
			}
		} else if (option == 6) {
			/*if (collision::fixed::LinevsCircle(position2, position4, position1, radius1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position2, position4, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 7) {
			/*if (collision::fixed::LinevsCircle(position1, position3, position2, radius2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position1, position3, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 8) {
			/*if (collision::fixed::LinevsLine(position1, position3, position2, position4)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position1, position3, acolor1);
			draw::Line(position2, position4, acolor2);
		} else if (option == 9) {
			position2 = mouse - size2 / 2;
			/*if (collision::fixed::LinevsAABB(position1, position3, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position1, position3, acolor1);
			draw::Rectangle(position2, size2, acolor2);
		} else if (option == 10) {
			/*if (collision::fixed::LinevsAABB(position2, position4, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position2, position4, acolor2);
			draw::Rectangle(position1, size1, acolor1);
		} else if (option == 11) {
			position2 = mouse - size2 / 2;
			/*if (collision::fixed::CirclevsAABB(position1, radius1, position2, size2)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Rectangle(position2, size2, acolor2);
			draw::Circle(position1, radius1, acolor1);
		} else if (option == 12) {
			/*if (collision::fixed::CirclevsAABB(position2, radius2, position1, size1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Rectangle(position1, size1, acolor1);
			draw::Circle(position2, radius2, acolor2);
		} else if (option == 13) {
			/*if (collision::fixed::PointvsLine(position1, position2, position4)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position2, position4, acolor2);
			draw::Point(position1, acolor1);
		} else if (option == 14) {
			/*if (collision::fixed::PointvsLine(position2, position1, position3)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Line(position1, position3, acolor1);
			draw::Point(position2, acolor2);
		} else if (option == 15) {
			/*if (collision::fixed::PointvsPoint(position2, position1)) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}*/
			draw::Point(position1, acolor1);
			draw::Point(position2, acolor2);
		}
	}
};

int main(int c, char** v) {
	FixedCollisionTest test;
	test.Start("Fixed Test, 'r' to change origin, 't' to toggle through shapes", { 600, 600 }, true);
	return 0;
}