#include "core/Engine.h"
#include "utility/Log.h"
#include "collision/StaticExperimental.h"
#include "collision/OverlapExperimental.h"
#include "renderer/Colors.h"
#include "interface/Input.h"
#include "interface/Draw.h"

using namespace ptgn;

class StaticCollisionTest : public Engine {
public:
	virtual void Init() {}
	V2_float position1{ 200, 200 };
	V2_float position3{ 500, 500 };
	V2_float position2{ 100, 100 };
	V2_float position4{ 300 - 50, 300 };
	V2_float size1{ 60, 60 };
	V2_float size2{ 200, 200 };
	float radius1{ 60 };
	float radius2{ 20 };
	Color color1{ color::GREEN };
	Color color2{ color::BLUE };
	const int options{ 13 };
	int option{ 7 };
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

		AABB<float> aabb1{ position1, size1 };
		AABB<float> aabb2{ position2, size2 };
		Circle<float> circle1{ position1, radius1 };
		Circle<float> circle2{ position2, radius2 };
		Line<float> line1{ position1, position3 };
		Line<float> line2{ position2, position4 };
		Capsule<float> capsule1{ position1, position3, radius1 };
		Capsule<float> capsule2{ position2, position4, radius2 };

		intersect::Collision c;

		// TODO: Implement LineCircle
		// TODO: Implement LineAABB
		// TODO: Implement CircleAABB
		// TODO: Implement CapsuleAABB

		if (option == 0) {
			/*const auto collision{ intersect::PointCircle(position2, circle1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.occured) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 1) {
			/*const auto collision{ intersect::PointCapsule(position2, capsule1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.occured) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 2) {
			/*const auto collision{ intersect::PointAABB(position2, aabb1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Point(position2, acolor2);
			if (collision.occured) {
				draw::Point(position2 + collision.penetration, color2);
				draw::Line({ position2, position2 + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 3) {
			/*const auto collision{ intersect::LineLine(line2, line1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line1, acolor1);
			draw::Line(line2, acolor2);
			if (collision.occured) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 4) {
			/*const auto collision{ intersect::LineCircle(line2, circle1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle1, acolor1);
			draw::Line(line2, acolor2);
			if (collision.occured) {
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
				draw::Line(line2.AddPenetration(collision.penetration), color2);
			}*/
		} else if (option == 5) {
			/*const auto collision{ intersect::LineCapsule(line2, capsule1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Line(line2, acolor2);
			if (collision.occured) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 6) {
			/*const auto collision{ intersect::LineAABB(line2, aabb1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Line(line2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.occured) {
				draw::Line(line2.AddPenetration(collision.penetration), color2);
				draw::Line({ line2.origin, line2.origin + collision.penetration }, color::GOLD);
				draw::Line({ line2.destination, line2.destination + collision.penetration }, color::GOLD);
			}*/
		} else if (option == 7) {
			bool occured{ intersect::CircleCircle(circle2, circle1, c) };
			if (occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Circle(circle2, acolor2);
			draw::Circle(circle1, acolor1);
			if (occured) {
				//auto new_circle{ circle2.Resolve(collision.normal * collision.depth) };
				auto new_circle{ circle2.Resolve(c.normal * c.depth) };
				draw::Circle(new_circle, color2);
				draw::Line({ circle2.c, new_circle.c }, color::GOLD);
				if (overlap::CircleCircle(new_circle, circle1)) {
					bool overlap{ overlap::CircleCircle(new_circle, circle1) };
					intersect::Collision c_2;
					bool again{ intersect::CircleCircle(new_circle, circle1, c_2) };
					if (overlap) {
						PrintLine("Overlap fell for it");
					}
					if (again) {
						PrintLine("Intersect fell for it");
					}
				}
			}
		} else if (option == 8) {
			/*const auto collision{ intersect::CircleCapsule(circle2, capsule1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule1, acolor1);
			draw::Circle(circle2, acolor2);
			if (collision.occured) {
				auto new_circle{ circle2.Resolve(collision.normal * collision.depth) };
				draw::Circle(new_circle, color2);
				draw::Line({ circle2.center, new_circle.center }, color::GOLD);
			}*/
		} else if (option == 9) {
			/*const auto collision{ intersect::CircleAABB(circle2, aabb1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb1, acolor1);
			draw::Circle(circle2, acolor2);
			if (collision.occured) {
				auto new_circle{ circle2.Resolve(collision.normal * collision.depth) };
				draw::Circle(new_circle, color2);
				draw::Line({ circle2.center, new_circle.center }, color::GOLD);
			}*/
		} else if (option == 10) {
			/*capsule2.origin = { 199, 201 };
			capsule2.destination = { 201, 199 };*/
			/*capsule2.origin = { 200, 200 };
			capsule2.destination = { 500, 500 };*/
			//const auto collision{ intersect::CapsuleCapsule(capsule2, capsule1) };
			//if (collision.Occured()) {
			//	acolor1 = color::RED;
			//	acolor2 = color::RED;
			//}
			//draw::Capsule(capsule1, acolor1);
			//draw::Capsule(capsule2, acolor2);
			//if (collision.Occured()) { // 10.0f * math::epsilon<float>
			//	const auto new_capsule{ capsule2.Resolve(collision.normal * (collision.depth)) };
			//	draw::Capsule(new_capsule, color2);
			//	draw::Line({ capsule2.origin, new_capsule.origin }, color::GOLD);
			//	draw::Line({ capsule2.destination, new_capsule.destination }, color::GOLD);
			//	if(overlap::CapsuleCapsule(new_capsule, capsule1)) {
			//		overlap::CapsuleCapsule(new_capsule, capsule1);
			//		const auto collision{ intersect::CapsuleCapsule(new_capsule, capsule1) };
			//		bool occured{ collision.Occured() };
			//		PrintLine(collision.depth);
			//		if (occured) {
			//			overlap::CapsuleCapsule(new_capsule, capsule1);
			//			const auto collision{ intersect::CapsuleCapsule(new_capsule, capsule1) };
			//			PrintLine(collision.depth);
			//		}

			//	}
			//}
		} else if (option == 11) {
			/*const auto collision{ intersect::CapsuleAABB(capsule2, aabb1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::Capsule(capsule2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.occured) {
				draw::Capsule(capsule2.AddPenetration(collision.penetration), color2);
				draw::Line({ capsule2.origin, capsule2.origin + collision.penetration }, color::GOLD);
				draw::Line({ capsule2.destination, capsule2.destination + collision.penetration }, color::GOLD);
			}
			*/
		} else if (option == 12) {
			/*aabb2.position = mouse - aabb2.size / 2;
			const auto collision{ intersect::AABBAABB(aabb2, aabb1) };
			if (collision.occured) {
				acolor1 = color::RED;
				acolor2 = color::RED;
			}
			draw::AABB(aabb2, acolor2);
			draw::AABB(aabb1, acolor1);
			if (collision.occured) {
				auto new_aabb{ aabb2.Resolve(collision.normal * collision.depth) };
				draw::AABB(new_aabb, color2);
				draw::Line({ aabb2.Center(), new_aabb.Center() }, color::GOLD);
			}*/
		}
	}
};

int main(int c, char** v) {
	StaticCollisionTest test;
	test.Start("Static Test, 'r' to change origin, 't' to toggle through shapes", { 600, 600 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}