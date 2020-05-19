#pragma once

struct Matrix2D {
	//Vec2D x;
	//Vec2D y;
	//Matrix2D() {
	//	x = Vec2D();
	//	y = Vec2D();
	//}
	//Matrix2D(float identity) {
	//	x = Vec2D(identity, 0.0f);
	//	y = Vec2D(0.0f, identity);
	//}
	//Matrix2D(Vec2D x, Vec2D y) : x(x), y(y) {}
	//Matrix2D(float topLeft, float topRight, float bottomLeft, float bottomRight) : x(topLeft, topRight), y(bottomLeft, bottomRight) {}

	//Matrix2D operator/ (float f) {
	//	return Matrix2D(x.x / f, x.y / f, y.x / f, y.y / f);
	//}
	//Matrix2D operator* (float f) {
	//	return Matrix2D(x.x * f, x.y * f, y.x * f, y.y * f);
	//}
	//Matrix2D operator* (Matrix2D m) {
	//	return Matrix2D(x.x * m.x.x + x.y * m.y.x, x.x * m.x.y + x.y * m.y.y, y.x * m.x.x + y.y * m.y.x, y.x * x.y + y.y * m.y.y);
	//}
	//Vec2D operator* (Vec2D v) {
	//	return Vec2D(x.x * v.x + x.y * v.y, y.x * v.x + y.y * v.y);
	//}
	//Matrix2D inverse() {
	//	return Matrix2D(y.y, -x.y, -y.x, x.x) / determinant();
	//}
	//float determinant() {
	//	return x.x * y.y - x.y * y.x;
	//}
};