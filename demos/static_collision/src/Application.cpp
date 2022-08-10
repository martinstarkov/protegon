#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/Static.h"
#include "renderer/Colors.h"
#include "interface/Input.h"
#include "interface/Draw.h"

using namespace ptgn;

class StaticCollisionTest : public Engine {
public:
	virtual void Init() {}
	V2_int position1{ 200, 200 };
	V2_int position2{ 100, 100 };
	V2_int position3{ 500, 500 };
	V2_int position4{ 300 - 50, 300 };
	V2_int size1{ 60, 60 };
	int radius1{ 30 };
	Color color1{ color::GREEN };
	V2_int size2{ 200, 200 };
	int radius2{ 20 };
	Color color2{ color::BLUE };
	const int options{ 13 };
	int option{ 0 };
	virtual void Update(float dt) {
		auto mouse = input::GetMouseScreenPosition();
		if (input::KeyDown(Key::T)) {
			option++;
			option = option++ % options;
		}
		if (input::KeyDown(Key::R)) {
			position4 = mouse;
		}
		position2 = mouse;

		auto acolor1{ color1 };
		auto acolor2{ color2 };

		AABB aabb1{ position1, size1 };
		AABB aabb2{ position2, size2 };
		Circle circle1{ position1, radius1 };
		Circle circle2{ position2, radius2 };
		Line line1{ position1, position3 };
		Line line2{ position2, position4 };
		Capsule capsule1{ position1, position3, radius1 };
		Capsule capsule2{ position2, position4, radius2 };

		// TODO: Implement LineCircle
		// TODO: Implement LineAABB
		// TODO: Implement CircleAABB
		// TODO: Implement CapsuleAABB

		if (option == 0) {
			const auto collision{ intersect::PointCircle(position2, circle1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.Occured()) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}
		} else if (option == 1) {
			const auto collision{ intersect::PointCapsule(position2, capsule1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.Occured()) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}
		} else if (option == 2) {
			const auto collision{ intersect::PointAABB(position2, aabb1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.Occured()) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}
		} else if (option == 3) {
			const auto collision{ intersect::LineLine(line2, line1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line1, acolor1);
			draw::Line(line2, acolor2);
			if (collision.Occured()) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}
		} else if (option == 4) {
			/*const auto collision{ intersect::LineCircle(line2, circle1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line2, acolor2);
			draw::Circle(circle1, acolor1);
			if (collision.Occured()) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}
			*/
		} else if (option == 5) {
			const auto collision{ intersect::LineCapsule(line2, capsule1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Line(line2, acolor2);
			if (collision.Occured()) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}
		} else if (option == 6) {
			/*const auto collision{ intersect::LineAABB(line2, aabb1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.Occured()) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}
			*/
		} else if (option == 7) {
			const auto collision{ intersect::CircleCircle(circle2, circle1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle2, acolor2);
			draw::Circle(circle1, acolor1);
			if (collision.Occured()) {
				draw::Circle(circle2.AddPenetration(collision.penetration), color2);
				draw::Line({ circle2.center, circle2.center + collision.penetration }, color::GOLD);
			}
		} else if (option == 8) {
			const auto collision{ intersect::CircleCapsule(circle2, capsule1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Circle(circle2, acolor2);
			if (collision.Occured()) {
				draw::Circle(circle2.AddPenetration(collision.penetration), color2);
				draw::Line({ circle2.center, circle2.center + collision.penetration }, color::GOLD);
			}
		} else if (option == 9) {
			/*const auto collision{ intersect::CircleAABB(circle2, aabb1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Circle(circle2, acolor2);
			if (collision.Occured()) {
				draw::Circle(circle2.AddPenetration(collision.penetration), color2);
				draw::Line({ circle2.center, circle2.center + collision.penetration }, color::GOLD);
			}
			*/
		} else if (option == 10) {
			const auto collision{ intersect::CapsuleCapsule(capsule2, capsule1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Capsule(capsule2, acolor2);
			if (collision.Occured()) {
				draw::Capsule(capsule2.AddPenetration(collision.penetration), color2);
				draw::Line({ capsule2.origin, capsule2.origin + collision.penetration }, color::GOLD);
				draw::Line({ capsule2.destination, capsule2.destination + collision.penetration }, color::GOLD);
			}
		} else if (option == 11) {
			/*const auto collision{ intersect::CapsuleAABB(capsule2, aabb1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.Occured()) {
				draw::Capsule(capsule2.AddPenetration(collision.penetration), color2);
				draw::Line({ capsule2.origin, capsule2.origin + collision.penetration }, color::GOLD);
				draw::Line({ capsule2.destination, capsule2.destination + collision.penetration }, color::GOLD);
			}
			*/
		} else if (option == 12) {
			aabb2.position = mouse - aabb2.size / 2;
			const auto collision{ intersect::AABBAABB(aabb2, aabb1) };
			if (collision.Occured()) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.Occured()) {
				draw::AABB(aabb2.AddPenetration(collision.penetration), color2);
				draw::Line({ aabb2.Center(), aabb2.Center() + collision.penetration }, color::GOLD);
			}
		}
	}
};

int main(int c, char** v) {
	StaticCollisionTest test;
	test.Start("Static Test, 'r' to change origin, 't' to toggle through shapes", { 600, 600 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}