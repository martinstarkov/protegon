#pragma once
#include  "SDL.h"
#include <iostream>
#include <math.h>

struct Vec2D {
	float x, y;
	Vec2D(float x, float y) : x(x), y(y) {}
	Vec2D(int x, int y) : x((float)x), y((float)y) {}
	Vec2D() : x(0), y(0) {}
	Vec2D infinite() {
		return Vec2D(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
	}
	Vec2D operator+ (Vec2D v) {
		return Vec2D(x + v.x, y + v.y);
	}
	Vec2D operator+ (float f) {
		return Vec2D(x + f, y + f);
	}
	Vec2D& operator+= (Vec2D v) {
		*this = *this + v;
		return *this;
	}
	Vec2D& operator+= (float f) {
		*this = *this + f;
		return *this;
	}
	Vec2D operator- (Vec2D v) {
		return Vec2D(x - v.x, y - v.y);
	}
	Vec2D operator- (float f) {
		return Vec2D(x - f, y - f);
	}
	Vec2D& operator-= (Vec2D v) {
		*this = *this - v;
		return *this;
	}
	Vec2D& operator-= (float f) {
		*this = *this - f;
		return *this;
	}
	Vec2D operator* (Vec2D v) {
		return Vec2D(x * v.x, y * v.y);
	}
	Vec2D operator* (float f) {
		return Vec2D(x * f, y * f);
	}
	Vec2D& operator*= (Vec2D v) {
		*this = *this * v;
		return *this;
	}
	Vec2D& operator*= (float f) {
		*this = *this * f;
		return *this;
	}
	Vec2D operator/ (Vec2D v) {
		return Vec2D(x / v.x, y / v.y);
	}
	Vec2D operator/ (float f) {
		return Vec2D(x / f, y / f);
	}
	Vec2D& operator/= (Vec2D v) {
		*this = *this / v;
		return *this;
	}
	Vec2D& operator/= (float f) {
		*this = *this / f;
		return *this;
	}
	bool operator== (Vec2D v) {
		return x == v.x && y == v.y;
	}
	bool operator!= (Vec2D v) {
		return x != v.x && y != v.y;
	}
	Vec2D abs() {
		return Vec2D(fabs(x), fabs(y));
	}
	float dotProduct(Vec2D v) {
		return x * v.x + y * v.y;
	}
	float crossProductArea(Vec2D v) {
		return x * v.y - y * v.x;
	}
	Vec2D unitVector() {
		if (magnitude() != 0) {
			return Vec2D(x / magnitude(), y / magnitude());
		}
		return Vec2D(0, 0);
	}
	Vec2D tangent() {
		return Vec2D(-y, x);
	}
	Vec2D opposite() {
		return Vec2D(-x, -y);
	}
	float magnitude() {
		return sqrtf(x * x + y * y);
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