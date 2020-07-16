#pragma once

#include <math.h>

#include "common.h"

#include "SDL.h"

struct Vec2D {
	double x, y;
	Vec2D(double x, double y) : x(x), y(y) {}
	Vec2D(int x, int y) : x(static_cast<double>(x)), y(static_cast<double>(y)) {}
	Vec2D(double both) : x(both), y(both) {}
	Vec2D(int both) : x(static_cast<double>(both)), y(static_cast<double>(both)) {}
	Vec2D() : x(0.0), y(0.0) {}
	Vec2D(std::string vstr) {
		std::size_t delimeter = vstr.find(","); // return index of delimeter
		assert(delimeter != std::string::npos && "Vec2D string constructor must contain comma delimeter");
		assert(vstr[0] == '(' && "Vec2D string constructor must start with opening parenthesis");
		assert(vstr[vstr.length() - 1] == ')' && "Vec2D string constructor must end with closing parenthesis");
		x = std::stod(vstr.substr(1, delimeter - 1)); // from first non parenthesis element to everything before comma
		y = std::stod(vstr.substr(delimeter + 1, vstr.size() - 2)); // everything after comma to before closing parenthesis
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
		return Vec2D(std::abs(v.x), std::abs(v.y));
	}
	SDL_Rect Vec2DtoSDLRect(Vec2D v) {
		return { static_cast<int>(round(x)), static_cast<int>(round(y)), static_cast<int>(round(v.x)), static_cast<int>(round(v.y)) };
	}
	double operator[] (int index) const {
		index = index % 2; // make sure index is in range
		if (index == 0) {
			return x;
		} else if (index == 1) {
			return y;
		}
		return -1.0;
	}
	double& operator[] (int index) {
		index = index % 2; // make sure index is in range
		if (index == 1) {
			return y;
		} else {
			return x;
		}
	}
	operator bool() const {
		return x != 0.0 || y != 0.0;
	}
	bool isZero() {
		return x == 0.0 && y == 0.0;
	}
	bool nonZero() {
		return x != 0.0 && y != 0.0;
	}
	Vec2D infinite() {
		return Vec2D(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
	}
	Vec2D operator+ (Vec2D v) {
		return Vec2D(x + v.x, y + v.y);
	}
	Vec2D operator+ (double f) {
		return Vec2D(x + f, y + f);
	}
	Vec2D& operator+= (Vec2D v) {
		*this = *this + v;
		return *this;
	}
	Vec2D& operator+= (double f) {
		*this = *this + f;
		return *this;
	}
	Vec2D operator- (Vec2D v) {
		return Vec2D(x - v.x, y - v.y);
	}
	Vec2D operator- (double f) {
		return Vec2D(x - f, y - f);
	}
	Vec2D& operator-= (Vec2D v) {
		*this = *this - v;
		return *this;
	}
	Vec2D& operator-= (double f) {
		*this = *this - f;
		return *this;
	}
	Vec2D operator* (const Vec2D& v) const {
		return Vec2D(x * v.x, y * v.y);
	}
	Vec2D& operator* (const Vec2D& v) {
		this->x *= v.x;
		this->y *= v.y;
		return *this;
	}
	Vec2D operator* (double f) {
		return Vec2D(x * f, y * f);
	}
	Vec2D operator* (unsigned int i) {
		return Vec2D(x * i, y * i);
	}
	Vec2D& operator*= (const Vec2D& v) {
		return (*this * v);
	}
	Vec2D& operator*= (double f) {
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
	Vec2D operator/ (double f) {
		return Vec2D(x / f, y / f);
	}
	Vec2D& operator/= (Vec2D v) {
		*this = *this / v;
		return *this;
	}
	Vec2D& operator/= (double f) {
		*this = *this / f;
		return *this;
	}
	bool operator== (Vec2D v) {
		return x == v.x && y == v.y;
	}
	bool operator== (double f) {
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
	double dotProduct(Vec2D v) {
		return x * v.x + y * v.y;
	}
	double crossProductArea(Vec2D v) {
		return x * v.y - y * v.x;
	}
	Vec2D normalized() {
		return unitVector();
	}
	Vec2D unitVector() {
		if (magnitude() != 0.0) {
			return Vec2D(x / magnitude(), y / magnitude());
		}
		return Vec2D();
	}
	Vec2D identityVector() {
		Vec2D identity = Vec2D();
		identity.x = x > 0.0 ? 1.0 : x == 0.0 ? 0.0 : -1.0;
		identity.y = y > 0.0 ? 1.0 : y == 0.0 ? 0.0 : -1.0;
		return identity;
	}
	Vec2D tangent() {
		return Vec2D(y, -x);
	}
	Vec2D opposite() {
		return Vec2D(-x, -y);
	}
	double magnitude() {
		return sqrt(x * x + y * y);
	}
	bool operator> (Vec2D v) {
		return magnitude() > v.magnitude();
	}
	bool operator> (double f) {
		return x > f || y > f;
	}
	bool operator>= (double f) {
		return x >= f || y >= f;
	}
	bool operator< (double f) {
		return x < f || y < f;
	}
	bool operator<= (double f) {
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
		j.at("x").get<double>(),
		j.at("y").get<double>()
	);
}