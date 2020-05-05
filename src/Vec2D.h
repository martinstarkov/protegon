#pragma once
#include  "SDL.h"
#include <iostream>
#include <math.h>

struct Vec2D {
	float x, y;
	Vec2D(float x, float y) : x(x), y(y) {}
	Vec2D() : x(0), y(0) {}
	Vec2D operator+ (Vec2D v) {
		return Vec2D(x + v.x, y + v.y);
	}
	Vec2D operator+ (float f) {
		return Vec2D(x + f, y + f);
	}
	Vec2D operator- (Vec2D v) {
		return Vec2D(x - v.x, y - v.y);
	}
	Vec2D operator- (float f) {
		return Vec2D(x - f, y - f);
	}
	Vec2D operator* (Vec2D v) {
		return Vec2D(x * v.x, y * v.y);
	}
	Vec2D operator* (float f) {
		return Vec2D(x * f, y * f);
	}
	Vec2D operator/ (Vec2D v) {
		return Vec2D(x / v.x, y / v.y);
	}
	Vec2D operator/ (float f) {
		return Vec2D(x / f, y / f);
	}
	bool operator== (Vec2D v) {
		return x == v.x && y == v.y;
	}
	bool operator!= (Vec2D v) {
		return x != v.x && y != v.y;
	}
	float dotProduct(Vec2D v) {
		return x * v.x + y * v.y;
	}
	Vec2D unitVector() {
		if (magnitude() != 0) {
			return Vec2D(x / magnitude(), y / magnitude());
		}
		return Vec2D(0, 0);
	}
	Vec2D opposite() {
		return Vec2D(-x, -y);
	}
	float magnitude() {
		return sqrt(x * x + y * y);
	}
	bool operator> (Vec2D v) {
		return magnitude() > v.magnitude();
	}
	bool operator>= (Vec2D v) {
		return magnitude() >= v.magnitude();
	}
	bool operator< (Vec2D v) {
		return magnitude() < v.magnitude();
	}
	bool operator<= (Vec2D v) {
		return magnitude() <= v.magnitude();
	}
	SDL_Rect Vec2DtoSDLRect(Vec2D v2) {
		return { (int)round(x), (int)round(y), (int)round(v2.x), (int)round(v2.y) };
	}
	friend std::ostream& operator<<(std::ostream& os, const Vec2D& v) {
		os << '(' << v.x << ',' << v.y << ')';
		return os;
	}
};