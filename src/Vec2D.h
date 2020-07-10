#pragma once

#include <math.h>

#include "common.h"

#include "SDL.h"

struct Vec2D {
	float x, y;
	Vec2D(float x, float y) : x(x), y(y) {}
	Vec2D(int x, int y) : x((float)x), y((float)y) {}
	Vec2D() : x(0.0f), y(0.0f) {}
	Vec2D(std::string vstr) {
		std::size_t delimeter = vstr.find(","); // return index of delimeter
		assert(delimeter != std::string::npos && "Vec2D string constructor must contain comma delimeter");
		assert(vstr[0] == '(' && "Vec2D string constructor must start with opening parenthesis");
		assert(vstr[vstr.length() - 1] == ')' && "Vec2D string constructor must end with closing parenthesis");
		x = std::stof(vstr.substr(1, delimeter - 1)); // from first non parenthesis element to everything before comma
		y = std::stof(vstr.substr(delimeter + 1, vstr.size() - 2)); // everything after comma to before closing parenthesis
	}
	friend std::istream& operator>>(std::istream& in, Vec2D& v) {
		std::string temp;
		in >> temp;
		v = Vec2D(temp);
		return in;
	}
	friend std::ostream& operator<<(std::ostream& out, const Vec2D& v) {
		out << '(' << v.x << ',' << v.y << ')';
		return out;
	}
	friend Vec2D abs(Vec2D v) {
		return Vec2D(fabs(v.x), fabs(v.y));
	}
	SDL_Rect Vec2DtoSDLRect(Vec2D v2) {
		return { (int)round(x), (int)round(y), (int)round(v2.x), (int)round(v2.y) };
	}
	float operator[] (int index) const {
		index = index % 2;
		if (index) {
			return y;
		}
		return x;
	}
	float& operator[] (int index) {
		index = index % 2;
		if (index) {
			return y;
		}
		return x;
	}
	operator bool() const {
		return x != 0 || y != 0;
	}
	bool isZero() {
		return x == 0.0f && y == 0.0f;
	}
	bool nonZero() {
		return x != 0.0f && y != 0.0f;
	}
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
	Vec2D operator* (unsigned int i) {
		return Vec2D(x * i, y * i);
	}
	Vec2D& operator*= (Vec2D v) {
		*this = *this * v;
		return *this;
	}
	Vec2D& operator*= (float f) {
		*this = *this * f;
		return *this;
	}
	Vec2D& operator- () {
		*this = Vec2D(-x, -y);
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
	bool operator== (float f) {
		return x == f && y == f;
	}
	bool intEqual(Vec2D v) {
		return round(x) == round(v.x) && round(y) == round(v.y);
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
	Vec2D normalized() {
		return unitVector();
	}
	Vec2D unitVector() {
		if (magnitude() != 0.0f) {
			return Vec2D(x / magnitude(), y / magnitude());
		}
		return Vec2D(0.0f, 0.0f);
	}
	Vec2D identityVector() { // p.s. I started by using a ternary operator here but I think it's unclear code so I wrote it out with if statements
		Vec2D identity = Vec2D();
		if (x > 0.0f) {
			identity.x = 1.0f;
		} else if (x < 0.0f) {
			identity.x = -1.0f;
		} else {
			identity.x = 0.0f;
		}
		if (y > 0.0f) {
			identity.y = 1.0f;
		} else if (y < 0.0f) {
			identity.y = -1.0f;
		} else {
			identity.y = 0.0f;
		}
		return identity;
	}
	Vec2D tangent() {
		return Vec2D(y, -x);
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
	bool operator> (float f) {
		return x > f || y > f;
	}
	bool operator>= (float f) {
		return x >= f || y >= f;
	}
	bool operator< (float f) {
		return x < f || y < f;
	}
	bool operator<= (float f) {
		return x <= f || y <= f;
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
};

// json serialization
inline void to_json(nlohmann::json& j, const Vec2D& o) {
	j["x"] = o.x;
	j["y"] = o.y;
}

inline void from_json(const nlohmann::json& j, Vec2D& o) {
	o = Vec2D(
		j.at("x").get<float>(),
		j.at("y").get<float>()
	);
}