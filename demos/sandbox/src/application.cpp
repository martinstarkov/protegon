#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "math/vector2.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

constexpr ptgn::V2_int window_size{ 800, 600 };

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Math and geometry
class Point2D;
class Vector2D;
class Circle;
template <typename EdgeType>
class AbstractPolygon2D;
template <typename EdgeType>
class AbstractRectangle2D;
class LineSegment2D;
class Line2D;
class Ray2D;

// Physics / Collision
class ColliderEdge;

// Drawable base and derivatives
class Drawable;
class DrawableCircle;
class DrawableLineSegment;
class PolygonalNode;
class RectangularNode;
class Path;

// GUI / Painting
class Painter;
class PaintCommandHandler;

namespace Util {

constexpr double EPSILON = 1e-6;

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
inline bool fuzzyCompare(T a, T b) {
	if constexpr (std::is_floating_point_v<T>) {
		if (std::isnan(a) && std::isnan(b)) {
			return true;
		}
	}
	return std::abs(static_cast<double>(a) - static_cast<double>(b)) < EPSILON;
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
inline bool isFuzzyZero(T a) {
	return fuzzyCompare(a, static_cast<T>(0));
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
inline bool isBetween(T lower, T value, T upper) {
	double lw = static_cast<double>(lower);
	double up = static_cast<double>(upper);

	if (lw > up) {
		return isBetween(upper, value, lower);
	}

	double val = static_cast<double>(value);
	return lw <= val && val <= up;
}

inline double clamp(double min, double value, double max) {
	return std::max(min, std::min(max, value));
}

} // namespace Util

class Stopwatch {
private:
	using Clock		= std::chrono::high_resolution_clock;
	using TimePoint = Clock::time_point;

	static constexpr long long SECONDS_TO_NANOSECONDS	   = 1'000'000'000LL;
	static constexpr long long MILLISECONDS_TO_NANOSECONDS = 1'000'000LL;
	static constexpr long long MICROSECONDS_TO_NANOSECONDS = 1'000LL;

	static constexpr double NANOSECONDS_TO_SECONDS		= 1.0 / SECONDS_TO_NANOSECONDS;
	static constexpr double NANOSECONDS_TO_MILLISECONDS = 1.0 / MILLISECONDS_TO_NANOSECONDS;
	static constexpr double NANOSECONDS_TO_MICROSECONDS = 1.0 / MICROSECONDS_TO_NANOSECONDS;

	TimePoint start;

public:
	Stopwatch() : start(Clock::now()) {}

	void startTimer() {
		start = Clock::now();
	}

	void restart() {
		start = Clock::now();
	}

	void reset() {
		start = Clock::now();
	}

	double getSeconds() const {
		return getNanoseconds() * NANOSECONDS_TO_SECONDS;
	}

	long long getMilliseconds() const {
		double elapsed = static_cast<double>(getNanoseconds());
		return static_cast<long long>(elapsed * NANOSECONDS_TO_MILLISECONDS);
	}

	long long getMicroseconds() const {
		double elapsed = static_cast<double>(getNanoseconds());
		return static_cast<long long>(elapsed * NANOSECONDS_TO_MICROSECONDS);
	}

	long long getNanoseconds() const {
		TimePoint current = Clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(current - start).count();
	}
};

template <typename T>
struct ReturnValue {
	T value;

	ReturnValue() = default;

	explicit ReturnValue(const T& defaultValue) : value(defaultValue) {}
};

class Matrix2x1 {
private:
	double m00;
	double m10;

public:
	Matrix2x1(double m00_, double m10_) : m00(m00_), m10(m10_) {}

	double getM00() const {
		return m00;
	}

	double getM10() const {
		return m10;
	}

	bool operator==(const Matrix2x1& other) const {
		return Util::fuzzyCompare(m00, other.m00) && Util::fuzzyCompare(m10, other.m10);
	}
};

class Point2D {
private:
	double x;
	double y;

public:
	Point2D(double x_ = 0.0, double y_ = 0.0);

	double getX() const;
	double getY() const;

	static double distanceBetween(const Point2D& a, const Point2D& b);
	static double angleBetween(const Point2D& a, const Point2D& b);

	static std::optional<Point2D> findClosestPoint(
		const Point2D& subject, const std::set<Point2D>& points
	);

	static std::optional<std::pair<Point2D, Point2D>> findClosestPair(
		const std::set<std::pair<Point2D, Point2D>>& pairs
	);

	static std::optional<std::pair<Point2D, Point2D>> findClosestPairAmongTwoLists(
		const std::set<Point2D>& list0, const std::set<Point2D>& list1
	);

	Point2D add(const Point2D& other) const;
	Point2D add(const Vector2D& other) const;
	Vector2D subtract(const Point2D& other) const;
	Vector2D multiply(double scalar) const;

	double distanceTo(const Point2D& other) const;
	double length() const;
	double angleBetween(const Point2D& other) const;

	Vector2D toVector2D() const;

	std::string toString() const;

	bool operator==(const Point2D& other) const;
	bool operator<(const Point2D& other) const;
};

class Matrix2x2 {
private:
	double m00, m01, m10, m11;

public:
	Matrix2x2(double m00_, double m01_, double m10_, double m11_);

	double getM00() const;
	double getM01() const;
	double getM10() const;
	double getM11() const;

	double determinant() const;
	std::optional<Matrix2x2> inverted() const;

	Matrix2x1 multiply(const Matrix2x1& other) const;

	static std::optional<Matrix2x1> solve(const Matrix2x2& A, const Matrix2x1& B);

	// Solves the system: [A0 B0; A1 B1] * [x; y] + [C0; C1] = [0; 0]
	static std::optional<Point2D> solve(
		double A0, double B0, double C0, double A1, double B1, double C1
	);

	bool operator==(const Matrix2x2& other) const;
};

class Vector2D {
public:
	static const Vector2D ZERO;

private:
	double x;
	double y;

public:
	Vector2D(double x_ = 0.0, double y_ = 0.0);

	double getX() const;
	double getY() const;

	bool equals(const Vector2D& other) const;
	bool equals(const Point2D& other) const;

	static double dot(const Vector2D& a, const Vector2D& b);
	static double angleBetween(const Vector2D& a, const Vector2D& b);

	Vector2D add(const Vector2D& other) const;
	Vector2D subtract(const Vector2D& other) const;
	Vector2D multiply(double scalar) const;
	Vector2D reversed() const;
	Vector2D normalized() const;
	Vector2D reflect(const Vector2D& normal) const;
	double dot(const Vector2D& other) const;
	bool isCollinear(const Vector2D& other) const;
	Vector2D normal() const;
	double length() const;
	double norm() const;
	double l2normValue() const;

	bool operator==(const Vector2D& other) const;
	bool operator==(const Point2D& other) const;

	Vector2D projectOnto(const Vector2D& other) const;
	Vector2D rejectionOf(const Vector2D& other) const;
	double angleBetween(const Vector2D& other) const;
	Vector2D rotate(double degrees) const;

	std::string toString() const;

	Point2D toPoint2D() const;
};

class Ray2D {
private:
	Point2D origin;
	Vector2D direction;

	double A{ 0.0 }, B{ 0.0 }, C{ 0.0 }; // line equation coefficients: Ax + By + C = 0

public:
	Ray2D() = default;

	Ray2D(const Point2D& origin_, const Vector2D& direction_);

	const Point2D& getOrigin() const;
	const Vector2D& getDirection() const;
	double getA() const;
	double getB() const;
	double getC() const;

	Point2D calculate(double t) const;
	std::optional<double> findParameterForGivenPoint(const Point2D& point) const;

	bool isParallelTo(const Ray2D& ray) const;
	bool isCollinear(const Ray2D& other) const;

	bool isPointOnBidirectionalRay(const Point2D& point) const;
	bool isPointOnRay(const Point2D& point) const;

	std::optional<Point2D> findIntersection(const Line2D& line) const;
	std::optional<Point2D> findIntersection(const Ray2D& other) const;
	std::optional<Point2D> findIntersection(const LineSegment2D& ls) const;

	Point2D findClosestPointToCenterOfCircle(const Circle& circle) const;

	std::string toString() const;
};

class Line2D {
private:
	Point2D P;
	Point2D Q;
	double slope;
	double A, B, C;
	Vector2D direction;

public:
	Line2D() = default;

	Line2D(const Point2D& P_, const Point2D& Q_);

	static std::array<double, 3> calculateEquationCoefficients(const Point2D& P, const Point2D& Q);
	static double calculateSlope(const Point2D& p0, const Point2D& p1);

	static Line2D from(const Ray2D& ray);
	static Line2D from(const LineSegment2D& ls);

	bool isPointOnLine(const Point2D& point) const;
	bool isParallelTo(const Line2D& other) const;
	std::optional<Point2D> findIntersection(const Line2D& other) const;

	double calculateDistanceToPoint(const Point2D& point) const;
	Point2D findClosestPointToCircleCenter(const Circle& circle) const;

	bool operator==(const Line2D& other) const;

	// Getters
	const Point2D& getP() const;
	const Point2D& getQ() const;
	double getSlope() const;
	double getA() const;
	double getB() const;
	double getC() const;
	const Vector2D& getDirection() const;
};

class Circle {
public:
	static const Circle UNIT_CIRCLE;

public:
	Point2D center;
	double radius;

public:
	Circle(const Point2D& center_, double radius_);

	Point2D calculatePointAt(double theta) const;
	Vector2D calculateGradientAt(double theta) const;
	Vector2D calculateNormalAt(double theta) const;
	double calculateSlopeOfTangent(double theta) const;

	std::vector<double> findParametersForGivenSlope(double slope) const;
	std::vector<Point2D> findPointsForGivenSlope(double slope) const;

	bool doesIntersect(const Line2D& line) const;
	Point2D findPointOnCircleClosestToLine(const Line2D& line) const;

	bool isPointOnCircle(const Point2D& point) const;
	bool isPointInsideCircle(const Point2D& point) const;

	std::vector<Point2D> findIntersection(const Line2D& line) const;
	std::set<Point2D> findIntersection(const Ray2D& ray) const;
	std::optional<Point2D> findIntersectionClosestToRayOrigin(const Ray2D& ray) const;
	std::set<Point2D> findIntersection(const LineSegment2D& ls) const;

	virtual Circle enlarge(double factor) const;

	const Point2D& getCenter() const;
	double getRadius() const;
};

class LineSegment2D {
public:
	LineSegment2D() = default;

	enum class NormalOrientation {
		INWARDS,
		OUTWARDS
	};

private:
	Point2D P_;
	Point2D Q_;
	double length_;
	std::string identifier_;
	Vector2D direction_;
	std::unordered_map<NormalOrientation, Vector2D> normals_;

	double A_, B_, C_; // Line equation coefficients

public:
	// Constructors
	LineSegment2D(const Point2D& P, const Point2D& Q);
	LineSegment2D(const Point2D& P, const Point2D& Q, std::string identifier);

	// Getters
	const Point2D& getP() const;
	const Point2D& getQ() const;
	double getLength() const;
	const std::string& getIdentifier() const;
	const Vector2D& getDirection() const;
	double getA() const;
	double getB() const;
	double getC() const;

	Vector2D getNormal(NormalOrientation normalOrientation) const;

	bool isPointOnLineSegment(const Point2D& point) const;
	Point2D getClosestVertexToPoint(const Point2D& point) const;

	std::optional<Point2D> findIntersection(const LineSegment2D& other) const;

	std::string toString() const;

private:
	void constructNormals();
};

class ColliderEdge : public LineSegment2D {
private:
	Line2D line;

public:
	ColliderEdge() = default;

	// Constructors
	ColliderEdge(const Point2D& P, const Point2D& Q, const std::string& identifier) :
		LineSegment2D(P, Q, identifier), line(Line2D::from(*this)) {}

	ColliderEdge(const Point2D& P, const Point2D& Q) :
		LineSegment2D(P, Q), line(Line2D::from(*this)) {}

	// Getter for the line
	const Line2D& getLine() const {
		return line;
	}
};

const Vector2D Vector2D::ZERO = Vector2D(0.0, 0.0);

const Circle Circle::UNIT_CIRCLE(Point2D(0.0, 0.0), 1.0);

inline std::ostream& operator<<(std::ostream& os, const Point2D& p) {
	os << p.toString();
	return os;
}

inline std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
	os << v.toString();
	return os;
}

LineSegment2D::LineSegment2D(const Point2D& P, const Point2D& Q) : LineSegment2D(P, Q, "") {}

LineSegment2D::LineSegment2D(const Point2D& P, const Point2D& Q, std::string identifier) :
	P_(P), Q_(Q), identifier_(std::move(identifier)) {
	length_			  = P_.distanceTo(Q_);
	auto coefficients = Line2D::calculateEquationCoefficients(P_, Q_);
	A_				  = coefficients.at(0);
	B_				  = coefficients.at(1);
	C_				  = coefficients.at(2);

	direction_ = Q_.subtract(P_).normalized();
	constructNormals();
}

const Point2D& LineSegment2D::getP() const {
	return P_;
}

const Point2D& LineSegment2D::getQ() const {
	return Q_;
}

double LineSegment2D::getLength() const {
	return length_;
}

const std::string& LineSegment2D::getIdentifier() const {
	return identifier_;
}

const Vector2D& LineSegment2D::getDirection() const {
	return direction_;
}

double LineSegment2D::getA() const {
	return A_;
}

double LineSegment2D::getB() const {
	return B_;
}

double LineSegment2D::getC() const {
	return C_;
}

Vector2D LineSegment2D::getNormal(NormalOrientation normalOrientation) const {
	auto it = normals_.find(normalOrientation);
	if (it != normals_.end()) {
		return it->second;
	}
	throw std::runtime_error("NormalOrientation not found");
}

bool LineSegment2D::isPointOnLineSegment(const Point2D& point) const {
	if (point == P_ || point == Q_) {
		return true;
	}
	double totalDistance = point.distanceTo(P_) + point.distanceTo(Q_);
	return Util::fuzzyCompare(length_, totalDistance);
}

Point2D LineSegment2D::getClosestVertexToPoint(const Point2D& point) const {
	double d0 = point.distanceTo(P_);
	double d1 = point.distanceTo(Q_);
	return (d0 < d1) ? P_ : Q_;
}

std::optional<Point2D> LineSegment2D::findIntersection(const LineSegment2D& other) const {
	if (isPointOnLineSegment(other.getP())) {
		return other.getP();
	}
	if (isPointOnLineSegment(other.getQ())) {
		return other.getQ();
	}
	if (other.isPointOnLineSegment(P_)) {
		return P_;
	}
	if (other.isPointOnLineSegment(Q_)) {
		return Q_;
	}

	auto intersectionOpt = Matrix2x2::solve(A_, B_, C_, other.getA(), other.getB(), other.getC());

	if (intersectionOpt) {
		const Point2D& intersection = *intersectionOpt;
		if (isPointOnLineSegment(intersection) && other.isPointOnLineSegment(intersection)) {
			return intersection;
		}
	}
	return std::nullopt;
}

std::string LineSegment2D::toString() const {
	return "LineSegment2D" + identifier_ + " : {P = " + P_.toString() + ", Q = " + Q_.toString() +
		   "}";
}

void LineSegment2D::constructNormals() {
	double dx = Q_.getX() - P_.getX();
	double dy = Q_.getY() - P_.getY();

	normals_[NormalOrientation::OUTWARDS] = Vector2D(-dy, dx).normalized();
	normals_[NormalOrientation::INWARDS]  = Vector2D(dy, -dx).normalized();
}

Circle::Circle(const Point2D& center_, double radius_) : center(center_), radius(radius_) {}

Point2D Circle::calculatePointAt(double theta) const {
	double x = center.getX() + radius * std::cos(theta);
	double y = center.getY() + radius * std::sin(theta);
	return Point2D(x, y);
}

Vector2D Circle::calculateGradientAt(double theta) const {
	return Vector2D(-radius * std::sin(theta), radius * std::cos(theta));
}

Vector2D Circle::calculateNormalAt(double theta) const {
	// Rotate gradient by PI/2 counter-clockwise = gradient(theta + PI/2)
	return calculateGradientAt(theta + 0.5 * M_PI).normalized();
}

double Circle::calculateSlopeOfTangent(double theta) const {
	double tanTheta = std::tan(theta);
	if (Util::isFuzzyZero(tanTheta)) {
		return std::numeric_limits<double>::quiet_NaN();
	} else {
		return -1.0 / tanTheta;
	}
}

std::vector<double> Circle::findParametersForGivenSlope(double slope) const {
	if (std::isnan(slope)) {
		return { 0.0, M_PI };
	} else if (Util::isFuzzyZero(slope)) {
		return { 0.5 * M_PI, 1.5 * M_PI };
	} else {
		double theta = std::atan(-1.0 / slope);
		return { theta, theta + M_PI };
	}
}

std::vector<Point2D> Circle::findPointsForGivenSlope(double slope) const {
	std::vector<double> parameters = findParametersForGivenSlope(slope);
	Point2D p0					   = calculatePointAt(parameters.at(0));
	Point2D p1					   = calculatePointAt(parameters.at(1));
	return { p0, p1 };
}

bool Circle::doesIntersect(const Line2D& line) const {
	bool intersects				= false;
	std::vector<Point2D> points = findPointsForGivenSlope(line.getSlope());
	Line2D perpendicularLine(points.at(0), points.at(1));
	std::optional<Point2D> intersection = perpendicularLine.findIntersection(line);
	if (intersection.has_value()) {
		intersects = isPointInsideCircle(intersection.value());
	}
	return intersects;
}

Point2D Circle::findPointOnCircleClosestToLine(const Line2D& line) const {
	double slope				= line.getSlope();
	std::vector<Point2D> points = findPointsForGivenSlope(slope);
	Point2D p0					= points.at(0);
	Point2D p1					= points.at(1);
	double dist0				= line.calculateDistanceToPoint(p0);
	double dist1				= line.calculateDistanceToPoint(p1);
	return dist0 < dist1 ? p0 : p1;
}

bool Circle::isPointOnCircle(const Point2D& point) const {
	double dx = center.getX() - point.getX();
	double dy = center.getY() - point.getY();
	return Util::fuzzyCompare(radius * radius, dx * dx + dy * dy);
}

bool Circle::isPointInsideCircle(const Point2D& point) const {
	double dx = center.getX() - point.getX();
	double dy = center.getY() - point.getY();
	return dx * dx + dy * dy <= radius * radius;
}

std::vector<Point2D> Circle::findIntersection(const Line2D& line) const {
	Point2D vertex			= line.getQ();
	Vector2D direction		= line.getDirection();
	Vector2D vertexToCenter = center.subtract(vertex);
	double dot				= direction.dot(vertexToCenter);

	Point2D closestPointToCenter = vertex.add(direction.multiply(dot));

	if (isPointOnCircle(closestPointToCenter)) {
		// Tangent line
		return { closestPointToCenter };
	} else if (isPointInsideCircle(closestPointToCenter)) {
		double distToCenter		  = center.distanceTo(closestPointToCenter);
		double distToIntersection = std::sqrt(radius * radius - distToCenter * distToCenter);

		Point2D p0 = closestPointToCenter.add(direction.multiply(distToIntersection));
		Point2D p1 = closestPointToCenter.add(direction.multiply(-distToIntersection));

		return { p0, p1 };
	}
	return {};
}

std::set<Point2D> Circle::findIntersection(const Ray2D& ray) const {
	std::set<Point2D> result;
	Line2D line						   = Line2D::from(ray);
	std::vector<Point2D> intersections = findIntersection(line);
	for (const auto& intersection : intersections) {
		if (ray.isPointOnRay(intersection)) {
			result.insert(intersection);
		}
	}
	return result;
}

std::optional<Point2D> Circle::findIntersectionClosestToRayOrigin(const Ray2D& ray) const {
	std::set<Point2D> intersections = findIntersection(ray);
	if (intersections.empty()) {
		return std::nullopt;
	}
	return Point2D::findClosestPoint(ray.getOrigin(), intersections);
}

std::set<Point2D> Circle::findIntersection(const LineSegment2D& ls) const {
	std::set<Point2D> result;
	Line2D line						   = Line2D::from(ls);
	std::vector<Point2D> intersections = findIntersection(line);
	for (const auto& intersection : intersections) {
		if (ls.isPointOnLineSegment(intersection)) {
			result.insert(intersection);
		}
	}
	return result;
}

Circle Circle::enlarge(double factor) const {
	return Circle(center, (1.0 + factor) * radius);
}

const Point2D& Circle::getCenter() const {
	return center;
}

double Circle::getRadius() const {
	return radius;
}

Line2D::Line2D(const Point2D& P_, const Point2D& Q_) : P(P_), Q(Q_) {
	auto coeffs = calculateEquationCoefficients(P, Q);
	A			= coeffs.at(0);
	B			= coeffs.at(1);
	C			= coeffs.at(2);

	slope	  = calculateSlope(P, Q);
	direction = Q.subtract(P).normalized();
}

std::array<double, 3> Line2D::calculateEquationCoefficients(const Point2D& P, const Point2D& Q) {
	double Px = P.getX();
	double Py = P.getY();
	double Qx = Q.getX();
	double Qy = Q.getY();

	double A = Qy - Py;
	double B = Px - Qx;
	double C = -A * Px - B * Py;

	return { A, B, C };
}

double Line2D::calculateSlope(const Point2D& p0, const Point2D& p1) {
	if (Util::fuzzyCompare(p0.getX(), p1.getX())) {
		return std::numeric_limits<double>::quiet_NaN();
	} else {
		return (p1.getY() - p0.getY()) / (p1.getX() - p0.getX());
	}
}

Line2D Line2D::from(const Ray2D& ray) {
	return Line2D(ray.getOrigin(), ray.calculate(1.0));
}

Line2D Line2D::from(const LineSegment2D& ls) {
	return Line2D(ls.getP(), ls.getQ());
}

// Member functions
bool Line2D::isPointOnLine(const Point2D& point) const {
	return Util::isFuzzyZero(A * point.getX() + B * point.getY() + C);
}

bool Line2D::isParallelTo(const Line2D& other) const {
	return Util::fuzzyCompare(slope, other.slope);
}

std::optional<Point2D> Line2D::findIntersection(const Line2D& other) const {
	return Matrix2x2::solve(A, B, C, other.A, other.B, other.C);
}

double Line2D::calculateDistanceToPoint(const Point2D& point) const {
	double x0 = point.getX();
	double y0 = point.getY();

	double x1 = P.getX();
	double y1 = P.getY();

	double x2 = Q.getX();
	double y2 = Q.getY();

	double numerator   = std::abs((y2 - y1) * x0 - (x2 - x1) * y0 + x2 * y1 - y2 * x1);
	double denominator = Q.distanceTo(P);

	return numerator / denominator;
}

Point2D Line2D::findClosestPointToCircleCenter(const Circle& circle) const {
	Point2D center			= circle.getCenter();
	Vector2D originToCenter = center.subtract(P);
	double dot				= direction.dot(originToCenter);
	return P.add(direction.multiply(dot));
}

bool Line2D::operator==(const Line2D& other) const {
	return Util::fuzzyCompare(slope, other.slope) && isPointOnLine(other.P);
}

const Point2D& Line2D::getP() const {
	return P;
}

const Point2D& Line2D::getQ() const {
	return Q;
}

double Line2D::getSlope() const {
	return slope;
}

double Line2D::getA() const {
	return A;
}

double Line2D::getB() const {
	return B;
}

double Line2D::getC() const {
	return C;
}

const Vector2D& Line2D::getDirection() const {
	return direction;
}

Point2D::Point2D(double x_, double y_) : x(x_), y(y_) {}

double Point2D::getX() const {
	return x;
}

double Point2D::getY() const {
	return y;
}

double Point2D::distanceBetween(const Point2D& a, const Point2D& b) {
	double dx = a.x - b.x;
	double dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

double Point2D::angleBetween(const Point2D& a, const Point2D& b) {
	double angle = std::atan2(a.y, a.x) - std::atan2(b.y, b.x);

	if (angle > M_PI) {
		angle -= 2 * M_PI;
	} else if (angle <= -M_PI) {
		angle += 2 * M_PI;
	}

	return angle;
}

std::optional<Point2D> Point2D::findClosestPoint(
	const Point2D& subject, const std::set<Point2D>& points
) {
	if (points.empty()) {
		return std::nullopt;
	}

	double minDistance = std::numeric_limits<double>::max();
	Point2D closestPoint;

	for (const auto& point : points) {
		double dist = subject.distanceTo(point);
		if (dist < minDistance) {
			minDistance	 = dist;
			closestPoint = point;
		}
	}
	return closestPoint;
}

std::optional<std::pair<Point2D, Point2D>> Point2D::findClosestPair(
	const std::set<std::pair<Point2D, Point2D>>& pairs
) {
	if (pairs.empty()) {
		return std::nullopt;
	}

	double minDistance = std::numeric_limits<double>::max();
	std::pair<Point2D, Point2D> closestPair;

	for (const auto& pair : pairs) {
		double dist = distanceBetween(pair.first, pair.second);
		if (dist < minDistance) {
			minDistance = dist;
			closestPair = pair;
		}
	}
	return closestPair;
}

std::optional<std::pair<Point2D, Point2D>> Point2D::findClosestPairAmongTwoLists(
	const std::set<Point2D>& list0, const std::set<Point2D>& list1
) {
	if (list0.empty() || list1.empty()) {
		return std::nullopt;
	}

	double minDistance = std::numeric_limits<double>::max();
	std::pair<Point2D, Point2D> closestPair;

	for (const auto& p0 : list0) {
		for (const auto& p1 : list1) {
			double dist = p0.distanceTo(p1);
			if (dist < minDistance) {
				minDistance = dist;
				closestPair = std::make_pair(p0, p1);
			}
		}
	}
	return closestPair;
}

Point2D Point2D::add(const Point2D& other) const {
	return Point2D(x + other.x, y + other.y);
}

Point2D Point2D::add(const Vector2D& other) const {
	return Point2D(x + other.getX(), y + other.getY());
}

Vector2D Point2D::subtract(const Point2D& other) const {
	return Vector2D(x - other.x, y - other.y);
}

Vector2D Point2D::multiply(double scalar) const {
	return Vector2D(x * scalar, y * scalar);
}

double Point2D::distanceTo(const Point2D& other) const {
	return distanceBetween(*this, other);
}

double Point2D::length() const {
	return std::sqrt(x * x + y * y);
}

double Point2D::angleBetween(const Point2D& other) const {
	return angleBetween(*this, other);
}

Vector2D Point2D::toVector2D() const {
	return Vector2D(x, y);
}

std::string Point2D::toString() const {
	char buffer[50];
	snprintf(buffer, sizeof(buffer), "Point2D{x = %.2f, y = %.2f}", x, y);
	return std::string(buffer);
}

bool Point2D::operator==(const Point2D& other) const {
	return Util::fuzzyCompare(x, other.x) && Util::fuzzyCompare(y, other.y);
}

bool Point2D::operator<(const Point2D& other) const {
	// Needed for std::set ordering
	if (!Util::fuzzyCompare(x, other.x)) {
		return x < other.x;
	}
	return y < other.y;
}

Matrix2x2::Matrix2x2(double m00_, double m01_, double m10_, double m11_) :
	m00(m00_), m01(m01_), m10(m10_), m11(m11_) {}

double Matrix2x2::getM00() const {
	return m00;
}

double Matrix2x2::getM01() const {
	return m01;
}

double Matrix2x2::getM10() const {
	return m10;
}

double Matrix2x2::getM11() const {
	return m11;
}

double Matrix2x2::determinant() const {
	return m00 * m11 - m01 * m10;
}

std::optional<Matrix2x2> Matrix2x2::inverted() const {
	double det = determinant();
	if (Util::isFuzzyZero(det)) {
		return std::nullopt;
	}

	double n00 = m11 / det;
	double n01 = -m01 / det;
	double n10 = -m10 / det;
	double n11 = m00 / det;

	return Matrix2x2(n00, n01, n10, n11);
}

Matrix2x1 Matrix2x2::multiply(const Matrix2x1& other) const {
	double r0 = m00 * other.getM00() + m01 * other.getM10();
	double r1 = m10 * other.getM00() + m11 * other.getM10();
	return Matrix2x1(r0, r1);
}

std::optional<Matrix2x1> Matrix2x2::solve(const Matrix2x2& A, const Matrix2x1& B) {
	auto inv = A.inverted();
	if (!inv) {
		return std::nullopt;
	}
	return inv->multiply(B);
}

std::optional<Point2D> Matrix2x2::solve(
	double A0, double B0, double C0, double A1, double B1, double C1
) {
	Matrix2x2 lhs(A0, B0, A1, B1);
	Matrix2x1 rhs(-C0, -C1);
	auto solution = solve(lhs, rhs);
	if (!solution) {
		return std::nullopt;
	}
	return Point2D(solution->getM00(), solution->getM10());
}

bool Matrix2x2::operator==(const Matrix2x2& other) const {
	return Util::fuzzyCompare(m00, other.m00) && Util::fuzzyCompare(m01, other.m01) &&
		   Util::fuzzyCompare(m10, other.m10) && Util::fuzzyCompare(m11, other.m11);
}

Vector2D::Vector2D(double x_, double y_) : x(x_), y(y_) {}

double Vector2D::getX() const {
	return x;
}

double Vector2D::getY() const {
	return y;
}

bool Vector2D::equals(const Vector2D& other) const {
	return Util::fuzzyCompare(x, other.x) && Util::fuzzyCompare(y, other.y);
}

bool Vector2D::equals(const Point2D& other) const {
	return Util::fuzzyCompare(x, other.getX()) && Util::fuzzyCompare(y, other.getY());
}

double Vector2D::dot(const Vector2D& a, const Vector2D& b) {
	return a.x * b.x + a.y * b.y;
}

double Vector2D::angleBetween(const Vector2D& a, const Vector2D& b) {
	double radians = std::atan2(a.y, a.x) - std::atan2(b.y, b.x);
	double degrees = radians * 180.0 / M_PI;

	if (degrees < -180.0) {
		degrees += 360.0;
	}
	if (degrees > 180.0) {
		degrees -= 360.0;
	}
	return degrees;
}

Vector2D Vector2D::add(const Vector2D& other) const {
	return Vector2D(x + other.x, y + other.y);
}

Vector2D Vector2D::subtract(const Vector2D& other) const {
	return Vector2D(x - other.x, y - other.y);
}

Vector2D Vector2D::multiply(double scalar) const {
	return Vector2D(scalar * x, scalar * y);
}

Vector2D Vector2D::reversed() const {
	return multiply(-1.0);
}

Vector2D Vector2D::normalized() const {
	double l2norm = l2normValue();

	double nx = x;
	double ny = y;

	if (!Util::fuzzyCompare(l2norm, 1.0) && !Util::isFuzzyZero(l2norm)) {
		double norm	 = std::sqrt(l2norm);
		nx			/= norm;
		ny			/= norm;
	}

	return Vector2D(nx, ny);
}

Vector2D Vector2D::reflect(const Vector2D& normal) const {
	Vector2D n	  = normal.normalized();
	double dotVal = dot(*this, n);
	return subtract(n.multiply(2.0 * dotVal));
}

double Vector2D::dot(const Vector2D& other) const {
	return dot(*this, other);
}

bool Vector2D::isCollinear(const Vector2D& other) const {
	return Util::isFuzzyZero(normal().dot(other));
}

Vector2D Vector2D::normal() const {
	return Vector2D(-y, x);
}

double Vector2D::length() const {
	return std::sqrt(x * x + y * y);
}

double Vector2D::norm() const {
	return length();
}

double Vector2D::l2normValue() const {
	return x * x + y * y;
}

bool Vector2D::operator==(const Vector2D& other) const {
	return Util::fuzzyCompare(x, other.x) && Util::fuzzyCompare(y, other.y);
}

bool Vector2D::operator==(const Point2D& other) const {
	return Util::fuzzyCompare(x, other.getX()) && Util::fuzzyCompare(y, other.getY());
}

Vector2D Vector2D::projectOnto(const Vector2D& other) const {
	double dotVal = dot(other);
	double l2	  = other.l2normValue();
	return other.multiply(dotVal / l2);
}

Vector2D Vector2D::rejectionOf(const Vector2D& other) const {
	Vector2D projection = projectOnto(other);
	return subtract(projection);
}

double Vector2D::angleBetween(const Vector2D& other) const {
	return angleBetween(*this, other);
}

Vector2D Vector2D::rotate(double degrees) const {
	double radians = degrees * M_PI / 180.0;
	double rx	   = x * std::cos(radians) - y * std::sin(radians);
	double ry	   = x * std::sin(radians) + y * std::cos(radians);
	return Vector2D(rx, ry);
}

std::string Vector2D::toString() const {
	char buffer[50];
	std::snprintf(buffer, sizeof(buffer), "Vector2D{x = %.2f, y = %.2f}", x, y);
	return std::string(buffer);
}

Point2D Vector2D::toPoint2D() const {
	return Point2D(x, y);
}

Ray2D::Ray2D(const Point2D& origin_, const Vector2D& direction_) :
	origin(origin_), direction(direction_.normalized()) {
	Point2D P = origin;
	Point2D Q = origin.add(direction.multiply(1.0));

	auto coefficients = Line2D::calculateEquationCoefficients(P, Q);
	A				  = coefficients.at(0);
	B				  = coefficients.at(1);
	C				  = coefficients.at(2);
}

const Point2D& Ray2D::getOrigin() const {
	return origin;
}

const Vector2D& Ray2D::getDirection() const {
	return direction;
}

double Ray2D::getA() const {
	return A;
}

double Ray2D::getB() const {
	return B;
}

double Ray2D::getC() const {
	return C;
}

Point2D Ray2D::calculate(double t) const {
	return origin.add(direction.multiply(t));
}

std::optional<double> Ray2D::findParameterForGivenPoint(const Point2D& point) const {
	if (!isPointOnBidirectionalRay(point)) {
		return std::nullopt;
	}

	const double Ox = origin.getX();
	const double Oy = origin.getY();
	const double Px = point.getX();
	const double Py = point.getY();
	const double Dx = direction.getX();
	const double Dy = direction.getY();

	double t = (Px - Ox) / Dx;
	double s = (Py - Oy) / Dy;

	if (Util::isFuzzyZero(Dx)) {
		return s;
	} else if (Util::isFuzzyZero(Dy)) {
		return t;
	} else if (Util::fuzzyCompare(t, s)) {
		return t;
	}

	return std::nullopt;
}

bool Ray2D::isParallelTo(const Ray2D& ray) const {
	bool same	  = direction.equals(ray.direction);
	bool opposite = direction.equals(ray.direction.reversed());
	return same || opposite;
}

bool Ray2D::isCollinear(const Ray2D& other) const {
	return isParallelTo(other) && isPointOnBidirectionalRay(other.origin);
}

bool Ray2D::isPointOnBidirectionalRay(const Point2D& point) const {
	return Util::isFuzzyZero(A * point.getX() + B * point.getY() + C);
}

bool Ray2D::isPointOnRay(const Point2D& point) const {
	auto tOpt = findParameterForGivenPoint(point);
	if (tOpt) {
		return tOpt.value() >= 0.0;
	}
	return false;
}

std::optional<Point2D> Ray2D::findIntersection(const Line2D& line) const {
	auto intersection = Matrix2x2::solve(A, B, C, line.getA(), line.getB(), line.getC());
	if (intersection && isPointOnRay(intersection.value())) {
		return intersection;
	}
	return std::nullopt;
}

std::optional<Point2D> Ray2D::findIntersection(const Ray2D& other) const {
	auto intersection = Matrix2x2::solve(A, B, C, other.A, other.B, other.C);
	if (intersection && isPointOnRay(intersection.value()) &&
		other.isPointOnRay(intersection.value())) {
		return intersection;
	}
	return std::nullopt;
}

std::optional<Point2D> Ray2D::findIntersection(const LineSegment2D& ls) const {
	auto intersection = Matrix2x2::solve(A, B, C, ls.getA(), ls.getB(), ls.getC());
	if (intersection && isPointOnRay(intersection.value()) &&
		ls.isPointOnLineSegment(intersection.value())) {
		return intersection;
	}
	return std::nullopt;
}

Point2D Ray2D::findClosestPointToCenterOfCircle(const Circle& circle) const {
	Vector2D originToCenter = circle.getCenter().subtract(origin);
	double dot				= direction.dot(originToCenter);

	if (dot <= 0.0) {
		dot = 0.0;
	}

	return origin.add(direction.multiply(dot));
}

std::string Ray2D::toString() const {
	std::ostringstream oss;
	oss << "Ray2D{origin = " << origin << ", direction = " << direction << "}";
	return oss.str();
}

class RandomGenerator {
public:
	static Vector2D generateRandomVelocity(double speed) {
		double vx = nextDouble(0.5, 1.0);
		double vy = nextDouble(0.5, 1.0);
		Vector2D direction(vx, vy);
		return direction.normalized().multiply(speed);
	}

	// Returns double in [0,1)
	static double nextDouble() {
		// Use uniform_real_distribution directly here for clarity
		return uniformDist()(getEngine());
	}

	// Returns double in [0, max)
	static double nextDouble(double max) {
		std::uniform_real_distribution<double> dist(0.0, max);
		return dist(getEngine());
	}

	// Returns double in [min, max)
	static double nextDouble(double min, double max) {
		std::uniform_real_distribution<double> dist(min, max);
		return dist(getEngine());
	}

private:
	static std::mt19937_64& getEngine() {
		static thread_local std::random_device rd;
		static thread_local std::mt19937_64 engine(rd());
		return engine;
	}

	// Single shared distribution in [0,1)
	static std::uniform_real_distribution<double>& uniformDist() {
		static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
		return dist;
	}

	// Prevent instantiation
	RandomGenerator() = delete;
};

template <typename T>
class AbstractPolygon2D {
protected:
	int numberOfVertices;
	int numberOfEdges;
	std::vector<std::string> identifiers;
	std::vector<T> edges;
	std::vector<Point2D> vertices;

public:
	AbstractPolygon2D(
		const std::vector<Point2D>& vertices_, const std::vector<std::string>& identifiers_ = {}
	) :
		numberOfVertices(static_cast<int>(vertices_.size())) {
		if (numberOfVertices <= 2) {
			throw std::runtime_error("# of vertices must be at least 3.");
		}
		if (identifiers_.empty()) {
			identifiers = std::vector<std::string>(numberOfVertices, "");
		} else {
			if (identifiers_.size() != vertices_.size()) {
				throw std::runtime_error("# of vertices and # of identifiers are different.");
			}
			identifiers = identifiers_;
		}
		vertices = vertices_;
		edges.reserve(numberOfVertices);
		for (int i = 0; i < numberOfVertices; ++i) {
			const std::string& identifier = identifiers.at(i);
			const Point2D& P			  = vertices.at(i);
			const Point2D& Q			  = vertices.at((i + 1) % numberOfVertices);
			edges.push_back(createEdge(P, Q, identifier));
		}
		numberOfEdges = static_cast<int>(edges.size());
	}

public:
	virtual ~AbstractPolygon2D() = default;

	virtual T createEdge(const Point2D& P, const Point2D& Q, const std::string& identifier) const {
		return T{ P, Q, identifier };
	};

	const std::vector<T>& getEdges() const {
		return edges;
	}

	const std::vector<Point2D>& getVertices() const {
		return vertices;
	}

	std::set<Point2D> findIntersections(const AbstractPolygon2D<T>& other) const {
		std::set<Point2D> intersections;
		for (const auto& edge : edges) {
			for (const auto& otherEdge : other.edges) {
				auto optIntersection = edge.findIntersection(otherEdge);
				if (optIntersection) {
					intersections.insert(*optIntersection);
				}
			}
		}
		return intersections;
	}

	std::set<Point2D> findIntersections(const LineSegment2D& ls) const {
		std::set<Point2D> intersections;
		for (const auto& edge : edges) {
			auto optIntersection = edge.findIntersection(ls);
			if (optIntersection) {
				intersections.insert(*optIntersection);
			}
		}
		return intersections;
	}

	std::set<Point2D> findIntersections(const Ray2D& ray) const {
		std::set<Point2D> intersections;
		for (const auto& edge : edges) {
			auto optIntersection = ray.findIntersection(edge);
			if (optIntersection) {
				intersections.insert(*optIntersection);
			}
		}
		return intersections;
	}

	bool contains(const Point2D& test) const {
		bool contains = false;
		double testX  = test.getX();
		double testY  = test.getY();
		int j		  = numberOfVertices - 1;
		for (int i = 0; i < numberOfVertices; ++i) {
			double yi = vertices.at(i).getY();
			double xi = vertices.at(i).getX();
			double yj = vertices.at(j).getY();
			double xj = vertices.at(j).getX();
			if (((yi > testY) != (yj > testY)) &&
				(testX < (xj - xi) * (testY - yi) / (yj - yi) + xi)) {
				contains = !contains;
			}
			j = i;
		}
		return contains;
	}

	void translate(const Point2D& delta) {
		std::vector<Point2D> newVertices;
		newVertices.reserve(numberOfVertices);
		std::vector<T> newEdges;
		newEdges.reserve(numberOfEdges);
		for (const auto& vertex : vertices) {
			newVertices.push_back(vertex.add(delta));
		}
		for (int i = 0; i < numberOfVertices; ++i) {
			const std::string& identifier = identifiers.at(i);
			const Point2D& P			  = newVertices.at(i);
			const Point2D& Q			  = newVertices.at((i + 1) % numberOfVertices);
			newEdges.push_back(createEdge(P, Q, identifier));
		}
		vertices = std::move(newVertices);
		edges	 = std::move(newEdges);
	}
};

template <typename T>
class AbstractRectangle2D : public AbstractPolygon2D<T> {
	static_assert(std::is_base_of<LineSegment2D, T>::value, "T must be a LineSegment2D");

protected:
	Point2D leftTop;
	Point2D leftBottom;
	Point2D rightTop;
	Point2D rightBottom;
	double width;
	double height;
	double x;
	double y;

public:
	AbstractRectangle2D(double x_, double y_, double width_, double height_) :
		AbstractPolygon2D<T>(
			std::vector<Point2D>{ Point2D(x_, y_), Point2D(x_, y_ + height_),
								  Point2D(x_ + width_, y_ + height_), Point2D(x_ + width_, y_) },
			std::vector<std::string>{ "[Left]", "[Bottom]", "[Right]", "[Top]" }
		) {
		construct(x_, y_, width_, height_);
	}

	void translate(const Point2D& delta) {
		AbstractPolygon2D<T>::translate(delta);
		construct(x + delta.getX(), y + delta.getY(), width, height);
	}

	void translate(double dx, double dy) {
		translate(Point2D(dx, dy));
	}

	bool collides(const AbstractRectangle2D<T>& other) const {
		for (const auto& edge : this->edges) {
			for (const auto& otherEdge : other.edges) {
				if (edge.findIntersection(otherEdge).has_value()) {
					return true;
				}
			}
		}
		for (const auto& vertex : other.vertices) {
			if (this->contains(vertex)) {
				return true;
			}
		}
		for (const auto& vertex : this->vertices) {
			if (other.contains(vertex)) {
				return true;
			}
		}
		return false;
	}

	const Point2D& getLeftTop() const {
		return leftTop;
	}

	const Point2D& getLeftBottom() const {
		return leftBottom;
	}

	const Point2D& getRightTop() const {
		return rightTop;
	}

	const Point2D& getRightBottom() const {
		return rightBottom;
	}

	double getX() const {
		return x;
	}

	double getY() const {
		return y;
	}

	double getWidth() const {
		return width;
	}

	double getHeight() const {
		return height;
	}

protected:
	virtual T createEdge(const Point2D& P, const Point2D& Q, const std::string& identifier) const {
		return T{ P, Q, identifier };
	}

private:
	void construct(double x_, double y_, double width_, double height_) {
		leftTop		= Point2D(x_, y_);
		leftBottom	= Point2D(x_, y_ + height_);
		rightBottom = Point2D(x_ + width_, y_ + height_);
		rightTop	= Point2D(x_ + width_, y_);
		x			= x_;
		y			= y_;
		width		= width_;
		height		= height_;
	}
};

class Drawable {
public:
	virtual ~Drawable() = default;

	virtual ptgn::Color getColor() const = 0;

	virtual bool isActiveDrawable() const					= 0;
	virtual void setIsActiveDrawable(bool isActiveDrawable) = 0;

	virtual void stroke(Painter& painter, const ptgn::Color& color, double width) = 0;
	virtual void stroke(Painter& painter, const ptgn::Color& color)				  = 0;
	virtual void stroke(Painter& painter)										  = 0;

	virtual void fill(Painter& painter, const ptgn::Color& color) = 0;
	virtual void fill(Painter& painter)							  = 0;
};

class PolygonalNode : public AbstractPolygon2D<ColliderEdge>, public Drawable {
public:
	// Constructors
	PolygonalNode(
		const std::vector<Point2D>& vertices, const std::vector<std::string>& identifiers,
		const ptgn::Color& c
	);
	PolygonalNode(const std::vector<Point2D>& vertices, const ptgn::Color& c);

protected:
	ColliderEdge createEdge(const Point2D& P, const Point2D& Q, const std::string& identifier)
		const override;

public:
	// Drawable interface
	bool isActiveDrawable() const override;
	void setIsActiveDrawable(bool isActiveDrawable) override;

	void stroke(Painter& painter, const ptgn::Color& c, double width) override;
	void stroke(Painter& painter, const ptgn::Color& c) override;
	void stroke(Painter& painter) override;

	void fill(Painter& painter, const ptgn::Color& c) override;
	void fill(Painter& painter) override;

	ptgn::Color getColor() const;

private:
	ptgn::Color color;
	bool activeDrawable = true;
};

class RectangularNode : public AbstractRectangle2D<ColliderEdge>, public Drawable {
public:
	RectangularNode(double x, double y, double width, double height, const ptgn::Color& c);

	// Overrides from AbstractRectangle2D
protected:
	ColliderEdge createEdge(const Point2D& P, const Point2D& Q, const std::string& identifier)
		const override;

public:
	// Drawable interface
	bool isActiveDrawable() const override;
	void setIsActiveDrawable(bool isActiveDrawable) override;

	void stroke(Painter& painter, const ptgn::Color& c, double width) override;
	void stroke(Painter& painter, const ptgn::Color& c) override;
	void stroke(Painter& painter) override;

	void fill(Painter& painter, const ptgn::Color& c) override;
	void fill(Painter& painter) override;

	ptgn::Color getColor() const override;

private:
	ptgn::Color color;
	bool activeDrawable = true;
};

class Polygon2D : public AbstractPolygon2D<LineSegment2D> {
public:
	Polygon2D(const std::vector<Point2D>& v, const std::vector<std::string>& i) :
		AbstractPolygon2D<LineSegment2D>(v, i) {}

	explicit Polygon2D(const std::vector<Point2D>& v) : AbstractPolygon2D<LineSegment2D>(v) {}

protected:
	LineSegment2D createEdge(const Point2D& P, const Point2D& Q, const std::string& i)
		const override {
		return LineSegment2D(P, Q, i);
	}
};

class Rectangle2D : public AbstractRectangle2D<LineSegment2D> {
public:
	Rectangle2D(double x_, double y_, double w, double h) :
		AbstractRectangle2D<LineSegment2D>(x_, y_, w, h) {}

protected:
	LineSegment2D createEdge(const Point2D& P, const Point2D& Q, const std::string& identifier)
		const override {
		return LineSegment2D(P, Q, identifier);
	}
};

class Manager {
protected:
	bool paused = false;

public:
	virtual ~Manager() = default;

	void pause() {
		paused = true;
	}

	void resume() {
		paused = false;
	}

	bool isPaused() const {
		return paused;
	}
};

class Constants {
public:
	struct World {
		static constexpr double TOP_PADDING = 64.0;
		static const ptgn::Color BACKGROUND_COLOR;
		static constexpr float FRICTION_COEFFICIENT = { 0.05f };
	};

	struct Ball {
		static constexpr double RADIUS	  = 12.0;
		static constexpr double MIN_SPEED = 500.0;
		static constexpr double MAX_SPEED = 700.0;
		static constexpr double INITIAL_X = 0.5 * window_size.x;
		static constexpr double INITIAL_Y = 0.5 * window_size.y;
		static const ptgn::Color COLOR;
		static float RESTITUTION_FACTOR;
		static float DO_NOT_BOUNCE_SPEED_THRESHOLD;
	};

	struct Paddle {
		static constexpr double WIDTH	  = 192.0;
		static constexpr double HEIGHT	  = 28.0;
		static constexpr double INITIAL_X = 0.5 * (window_size.x - WIDTH);
		static constexpr double INITIAL_Y = window_size.y - 100.0;
		static const ptgn::Color COLOR;
		static constexpr double ARC_RADIUS = 0.0;
		static float FRICTION_COEFFICIENT;
	};

	struct Brick {
		static constexpr double WIDTH			   = 82.0;
		static constexpr double HEIGHT			   = 32.0;
		static constexpr double HORIZONTAL_SPACING = 2.0;
		static constexpr double VERTICAL_SPACING   = 2.0;
		static constexpr double ARC_RADIUS		   = 0.0;
		static const std::map<int, ptgn::Color> COLORS_PER_ROW;
		static const ptgn::Color INTERPOLATION_START_COLOR;
		static const ptgn::Color INTERPOLATION_END_COLOR;
		static float FRICTION_COEFFICIENT;
	};

	struct Obstacle {
		static float FRICTION_COEFFICIENT;
	};

	struct Physics {
		static constexpr float SIMULATION_RATIO				  = { 0.0125f };
		static constexpr float GRAVITY						  = { 500.0f };
		static constexpr float NET_FORCE_CALCULATOR_TOLERANCE = { 0.001f };
	};
};

const ptgn::Color Constants::World::BACKGROUND_COLOR = ptgn::Color(24, 24, 24, 255);

const ptgn::Color Constants::Ball::COLOR			 = ptgn::Color(255, 255, 255, 255);
float Constants::Ball::RESTITUTION_FACTOR			 = { 0.6f };
float Constants::Ball::DO_NOT_BOUNCE_SPEED_THRESHOLD = { 8.0f };

const ptgn::Color Constants::Paddle::COLOR	  = ptgn::Color(255, 255, 255, 255);
float Constants::Paddle::FRICTION_COEFFICIENT = { 0.0f };

const std::map<int, ptgn::Color> Constants::Brick::COLORS_PER_ROW = {
	{ 0, ptgn::Color(255, 0, 0, 255) },	  { 1, ptgn::Color(255, 64, 0, 255) },
	{ 2, ptgn::Color(255, 127, 0, 255) }, { 3, ptgn::Color(255, 196, 0, 255) },
	{ 4, ptgn::Color(255, 255, 0, 255) }, { 5, ptgn::Color(220, 255, 0, 255) },
	{ 6, ptgn::Color(170, 255, 0, 255) }, { 7, ptgn::Color(127, 255, 0, 255) },
};
const ptgn::Color Constants::Brick::INTERPOLATION_START_COLOR = ptgn::Color(90, 40, 250, 255);
const ptgn::Color Constants::Brick::INTERPOLATION_END_COLOR	  = ptgn::Color(96, 245, 145, 255);
float Constants::Brick::FRICTION_COEFFICIENT				  = { 0.0f };

float Constants::Obstacle::FRICTION_COEFFICIENT = { 0.05f };

class GameObject {
public:
	virtual ~GameObject() = default; // Virtual destructor for interface
};

class GraphicsContext {
public:
	virtual ~GraphicsContext() = default;

	// State management
	virtual void save()	   = 0;
	virtual void restore() = 0;

	// Transformations
	virtual void scale(double sx, double sy) = 0;

	// Setting drawing styles
	virtual void setStroke(const ptgn::Color& color) = 0;
	virtual void setFill(const ptgn::Color& color)	 = 0;
	virtual void setLineWidth(double width)			 = 0;

	// Clearing
	virtual void clearRect(double x, double y, double width, double height) = 0;

	// Basic shapes
	virtual void strokeLine(double x1, double y1, double x2, double y2)	   = 0;
	virtual void fillRect(double x, double y, double width, double height) = 0;
	virtual void fillRoundRect(
		double x, double y, double width, double height, double arcWidth, double arcHeight
	)																		 = 0;
	virtual void strokeRect(double x, double y, double width, double height) = 0;
	virtual void fillOval(double x, double y, double width, double height)	 = 0;
	virtual void strokeOval(double x, double y, double width, double height) = 0;

	// Paths (polygons)
	virtual void beginPath()				= 0;
	virtual void moveTo(double x, double y) = 0;
	virtual void lineTo(double x, double y) = 0;
	virtual void closePath()				= 0;
	virtual void fill()						= 0;
	virtual void stroke()					= 0;
};

class Draggable {
public:
	virtual ~Draggable() = default;

	// Default implementation for active draggable
	virtual bool isActiveDraggable() const {
		return true;
	}

	// Pure virtual methods to be implemented by subclasses
	virtual bool contains(const Point2D& query) const = 0;
	virtual void translate(const Point2D& delta)	  = 0;
};

class Collider : public GameObject {
public:
	virtual ~Collider() = default;

	// Returns edges of the collider
	virtual std::vector<ColliderEdge> getEdges() const = 0;

	// Returns friction coefficient
	virtual double getFrictionCoefficient() const = 0;

	// Returns normal vector for a given edge
	virtual Vector2D getNormalOf(const LineSegment2D& edge) const = 0;

	// Returns whether the collider is active; default implementation returns true
	virtual bool isActiveCollider() const {
		return true;
	}
};

class Painter {
public:
	Painter(std::shared_ptr<GraphicsContext> context, double width, double height);

	void scale(double scale);
	void clear();
	void fillBackground(const ptgn::Color& color);

	void save();
	void restore();

	void drawLine(const Point2D& p0, const Point2D& p1, const ptgn::Color& color, double thickness);
	void drawLine(const LineSegment2D& ls, const ptgn::Color& color, double thickness);
	void drawLine(const LineSegment2D& ls, const ptgn::Color& color);

	void stroke(const DrawableLineSegment& ls, const ptgn::Color& color, double width);
	void stroke(const DrawableLineSegment& ls, const ptgn::Color& color);
	void stroke(const DrawableLineSegment& ls);

	void fillCircle(const Point2D& center, double radius, const ptgn::Color& color);
	void fillCircle(const Circle& circle, const ptgn::Color& color);

	void fill(const DrawableCircle& circle, const ptgn::Color& color);
	void fill(const DrawableCircle& circle);

	void strokeCircle(const Point2D& center, double radius, const ptgn::Color& color, double width);
	void strokeCircle(const Circle& circle, const ptgn::Color& color, double width);

	void stroke(const DrawableCircle& circle, const ptgn::Color& color, double width);
	void stroke(const DrawableCircle& circle, const ptgn::Color& color);
	void stroke(const DrawableCircle& circle);

	void fillRectangle(double x, double y, double w, double h, const ptgn::Color& color);
	void fillRoundRectangle(
		double x, double y, double width, double height, double arcWidth, double arcHeight,
		const ptgn::Color& color
	);
	void fillRoundRectangle(const RectangularNode& rect, double arcWidth, double arcHeight);

	void strokeRectangle(
		double x, double y, double w, double h, const ptgn::Color& color, double width
	);

	void fillPolygon(const std::vector<Point2D>& vertices, const ptgn::Color& color);
	void fill(const PolygonalNode& polygon, const ptgn::Color& color);
	void fill(const PolygonalNode& polygon);
	void fill(const RectangularNode& rect, const ptgn::Color& color);
	void fill(const RectangularNode& rect);

	void strokePath(
		const std::vector<Point2D>& vertices, const ptgn::Color& color, double width, bool closePath
	);
	void stroke(const PolygonalNode& polygon, const ptgn::Color& color, double width);
	void stroke(const PolygonalNode& polygon, const ptgn::Color& color);
	void stroke(const PolygonalNode& polygon);

	void stroke(const RectangularNode& rect, const ptgn::Color& color, double width);
	void stroke(const RectangularNode& rect, const ptgn::Color& color);
	void stroke(const RectangularNode& rect);

	void stroke(const Path& path, const ptgn::Color& color, double width);
	void stroke(const Path& path, const ptgn::Color& color);
	void stroke(const Path& path);

	void processCommands(const PaintCommandHandler& handler);

private:
	std::shared_ptr<GraphicsContext> gc;
	double width;
	double height;
};

class PaintCommandHandler {
public:
	class PaintCommand {
	public:
		PaintCommand(std::shared_ptr<Drawable> shape, const ptgn::Color& color);
		virtual ~PaintCommand();

		std::shared_ptr<Drawable> getShape() const;
		ptgn::Color getColor() const;

	private:
		std::shared_ptr<Drawable> shape_;
		ptgn::Color color_;
	};

	class FillCommand : public PaintCommand {
	public:
		FillCommand(std::shared_ptr<Drawable> shape, const ptgn::Color& color);
	};

	class StrokeCommand : public PaintCommand {
	public:
		StrokeCommand(std::shared_ptr<Drawable> shape, const ptgn::Color& color, double width);

		double getWidth() const;

	private:
		double width_;
	};

	PaintCommandHandler();

	void clear();

	void drawLine(const Point2D& p0, const Point2D& p1, const ptgn::Color& color, double width);
	void drawLine(const Point2D& p0, const Point2D& p1, const ptgn::Color& color);
	void drawLine(const LineSegment2D& ls, const ptgn::Color& color, double width);
	void drawLine(const LineSegment2D& ls, const ptgn::Color& color);
	void drawLine(std::shared_ptr<DrawableLineSegment> ls, const ptgn::Color& color);
	void drawLine(std::shared_ptr<DrawableLineSegment> ls);

	void stroke(std::shared_ptr<Drawable> drawable, const ptgn::Color& color, double width);
	void stroke(std::shared_ptr<Drawable> drawable, const ptgn::Color& color);
	void stroke(std::shared_ptr<Drawable> drawable);
	void stroke(std::shared_ptr<Drawable> drawable, double width);

	void fill(std::shared_ptr<Drawable> drawable, const ptgn::Color& color);
	void fill(std::shared_ptr<Drawable> drawable);

	std::vector<std::shared_ptr<PaintCommand>> copyCommands() const;
	void setCommands(const std::vector<std::shared_ptr<PaintCommand>>& commands);

private:
	void addStrokeCommand(
		std::shared_ptr<Drawable> drawable, const ptgn::Color& color, double width
	);

	std::deque<std::shared_ptr<PaintCommand>> commands_;
	mutable std::mutex mutex_;
};

class Path : public Drawable {
public:
	Path(const std::vector<Point2D>& v, const ptgn::Color& color);

	const std::vector<Point2D>& getVertices() const;
	ptgn::Color getColor() const;

	bool isActiveDrawable() const override;
	void setIsActiveDrawable(bool isActiveDrawable) override;

	void stroke(Painter& painter, const ptgn::Color& c, double width) override;
	void stroke(Painter& painter, const ptgn::Color& c) override;
	void stroke(Painter& painter) override;

	void fill(Painter& painter, const ptgn::Color& c) override;
	void fill(Painter& painter) override;

private:
	std::vector<Point2D> vertices;
	ptgn::Color color;
	bool activeDrawable = true;
};

class DrawableLineSegment : public LineSegment2D, public Drawable {
public:
	DrawableLineSegment(const Point2D& P, const Point2D& Q, const ptgn::Color& color);
	DrawableLineSegment(
		const Point2D& P, const Point2D& Q, const std::string& identifier, const ptgn::Color& color
	);

	ptgn::Color getColor() const override;

	bool isActiveDrawable() const override;
	void setIsActiveDrawable(bool isActiveDrawable) override;

	void stroke(Painter& painter, const ptgn::Color& color, double width) override;
	void stroke(Painter& painter, const ptgn::Color& color) override;
	void stroke(Painter& painter) override;

	void fill(Painter& painter, const ptgn::Color& color) override;
	void fill(Painter& painter) override;

private:
	ptgn::Color color_;
	bool isActiveDrawable_ = true;
};

class DrawableCircle : public Circle, public Drawable {
public:
	DrawableCircle(const Point2D& center, double radius, const ptgn::Color& color);

	ptgn::Color getColor() const;

	bool isActiveDrawable() const override;
	void setIsActiveDrawable(bool isActiveDrawable) override;

	void stroke(Painter& painter, const ptgn::Color& c, double width) override;
	void stroke(Painter& painter, const ptgn::Color& c) override;
	void stroke(Painter& painter) override;

	void fill(Painter& painter, const ptgn::Color& c) override;
	void fill(Painter& painter) override;

private:
	ptgn::Color color;
	bool activeDrawable = true;
};

PolygonalNode::PolygonalNode(
	const std::vector<Point2D>& vertices, const std::vector<std::string>& identifiers,
	const ptgn::Color& c
) :
	AbstractPolygon2D(vertices, identifiers), color(color), activeDrawable(true) {}

PolygonalNode::PolygonalNode(const std::vector<Point2D>& vertices, const ptgn::Color& c) :
	AbstractPolygon2D(vertices), color(color), activeDrawable(true) {}

bool PolygonalNode::isActiveDrawable() const {
	return activeDrawable;
}

void PolygonalNode::setIsActiveDrawable(bool isActive) {
	activeDrawable = isActive;
}

void PolygonalNode::stroke(Painter& painter, const ptgn::Color& c, double width_) {
	painter.stroke(*this, c, width_);
}

void PolygonalNode::stroke(Painter& painter, const ptgn::Color& c) {
	painter.stroke(*this, c);
}

void PolygonalNode::stroke(Painter& painter) {
	painter.stroke(*this);
}

void PolygonalNode::fill(Painter& painter, const ptgn::Color& c) {
	painter.fill(*this, c);
}

void PolygonalNode::fill(Painter& painter) {
	painter.fill(*this);
}

ptgn::Color PolygonalNode::getColor() const {
	return color;
}

DrawableCircle::DrawableCircle(const Point2D& center, double radius, const ptgn::Color& color) :
	Circle(center, radius), color(color), activeDrawable(true) {}

ptgn::Color DrawableCircle::getColor() const {
	return color;
}

bool DrawableCircle::isActiveDrawable() const {
	return activeDrawable;
}

void DrawableCircle::setIsActiveDrawable(bool isActive) {
	activeDrawable = isActive;
}

void DrawableCircle::stroke(Painter& painter, const ptgn::Color& c, double width) {
	painter.stroke(*this, c, width);
}

void DrawableCircle::stroke(Painter& painter, const ptgn::Color& c) {
	painter.stroke(*this, c);
}

void DrawableCircle::stroke(Painter& painter) {
	painter.stroke(*this);
}

void DrawableCircle::fill(Painter& painter, const ptgn::Color& c) {
	painter.fill(*this, c);
}

void DrawableCircle::fill(Painter& painter) {
	painter.fill(*this);
}

Path::Path(const std::vector<Point2D>& vertices, const ptgn::Color& c) :
	vertices(vertices), color(c), activeDrawable(true) {}

const std::vector<Point2D>& Path::getVertices() const {
	return vertices;
}

ptgn::Color Path::getColor() const {
	return color;
}

bool Path::isActiveDrawable() const {
	return activeDrawable;
}

void Path::setIsActiveDrawable(bool isActive) {
	activeDrawable = isActive;
}

void Path::stroke(Painter& painter, const ptgn::Color& c, double width_) {
	painter.stroke(*this, c, width_);
}

void Path::stroke(Painter& painter, const ptgn::Color& c) {
	painter.stroke(*this, c);
}

void Path::stroke(Painter& painter) {
	painter.stroke(*this);
}

void Path::fill(Painter& painter, const ptgn::Color& /*color*/) {
	throw std::logic_error("Path cannot be filled!");
}

void Path::fill(Painter& painter) {
	throw std::logic_error("Path cannot be filled!");
}

RectangularNode::RectangularNode(
	double x, double y, double width, double height, const ptgn::Color& c
) :
	AbstractRectangle2D(x, y, width, height), color(color), activeDrawable(true) {}

ColliderEdge PolygonalNode::createEdge(
	const Point2D& P, const Point2D& Q, const std::string& identifier
) const {
	return ColliderEdge(P, Q, identifier);
}

ColliderEdge RectangularNode::createEdge(
	const Point2D& P, const Point2D& Q, const std::string& identifier
) const {
	return ColliderEdge(P, Q, identifier);
}

bool RectangularNode::isActiveDrawable() const {
	return activeDrawable;
}

void RectangularNode::setIsActiveDrawable(bool isActive) {
	activeDrawable = isActive;
}

void RectangularNode::stroke(Painter& painter, const ptgn::Color& c, double width_) {
	painter.stroke(*this, c, width_);
}

void RectangularNode::stroke(Painter& painter, const ptgn::Color& c) {
	painter.stroke(*this, c);
}

void RectangularNode::stroke(Painter& painter) {
	painter.stroke(*this);
}

void RectangularNode::fill(Painter& painter, const ptgn::Color& c) {
	painter.fill(*this, c);
}

void RectangularNode::fill(Painter& painter) {
	painter.fill(*this);
}

ptgn::Color RectangularNode::getColor() const {
	return color;
}

Painter::Painter(std::shared_ptr<GraphicsContext> context, double width, double height) :
	gc(context), width(width), height(height) {}

void Painter::scale(double scale) {
	gc->scale(scale, scale);
}

void Painter::clear() {
	gc->clearRect(0, 0, width, height);
}

void Painter::fillBackground(const ptgn::Color& color) {
	gc->save();
	gc->setFill(color);
	gc->fillRect(0, 0, width, height);
	gc->restore();
}

void Painter::save() {
	gc->save();
}

void Painter::restore() {
	gc->restore();
}

void Painter::drawLine(
	const Point2D& p0, const Point2D& p1, const ptgn::Color& color, double thickness
) {
	gc->setStroke(color);
	gc->setLineWidth(thickness);
	gc->strokeLine(p0.getX(), p0.getY(), p1.getX(), p1.getY());
}

void Painter::drawLine(const LineSegment2D& ls, const ptgn::Color& color, double thickness) {
	drawLine(ls.getP(), ls.getQ(), color, thickness);
}

void Painter::drawLine(const LineSegment2D& ls, const ptgn::Color& color) {
	drawLine(ls.getP(), ls.getQ(), color, 1.0);
}

void Painter::stroke(const DrawableLineSegment& ls, const ptgn::Color& color, double width_) {
	drawLine(ls.getP(), ls.getQ(), color, width_);
}

void Painter::stroke(const DrawableLineSegment& ls, const ptgn::Color& color) {
	stroke(ls, color, 1.0);
}

void Painter::stroke(const DrawableLineSegment& ls) {
	stroke(ls, ls.getColor(), 1.0);
}

void Painter::fillCircle(const Point2D& center, double radius, const ptgn::Color& color) {
	gc->setFill(color);
	double left = center.getX() - radius;
	double top	= center.getY() - radius;
	gc->fillOval(left, top, 2 * radius, 2 * radius);
}

void Painter::fillCircle(const Circle& circle, const ptgn::Color& color) {
	fillCircle(circle.getCenter(), circle.getRadius(), color);
}

void Painter::fill(const DrawableCircle& circle, const ptgn::Color& color) {
	fillCircle(circle.getCenter(), circle.getRadius(), color);
}

void Painter::fill(const DrawableCircle& circle) {
	fillCircle(circle.getCenter(), circle.getRadius(), circle.getColor());
}

void Painter::strokeCircle(
	const Point2D& center, double radius, const ptgn::Color& color, double width_
) {
	gc->setStroke(color);
	gc->setLineWidth(width_);
	double left = center.getX() - radius;
	double top	= center.getY() - radius;
	gc->strokeOval(left, top, 2 * radius, 2 * radius);
}

void Painter::strokeCircle(const Circle& circle, const ptgn::Color& color, double width_) {
	strokeCircle(circle.getCenter(), circle.getRadius(), color, width_);
}

void Painter::stroke(const DrawableCircle& circle, const ptgn::Color& color, double width_) {
	strokeCircle(circle, color, width_);
}

void Painter::stroke(const DrawableCircle& circle, const ptgn::Color& color) {
	strokeCircle(circle, color, 1.0);
}

void Painter::stroke(const DrawableCircle& circle) {
	strokeCircle(circle, circle.getColor(), 1.0);
}

void Painter::fillRectangle(double x, double y, double w, double h, const ptgn::Color& color) {
	gc->setFill(color);
	gc->fillRect(x, y, w, h);
}

void Painter::fillRoundRectangle(
	double x, double y, double width_, double height_, double arcWidth, double arcHeight,
	const ptgn::Color& color
) {
	gc->setFill(color);
	gc->fillRoundRect(x, y, width_, height_, arcWidth, arcHeight);
}

void Painter::fillRoundRectangle(const RectangularNode& rect, double arcWidth, double arcHeight) {
	fillRoundRectangle(
		rect.getX(), rect.getY(), rect.getWidth(), rect.getHeight(), arcWidth, arcHeight,
		rect.getColor()
	);
}

void Painter::strokeRectangle(
	double x, double y, double w, double h, const ptgn::Color& color, double width_
) {
	gc->setStroke(color);
	gc->setLineWidth(width_);
	gc->strokeRect(x, y, w, h);
}

void Painter::fillPolygon(const std::vector<Point2D>& vertices, const ptgn::Color& color) {
	gc->setFill(color);
	gc->beginPath();
	if (vertices.empty()) {
		return;
	}
	gc->moveTo(vertices.at(0).getX(), vertices.at(0).getY());

	for (size_t i = 1; i < vertices.size(); ++i) {
		gc->lineTo(vertices.at(i).getX(), vertices.at(i).getY());
	}

	gc->closePath();
	gc->fill();
}

void Painter::fill(const PolygonalNode& polygon, const ptgn::Color& color) {
	fillPolygon(polygon.getVertices(), color);
}

void Painter::fill(const PolygonalNode& polygon) {
	fillPolygon(polygon.getVertices(), polygon.getColor());
}

void Painter::fill(const RectangularNode& rect, const ptgn::Color& color) {
	fillPolygon(rect.getVertices(), color);
}

void Painter::fill(const RectangularNode& rect) {
	fillPolygon(rect.getVertices(), rect.getColor());
}

void Painter::strokePath(
	const std::vector<Point2D>& vertices, const ptgn::Color& color, double width_, bool closePath
) {
	if (vertices.empty()) {
		return;
	}
	gc->setStroke(color);
	gc->setLineWidth(width_);

	gc->beginPath();
	gc->moveTo(vertices.at(0).getX(), vertices.at(0).getY());

	for (size_t i = 1; i < vertices.size(); ++i) {
		gc->lineTo(vertices.at(i).getX(), vertices.at(i).getY());
	}

	if (closePath) {
		gc->closePath();
	}

	gc->stroke();
}

void Painter::stroke(const PolygonalNode& polygon, const ptgn::Color& color, double width_) {
	strokePath(polygon.getVertices(), color, width_, true);
}

void Painter::stroke(const PolygonalNode& polygon, const ptgn::Color& color) {
	strokePath(polygon.getVertices(), color, 1.0, true);
}

void Painter::stroke(const PolygonalNode& polygon) {
	strokePath(polygon.getVertices(), polygon.getColor(), 1.0, true);
}

void Painter::stroke(const RectangularNode& rect, const ptgn::Color& color, double width_) {
	strokePath(rect.getVertices(), color, width_, true);
}

void Painter::stroke(const RectangularNode& rect, const ptgn::Color& color) {
	strokePath(rect.getVertices(), color, 1.0, true);
}

void Painter::stroke(const RectangularNode& rect) {
	strokePath(rect.getVertices(), rect.getColor(), 1.0, true);
}

void Painter::stroke(const Path& path, const ptgn::Color& color, double width_) {
	strokePath(path.getVertices(), color, width_, false);
}

void Painter::stroke(const Path& path, const ptgn::Color& color) {
	strokePath(path.getVertices(), color, 1.0, false);
}

void Painter::stroke(const Path& path) {
	strokePath(path.getVertices(), path.getColor(), 1.0, false);
}

void Painter::processCommands(const PaintCommandHandler& handler) {
	auto commands = handler.copyCommands();

	for (const auto& command : commands) {
		auto color = command->getColor();
		auto shape = command->getShape();

		if (dynamic_cast<const PaintCommandHandler::FillCommand*>(command.get()) != nullptr) {
			shape->fill(*this, color);
		} else if (auto strokeCmd =
					   dynamic_cast<const PaintCommandHandler::StrokeCommand*>(command.get())) {
			shape->stroke(*this, color, strokeCmd->getWidth());
		}
	}
}

PaintCommandHandler::PaintCommand::PaintCommand(
	std::shared_ptr<Drawable> shape, const ptgn::Color& c
) :
	shape_(std::move(shape)), color_(c) {}

PaintCommandHandler::PaintCommand::~PaintCommand() = default;

std::shared_ptr<Drawable> PaintCommandHandler::PaintCommand::getShape() const {
	return shape_;
}

ptgn::Color PaintCommandHandler::PaintCommand::getColor() const {
	return color_;
}

// --- FillCommand ---

PaintCommandHandler::FillCommand::FillCommand(
	std::shared_ptr<Drawable> shape, const ptgn::Color& c
) :
	PaintCommand(std::move(shape), c) {}

// --- StrokeCommand ---

PaintCommandHandler::StrokeCommand::StrokeCommand(
	std::shared_ptr<Drawable> shape, const ptgn::Color& c, double width
) :
	PaintCommand(std::move(shape), c), width_(width) {}

double PaintCommandHandler::StrokeCommand::getWidth() const {
	return width_;
}

// --- PaintCommandHandler ---

PaintCommandHandler::PaintCommandHandler() = default;

void PaintCommandHandler::clear() {
	std::lock_guard<std::mutex> lock(mutex_);
	commands_.clear();
}

void PaintCommandHandler::drawLine(
	const Point2D& p0, const Point2D& p1, const ptgn::Color& c, double width_
) {
	auto segment = std::make_shared<DrawableLineSegment>(p0, p1, c);
	addStrokeCommand(segment, c, width_);
}

void PaintCommandHandler::drawLine(const Point2D& p0, const Point2D& p1, const ptgn::Color& c) {
	drawLine(p0, p1, c, 1.0);
}

void PaintCommandHandler::drawLine(const LineSegment2D& ls, const ptgn::Color& c, double width) {
	auto segment = std::make_shared<DrawableLineSegment>(ls.getP(), ls.getQ(), c);
	addStrokeCommand(segment, c, width);
}

void PaintCommandHandler::drawLine(const LineSegment2D& ls, const ptgn::Color& c) {
	drawLine(ls, c, 1.0);
}

void PaintCommandHandler::drawLine(std::shared_ptr<DrawableLineSegment> ls, const ptgn::Color& c) {
	addStrokeCommand(std::move(ls), c, 1.0);
}

void PaintCommandHandler::drawLine(std::shared_ptr<DrawableLineSegment> ls) {
	addStrokeCommand(ls, ls->getColor(), 1.0);
}

void PaintCommandHandler::stroke(
	std::shared_ptr<Drawable> drawable, const ptgn::Color& c, double width
) {
	addStrokeCommand(std::move(drawable), c, width);
}

void PaintCommandHandler::stroke(std::shared_ptr<Drawable> drawable, const ptgn::Color& c) {
	stroke(std::move(drawable), c, 1.0);
}

void PaintCommandHandler::stroke(std::shared_ptr<Drawable> drawable) {
	stroke(std::move(drawable), drawable->getColor(), 1.0);
}

void PaintCommandHandler::stroke(std::shared_ptr<Drawable> drawable, double width) {
	stroke(std::move(drawable), drawable->getColor(), width);
}

void PaintCommandHandler::fill(std::shared_ptr<Drawable> drawable, const ptgn::Color& c) {
	std::lock_guard<std::mutex> lock(mutex_);
	commands_.push_back(std::make_shared<FillCommand>(std::move(drawable), c));
}

void PaintCommandHandler::fill(std::shared_ptr<Drawable> drawable) {
	fill(std::move(drawable), drawable->getColor());
}

std::vector<std::shared_ptr<PaintCommandHandler::PaintCommand>> PaintCommandHandler::copyCommands(
) const {
	std::lock_guard<std::mutex> lock(mutex_);
	return std::vector<std::shared_ptr<PaintCommand>>(commands_.begin(), commands_.end());
}

void PaintCommandHandler::setCommands(const std::vector<std::shared_ptr<PaintCommand>>& commands) {
	std::lock_guard<std::mutex> lock(mutex_);
	commands_.clear();
	commands_.insert(commands_.end(), commands.begin(), commands.end());
}

void PaintCommandHandler::addStrokeCommand(
	std::shared_ptr<Drawable> drawable, const ptgn::Color& c, double width
) {
	std::lock_guard<std::mutex> lock(mutex_);
	commands_.push_back(std::make_shared<StrokeCommand>(std::move(drawable), c, width));
}

DrawableLineSegment::DrawableLineSegment(const Point2D& P, const Point2D& Q, const ptgn::Color& c) :
	LineSegment2D(P, Q), color_(c) {}

DrawableLineSegment::DrawableLineSegment(
	const Point2D& P, const Point2D& Q, const std::string& identifier, const ptgn::Color& c
) :
	LineSegment2D(P, Q, identifier), color_(c) {}

ptgn::Color DrawableLineSegment::getColor() const {
	return color_;
}

bool DrawableLineSegment::isActiveDrawable() const {
	return isActiveDrawable_;
}

void DrawableLineSegment::setIsActiveDrawable(bool isActiveDrawable) {
	isActiveDrawable_ = isActiveDrawable;
}

void DrawableLineSegment::stroke(Painter& painter, const ptgn::Color& c, double width) {
	painter.stroke(*this, c, width);
}

void DrawableLineSegment::stroke(Painter& painter, const ptgn::Color& c) {
	painter.stroke(*this, c);
}

void DrawableLineSegment::stroke(Painter& painter) {
	painter.stroke(*this);
}

void DrawableLineSegment::fill(Painter& painter, const ptgn::Color& /*color*/) {
	throw std::runtime_error("Line cannot be filled!");
}

void DrawableLineSegment::fill(Painter& painter) {
	throw std::runtime_error("Line cannot be filled!");
}

class CriticalPointPair {
public:
	virtual ~CriticalPointPair() = default;

	virtual Point2D getPointOnCircle() const = 0;

	virtual Point2D getPointOnEdge() const = 0;

	// Default implementation returns 0, override if needed
	virtual double getDistance() const {
		return 0.0;
	}
};

class TangentialCriticalPoint : public CriticalPointPair {
private:
	Point2D point;

public:
	explicit TangentialCriticalPoint(const Point2D& p) : point(p) {}

	Point2D getPointOnCircle() const override {
		return point;
	}

	Point2D getPointOnEdge() const override {
		return point;
	}
};

class SeparateCriticalPointPair : public CriticalPointPair {
private:
	Point2D pointOnCircle;
	Point2D pointOnEdge;

public:
	SeparateCriticalPointPair(const Point2D& circlePoint, const Point2D& edgePoint) :
		pointOnCircle(circlePoint), pointOnEdge(edgePoint) {}

	Point2D getPointOnCircle() const override {
		return pointOnCircle;
	}

	Point2D getPointOnEdge() const override {
		return pointOnEdge;
	}

	double getDistance() const override {
		return Point2D::distanceBetween(pointOnEdge, pointOnCircle);
	}
};

class CuttingCriticalPointPair : public CriticalPointPair {
private:
	std::vector<Point2D> points;

public:
	explicit CuttingCriticalPointPair(const std::vector<Point2D>& pts) : points(pts) {}

	Point2D getPointOnCircle() const override {
		return points.at(0);
	}

	Point2D getPointOnEdge() const override {
		return points.at(0);
	}

	// You can add other methods to access points if needed
	const std::vector<Point2D>& getPoints() const {
		return points;
	}
};

class CriticalPointFinder {
public:
	// Using smart pointers for polymorphic CriticalPointPair
	static std::optional<std::shared_ptr<CriticalPointPair>> findCriticalPointsAlongGivenDirection(
		const Circle& circle, const ColliderEdge& edge, const Vector2D& direction
	) {
		std::shared_ptr<CriticalPointPair> result = nullptr;
		Line2D line								  = edge.getLine();
		std::vector<Point2D> intersections		  = circle.findIntersection(line);

		if (intersections.empty()) {
			// Case 1: line does not intersect the circle
			if (direction.l2normValue() != 0) {
				Point2D pointOnCircleClosestToLine = circle.findPointOnCircleClosestToLine(line);
				Ray2D rayFromCircleToLine(pointOnCircleClosestToLine, direction);

				auto pointOnLineOpt = rayFromCircleToLine.findIntersection(line);
				if (pointOnLineOpt) {
					Point2D pointOnLine = *pointOnLineOpt;
					if (edge.isPointOnLineSegment(pointOnLine)) {
						result = std::make_shared<SeparateCriticalPointPair>(
							pointOnCircleClosestToLine, pointOnLine
						);
					} else {
						Point2D closestVertex = edge.getClosestVertexToPoint(pointOnLine);
						Ray2D rayFromVertexToCircle(closestVertex, direction.reversed());
						auto pointOnCircleOpt =
							circle.findIntersectionClosestToRayOrigin(rayFromVertexToCircle);
						if (pointOnCircleOpt) {
							result = std::make_shared<SeparateCriticalPointPair>(
								*pointOnCircleOpt, closestVertex
							);
						}
					}
				}
			}
		} else {
			// Case 2: line intersects the circle
			std::vector<Point2D> pointsOnLineSegment;
			for (const auto& intersection : intersections) {
				if (edge.isPointOnLineSegment(intersection)) {
					pointsOnLineSegment.push_back(intersection);
				}
			}

			if (pointsOnLineSegment.size() == 1) {
				result = std::make_shared<TangentialCriticalPoint>(pointsOnLineSegment.at(0));
			} else if (pointsOnLineSegment.size() == 2) {
				result = std::make_shared<CuttingCriticalPointPair>(pointsOnLineSegment);
			}

			if (!result) {
				// line segment does not intersect circle
				if (direction.l2normValue() != 0) {
					Point2D center		  = circle.getCenter();
					Point2D closestVertex = edge.getClosestVertexToPoint(center);
					Ray2D rayFromLineSegmentToCircle(closestVertex, direction.reversed());
					auto pointOnCircleOpt =
						circle.findIntersectionClosestToRayOrigin(rayFromLineSegmentToCircle);
					if (pointOnCircleOpt) {
						result = std::make_shared<SeparateCriticalPointPair>(
							*pointOnCircleOpt, closestVertex
						);
					}
				}
			}
		}

		return (result != nullptr) ? std::optional<std::shared_ptr<CriticalPointPair>>(result)
								   : std::nullopt;
	}

	static std::optional<std::shared_ptr<CriticalPointPair>> findConflictingCriticalPoints(
		const Circle& circle, const ColliderEdge& edge
	) {
		std::shared_ptr<CriticalPointPair> result = nullptr;

		Line2D line						   = edge.getLine();
		std::vector<Point2D> intersections = circle.findIntersection(line);

		std::vector<Point2D> pointsOnLineSegment;
		for (const auto& intersection : intersections) {
			if (edge.isPointOnLineSegment(intersection)) {
				pointsOnLineSegment.push_back(intersection);
			}
		}

		if (pointsOnLineSegment.size() == 1) {
			result = std::make_shared<TangentialCriticalPoint>(pointsOnLineSegment.at(0));
		} else if (pointsOnLineSegment.size() == 2) {
			result = std::make_shared<CuttingCriticalPointPair>(pointsOnLineSegment);
		}

		return (result != nullptr) ? std::optional<std::shared_ptr<CriticalPointPair>>(result)
								   : std::nullopt;
	}
};

struct Collision {
	std::shared_ptr<Collider> collider;
	ColliderEdge edge; // held by value because it's concrete
	std::shared_ptr<CriticalPointPair> contact;

	Collision(
		std::shared_ptr<Collider> c, const ColliderEdge& e,
		std::shared_ptr<CriticalPointPair> contact_
	) :
		collider(std::move(c)), edge(e), contact(std::move(contact_)) {}

	virtual ~Collision() = default;

	virtual std::shared_ptr<CriticalPointPair> getContact() const {
		return contact;
	}

	virtual double getTimeToCollision() const {
		return 0.0;
	}

	Vector2D getNormal() const {
		return collider->getNormalOf(edge);
	}

	const std::shared_ptr<Collider>& getCollider() const {
		return collider;
	}

	ColliderEdge getEdge() const {
		return edge;
	}

	virtual std::string toString() const {
		return "Collision";
	}
};

struct ProspectiveCollision : public Collision {
	std::shared_ptr<SeparateCriticalPointPair> getSeparateContact() const {
		return std::dynamic_pointer_cast<SeparateCriticalPointPair>(contact);
	}

	ProspectiveCollision(
		std::shared_ptr<Collider> c, const ColliderEdge& e,
		std::shared_ptr<SeparateCriticalPointPair> contact_
	) :
		Collision(std::move(c), e, std::move(contact_)) {}

	std::string toString() const override {
		return "ProspectiveCollision";
	}
};

struct PresentCollision : public Collision {
	PresentCollision(
		std::shared_ptr<Collider> c, const ColliderEdge& e,
		std::shared_ptr<CriticalPointPair> contact_
	) :
		Collision(std::move(c), e, std::move(contact_)) {}

	std::string toString() const override {
		return "PresentCollision";
	}
};

struct PotentialCollision : public ProspectiveCollision {
	PotentialCollision(
		std::shared_ptr<Collider> c, const ColliderEdge& e,
		std::shared_ptr<SeparateCriticalPointPair> contact_
	) :
		ProspectiveCollision(std::move(c), e, std::move(contact_)) {}

	std::string toString() const override {
		return "PotentialCollision";
	}
};

struct InevitableCollision : public ProspectiveCollision {
	double timeToCollision;

	double getTimeToCollision() const override {
		return timeToCollision;
	}

	InevitableCollision(
		std::shared_ptr<Collider> c, const ColliderEdge& e,
		std::shared_ptr<SeparateCriticalPointPair> contact_, double ttc
	) :
		ProspectiveCollision(std::move(c), e, std::move(contact_)), timeToCollision(ttc) {}

	std::string toString() const override {
		return "InevitableCollision with timeToCollision = " + std::to_string(timeToCollision);
	}
};

class CollisionConstructor {
public:
	CollisionConstructor(const Circle& circle, const Vector2D& velocity, double deltaTime) :
		velocity_(velocity),
		deltaTime_(deltaTime),
		speed_(velocity.length()),
		center_(circle.getCenter()) {}

	static bool isPointWithinCollisionTrajectory(
		const Point2D& pointOnCircle, const Point2D& test, const Vector2D& velocity
	) {
		Vector2D circleToTestPoint = test.subtract(pointOnCircle);
		double dot				   = Vector2D::dot(circleToTestPoint, velocity);
		return dot > 0;
	}

	std::optional<std::shared_ptr<Collision>> constructIfPossible(
		std::shared_ptr<Collider> collider, const ColliderEdge& edge,
		std::shared_ptr<CriticalPointPair> pair
	) {
		if (auto separate = std::dynamic_pointer_cast<SeparateCriticalPointPair>(pair)) {
			Point2D pointOnEdge = separate->getPointOnEdge();

			if (isPointWithinCollisionTrajectory(center_, pointOnEdge, velocity_)) {
				double distance			   = separate->getDistance();
				double timeToCollision	   = distance / speed_;
				bool isInevitableCollision = timeToCollision <= deltaTime_;

				if (isInevitableCollision) {
					return std::make_shared<InevitableCollision>(
						collider, edge, separate, timeToCollision
					);
				} else {
					return std::make_shared<PotentialCollision>(collider, edge, separate);
				}
			}
		} else {
			Vector2D normal = collider->getNormalOf(edge);
			bool colliding	= normal.dot(velocity_) < -Util::EPSILON;

			if (colliding) {
				return std::make_shared<PresentCollision>(collider, edge, pair);
			}
		}

		return std::nullopt;
	}

private:
	Vector2D velocity_;
	double deltaTime_;
	double speed_;
	Point2D center_;
};

struct Conflict : public Collision {
	// This class represents two cases below.
	//
	//         x   x
	//       x        x
	//      x   Ball   x
	//      x          x
	//       x        x
	//         x   x
	//    ----------------  Collider Edge
	//
	// or
	//
	//         x   x
	//       x        x
	//      x   Ball   x
	//      x          x
	//  ---------------------- Collider Edge
	//         x   x
	//
	//
	// PresentCollision is special case of a Conflict where the dot product of velocity and collider
	// normal is negative, in other words, Conflict is velocity ignorant while PresentCollision
	// concerns the direction of velocity and the normal of the collider.

	Conflict(
		const std::shared_ptr<Collider>& collider_, const ColliderEdge& edge_,
		const std::shared_ptr<CriticalPointPair>& contact_
	) :
		Collision(collider_, edge_, contact_) {}
};

class TickBase {
public:
	virtual ~TickBase()															 = default;
	virtual double getTimeSpent() const											 = 0;
	virtual void setSimulationTime(double time)									 = 0;
	virtual double getMinimumDistanceToCollision() const						 = 0;
	virtual const std::vector<std::shared_ptr<Collision>>& getCollisions() const = 0;

	virtual bool isStationary() const {
		return false;
	}

	virtual bool isFree() const {
		return false;
	}

	virtual bool isPaused() const {
		return false;
	}

	virtual bool isCrash() const {
		return false;
	}

	// Other common virtual functions if needed
};

class Tick : public TickBase {
protected:
	std::vector<std::shared_ptr<Collision>> collisions_;
	double timeSpent_;
	double minimumTimeToCollision_;
	double minimumDistanceToCollision_;
	int numberOfSeparateCriticalPointPairs_;
	int numberOfTangentialCriticalPoints_;
	int numberOfCuttingCriticalPointPairs_;

	double simulationTime_ = 0.0;

public:
	Tick(std::vector<std::shared_ptr<Collision>> collisions, double timeSpent) :
		collisions_(std::move(collisions)), timeSpent_(timeSpent) {
		minimumTimeToCollision_		= computeMinimumTimeToCollision();
		minimumDistanceToCollision_ = computeMinimumDistanceToCollision();

		numberOfSeparateCriticalPointPairs_ = 0;
		numberOfTangentialCriticalPoints_	= 0;
		numberOfCuttingCriticalPointPairs_	= 0;

		for (const auto& collision : collisions_) {
			auto pair = collision->getContact();

			if (dynamic_cast<SeparateCriticalPointPair*>(pair.get()) != nullptr) {
				++numberOfSeparateCriticalPointPairs_;
			}
			if (dynamic_cast<CuttingCriticalPointPair*>(pair.get()) != nullptr) {
				++numberOfTangentialCriticalPoints_;
			}
			if (dynamic_cast<TangentialCriticalPoint*>(pair.get()) != nullptr) {
				++numberOfCuttingCriticalPointPairs_;
			}
		}
	}

	virtual ~Tick() = default;

	// Accessors
	const std::vector<std::shared_ptr<Collision>>& getCollisions() const override {
		return collisions_;
	}

	double getTimeSpent() const {
		return timeSpent_;
	}

	double getMinimumTimeToCollision() const {
		return minimumTimeToCollision_;
	}

	double getMinimumDistanceToCollision() const override {
		return minimumDistanceToCollision_;
	}

	int getNumberOfSeparateCriticalPointPairs() const {
		return numberOfSeparateCriticalPointPairs_;
	}

	int getNumberOfTangentialCriticalPoints() const {
		return numberOfTangentialCriticalPoints_;
	}

	int getNumberOfCuttingCriticalPointPairs() const {
		return numberOfCuttingCriticalPointPairs_;
	}

	double getSimulationTime() const {
		return simulationTime_;
	}

	void setSimulationTime(double val) {
		simulationTime_ = val;
	}

	virtual std::string getChildName() const = 0;

	std::string toString() const {
		std::ostringstream oss;
		oss << getChildName() << "\n"
			<< "    # of Collisions             : " << collisions_.size() << "\n"
			<< "    Remaining Time to Collision : ";
		if (minimumTimeToCollision_ == std::numeric_limits<double>::max()) {
			oss << "N/A\n";
		} else {
			oss.precision(6);
			oss << std::fixed << (minimumTimeToCollision_ - timeSpent_) << "\n";
		}
		oss << "    Time Spent                  : " << std::fixed << std::setprecision(6)
			<< timeSpent_ << "\n"
			<< "    Simulation Time             : " << std::fixed << std::setprecision(6)
			<< simulationTime_ << "\n"
			<< "    # of Separate CPs           : " << numberOfSeparateCriticalPointPairs_ << "\n"
			<< "    # of Tangential CPs         : " << numberOfTangentialCriticalPoints_ << "\n"
			<< "    # of Cutting CPs            : " << numberOfCuttingCriticalPointPairs_;
		return oss.str();
	}

private:
	double computeMinimumTimeToCollision() const {
		// Filter InevitableCollision and find the minimum timeToCollision
		std::vector<std::shared_ptr<Collision>> inevs;

		for (const auto& collision : collisions_) {
			auto inev = std::dynamic_pointer_cast<Collision>(collision);
			if (inev) {
				inevs.push_back(inev);
			}
		}

		if (inevs.empty()) {
			return std::numeric_limits<double>::max();
		}

		auto minIter = std::min_element(
			inevs.begin(), inevs.end(),
			[](const std::shared_ptr<Collision>& a, const std::shared_ptr<Collision>& b) {
				return a->getTimeToCollision() < b->getTimeToCollision();
			}
		);

		return (*minIter)->getTimeToCollision();
	}

	double computeMinimumDistanceToCollision() const {
		if (collisions_.empty()) {
			return std::numeric_limits<double>::max();
		}

		auto minIter = std::min_element(
			collisions_.begin(), collisions_.end(),
			[](const std::shared_ptr<Collision>& a, const std::shared_ptr<Collision>& b) {
				return a->getContact()->getDistance() < b->getContact()->getDistance();
			}
		);

		return (*minIter)->getContact()->getDistance();
	}
};

class StationaryTick : public Tick {
public:
	StationaryTick(std::vector<std::shared_ptr<Collision>> collisions, double timeSpent) :
		Tick(std::move(collisions), timeSpent) {}

	virtual bool isStationary() const override {
		return true;
	}

	std::string getChildName() const override {
		return "Stationary Tick";
	}
};

class PausedTick : public Tick {
public:
	PausedTick() : Tick(std::vector<std::shared_ptr<Collision>>(), 0.0) {}

	virtual bool isPaused() const override {
		return true;
	}

	std::string getChildName() const override {
		return "Paused Tick";
	}
};

class FreeTick : public Tick {
public:
	FreeTick(std::vector<std::shared_ptr<Collision>> collisions, double timeSpent) :
		Tick(std::move(collisions), timeSpent) {}

	virtual bool isFree() const override {
		return true;
	}

	std::string getChildName() const override {
		return "Free Tick";
	}
};

class CrashTick : public Tick {
private:
	Vector2D normal_;

public:
	virtual bool isCrash() const override {
		return true;
	}

	CrashTick(
		std::vector<std::shared_ptr<Collision>> collisions, Vector2D normal, double timeSpent
	) :
		Tick(std::move(collisions), timeSpent), normal_(normal) {}

	const Vector2D& getNormal() const {
		return normal_;
	}

	std::string getChildName() const override {
		return "Crash Tick";
	}
};

class Ball : public DrawableCircle, public Draggable, public GameObject {
public:
	const Vector2D& getVelocity() const {
		return velocity;
	}

	const Vector2D& getNetForce() const {
		return netForce;
	}

	void setNetForce(const Vector2D& net_force) {
		netForce = net_force;
	}

	void setVelocity(const Vector2D& velocity_) {
		velocity = velocity_;
	}

	bool isFreeze() const {
		return freeze;
	}

	void setFreeze(bool new_freeze) {
		freeze = new_freeze;
	}

	Vector2D velocity;
	Vector2D netForce = Vector2D::ZERO;
	bool freeze		  = false;

	Ball(const Point2D& center, double radius, const Vector2D& velocity, const ptgn::Color& color) :
		DrawableCircle(center, radius, color), velocity(velocity) {}

	// Moves ball based on current velocity
	void move(double deltaTime) {
		if (freeze) {
			return;
		}

		auto dx = velocity.multiply(deltaTime);
		center	= center.add(dx);

		if (Util::isFuzzyZero(velocity.length())) {
			velocity = Vector2D::ZERO;
		}
	}

	// Slides based on net force and deceleration model
	void slide(const Vector2D& netForce_, double deltaTime) {
		if (freeze) {
			return;
		}

		double speed	  = velocity.length();
		double dotProduct = netForce_.dot(velocity);

		if (dotProduct >= 0.0) {
			move(netForce_, deltaTime); // same direction or zero
		} else {
			double netMag		 = netForce_.length();
			double timeUntilStop = speed / netMag;
			move(netForce_, std::min(timeUntilStop, deltaTime));
		}
	}

	// Moves and updates velocity under acceleration
	void move(const Vector2D& acceleration, double deltaTime) {
		if (freeze) {
			return;
		}

		auto dx = velocity.multiply(deltaTime);
		center	= center.add(dx);

		auto dv	 = acceleration.multiply(deltaTime);
		velocity = velocity.add(dv);

		if (Util::isFuzzyZero(velocity.length())) {
			velocity = Vector2D::ZERO;
		}
	}

	// Reflects velocity over a normal vector
	void collide(const Vector2D& normal) {
		if (freeze) {
			return;
		}
		velocity = velocity.reflect(normal);
	}

	// Reflects velocity with restitution and friction
	void collide(const Vector2D& normal, double restitution, double friction) {
		if (freeze) {
			return;
		}

		Vector2D vertical	= velocity.projectOnto(normal);
		Vector2D horizontal = velocity.rejectionOf(normal);

		vertical   = vertical.multiply(1.0 - restitution);
		horizontal = horizontal.multiply(1.0 - friction);

		if (vertical.length() < Constants::Ball::DO_NOT_BOUNCE_SPEED_THRESHOLD) {
			vertical = Vector2D::ZERO;
		}
		if (horizontal.length() < Constants::Ball::DO_NOT_BOUNCE_SPEED_THRESHOLD) {
			horizontal = Vector2D::ZERO;
		}

		velocity = vertical.reversed().add(horizontal);

		if (Util::isFuzzyZero(velocity.length())) {
			velocity = Vector2D::ZERO;
		}
	}

	// Move in a direction by a distance
	void translate(const Vector2D& direction, double distance) {
		center = center.add(direction.multiply(distance));
	}

	// Translate using delta point
	void translate(const Point2D& delta) override {
		center = center.add(delta);
	}

	// Set exact center
	void setCenter(const Point2D& newCenter) {
		center = newCenter;
	}

	// Check if query is inside ball
	bool contains(const Point2D& query) const override {
		return isPointInsideCircle(query);
	}

	// Contains with tolerance radius scaling
	bool contains(const Point2D& query, double tolerance) const {
		double dx			 = center.getX() - query.getX();
		double dy			 = center.getY() - query.getY();
		double distanceSq	 = dx * dx + dy * dy;
		double maxDistanceSq = tolerance * tolerance * radius * radius;
		return Util::isBetween(0.0, distanceSq, maxDistanceSq);
	}

	// Clone the ball
	std::shared_ptr<Ball> copy() const {
		return std::make_shared<Ball>(center, radius, velocity, getColor());
	}

	// Get speed magnitude
	double getSpeed() const {
		return velocity.length();
	}

	// Check if velocity is near-zero
	bool isStationary() const {
		return Util::isFuzzyZero(getSpeed());
	}
};

class Brick : public RectangularNode, public Collider {
private:
	bool hit = false;

public:
	Brick(double x, double y, double width, double height, const ptgn::Color& color) :
		RectangularNode(x, y, width, height, color) {}

	std::vector<ColliderEdge> getEdges() const override {
		return RectangularNode::getEdges();
	}

	// Collider
	bool isActiveCollider() const override {
		return !hit;
	}

	// Drawable
	bool isActiveDrawable() const override {
		return !hit;
	}

	double getFrictionCoefficient() const override {
		return Constants::Brick::FRICTION_COEFFICIENT;
	}

	Vector2D getNormalOf(const LineSegment2D& edge) const override {
		return edge.getNormal(LineSegment2D::NormalOrientation::OUTWARDS);
	}

	// Accessor and mutator for hit
	bool isHit() const {
		return hit;
	}

	void setHit(bool value) {
		hit = value;
	}
};

class Obstacle : public PolygonalNode, public Draggable, public Collider {
public:
	// Constructors matching Java constructors
	Obstacle(
		const std::vector<Point2D>& vertices, const std::vector<std::string>& identifiers,
		const ptgn::Color& color
	) :
		PolygonalNode(vertices, identifiers, color) {}

	Obstacle(const std::vector<Point2D>& vertices, const ptgn::Color& color) :
		PolygonalNode(vertices, color) {}

	Obstacle(double x, double y, double width, double height, const ptgn::Color& color) :
		PolygonalNode(
			std::vector<Point2D>{ Point2D(x, y), Point2D(x, y + height),
								  Point2D(x + width, y + height), Point2D(x + width, y) },
			color
		) {}

	virtual bool contains(const Point2D& query) const {
		return PolygonalNode::contains(query);
	}

	virtual void translate(const Point2D& delta) {
		PolygonalNode::translate(delta);
	}

	std::vector<ColliderEdge> getEdges() const override {
		return PolygonalNode::getEdges();
	}

	// Collider interface
	double getFrictionCoefficient() const override {
		return Constants::Obstacle::FRICTION_COEFFICIENT;
	}

	Vector2D getNormalOf(const LineSegment2D& edge) const override {
		return edge.getNormal(LineSegment2D::NormalOrientation::OUTWARDS);
	}
};

class Paddle : public RectangularNode, public Draggable, public Collider {
private:
	bool isActiveCollider  = true;
	bool isActiveDraggable = true;

public:
	Paddle(double x, double y, double width, double height, const ptgn::Color& color) :
		RectangularNode(x, y, width, height, color) {}

	std::vector<ColliderEdge> getEdges() const override {
		return RectangularNode::getEdges();
	}

	virtual bool contains(const Point2D& query) const {
		return RectangularNode::contains(query);
	}

	virtual void translate(const Point2D& delta) {
		RectangularNode::translate(delta);
	}

	// Accessors and mutators for isActiveCollider
	bool getIsActiveCollider() const {
		return isActiveCollider;
	}

	void setIsActiveCollider(bool active) {
		isActiveCollider = active;
	}

	// Accessors and mutators for isActiveDraggable
	bool getIsActiveDraggable() const {
		return isActiveDraggable;
	}

	void setIsActiveDraggable(bool active) {
		isActiveDraggable = active;
	}

	// Collider interface implementation
	double getFrictionCoefficient() const override {
		return Constants::Paddle::FRICTION_COEFFICIENT;
	}

	Vector2D getNormalOf(const LineSegment2D& edge) const override {
		return edge.getNormal(LineSegment2D::NormalOrientation::OUTWARDS);
	}
};

class World : public RectangularNode, public Collider {
public:
	World(double x, double y, double width, double height, const ptgn::Color& color) :
		RectangularNode(x, y, width, height, color) {}

	std::vector<ColliderEdge> getEdges() const override {
		return RectangularNode::getEdges();
	}

	double getFrictionCoefficient() const override {
		return Constants::World::FRICTION_COEFFICIENT;
	}

	Vector2D getNormalOf(const LineSegment2D& edge) const override {
		return edge.getNormal(LineSegment2D::NormalOrientation::INWARDS);
	}
};

class Canvas {
public:
	virtual ~Canvas()				 = default;
	virtual double getWidth() const	 = 0;
	virtual double getHeight() const = 0;
};

class TransformationHelper {
public:
	static void initialize(std::shared_ptr<World> world, std::shared_ptr<Canvas> node) {
		impl = std::make_unique<TransformationHelperInner>(world, node);
	}

	static Point2D fromWorldToCanvas(double x, double y) {
		return impl->fromWorldToCanvas(x, y);
	}

	static Point2D fromWorldToCanvas(const Point2D& p) {
		return impl->fromWorldToCanvas(p);
	}

	static Point2D fromCanvasToWorld(double x, double y) {
		return impl->fromCanvasToWorld(x, y);
	}

	static Point2D fromCanvasToWorld(const Point2D& p) {
		return impl->fromCanvasToWorld(p);
	}

	static Point2D getCanvasCenter() {
		return impl->getCanvasCenter();
	}

	static Point2D getWorldCenter() {
		return impl->getWorldCenter();
	}

private:
	class TransformationHelperInner {
	public:
		TransformationHelperInner(std::shared_ptr<World> world_, std::shared_ptr<Canvas> node_) :
			world(world_), node(node_) {}

		Point2D fromWorldToCanvas(double x, double y) const {
			double ww = world->getWidth();
			double wh = world->getHeight();
			double gw = node->getWidth();
			double gh = node->getHeight();

			// 0 to 1
			double nx = x / ww;
			double ny = y / wh;

			return Point2D(nx * gw, ny * gh);
		}

		Point2D fromWorldToCanvas(const Point2D& p) const {
			return fromWorldToCanvas(p.getX(), p.getY());
		}

		Point2D fromCanvasToWorld(double x, double y) const {
			double ww = world->getWidth();
			double wh = world->getHeight();
			double gw = node->getWidth();
			double gh = node->getHeight();

			// 0 to 1
			double nx = x / gw;
			double ny = y / gh;

			return Point2D(nx * ww, ny * wh);
		}

		Point2D fromCanvasToWorld(const Point2D& p) const {
			return fromCanvasToWorld(p.getX(), p.getY());
		}

		Point2D getWorldCenter() const {
			return Point2D(0.5 * world->getWidth(), 0.5 * world->getHeight());
		}

		Point2D getCanvasCenter() const {
			return Point2D(0.5 * node->getWidth(), 0.5 * node->getHeight());
		}

	private:
		std::shared_ptr<World> world;
		std::shared_ptr<Canvas> node;
	};

	static std::unique_ptr<TransformationHelperInner> impl;
};

std::unique_ptr<TransformationHelper::TransformationHelperInner> TransformationHelper::impl =
	nullptr;

class GameObjects {
private:
	std::shared_ptr<World> world;
	std::vector<std::shared_ptr<Brick>> bricks;
	std::vector<std::shared_ptr<Obstacle>> obstacles;
	std::shared_ptr<Ball> ball;
	std::shared_ptr<Paddle> paddle;

	// Polymorphic interface collections use shared_ptr to base classes
	std::vector<std::shared_ptr<Collider>> colliders;
	std::vector<std::shared_ptr<Draggable>> draggables;

public:
	GameObjects(
		std::shared_ptr<World> w, std::vector<std::shared_ptr<Brick>> b,
		std::vector<std::shared_ptr<Obstacle>> o, std::shared_ptr<Ball> ba,
		std::shared_ptr<Paddle> p
	) :
		world(std::move(w)),
		bricks(std::move(b)),
		obstacles(std::move(o)),
		ball(std::move(ba)),
		paddle(std::move(p)) {
		colliders.emplace_back(world);

		for (auto& brick : bricks) {
			colliders.emplace_back(brick);
		}
		for (auto& obstacle : obstacles) {
			colliders.emplace_back(obstacle);
		}
		colliders.emplace_back(paddle);

		for (auto& obstacle : obstacles) {
			draggables.emplace_back(obstacle);
		}
		draggables.emplace_back(paddle);
	}

	// Getters
	std::shared_ptr<World> getWorld() const {
		return world;
	}

	const std::vector<std::shared_ptr<Brick>>& getBricks() const {
		return bricks;
	}

	const std::vector<std::shared_ptr<Obstacle>>& getObstacles() const {
		return obstacles;
	}

	std::shared_ptr<Ball> getBall() const {
		return ball;
	}

	std::shared_ptr<Paddle> getPaddle() const {
		return paddle;
	}

	const std::vector<std::shared_ptr<Collider>>& getColliders() const {
		return colliders;
	}

	const std::vector<std::shared_ptr<Draggable>>& getDraggables() const {
		return draggables;
	}
};

class GameObjectConstructor {
public:
	static GameObjects construct(bool isDebugMode) {
		auto world = std::make_shared<World>(
			0, 0, window_size.x, window_size.y, Constants::World::BACKGROUND_COLOR
		);

		auto ball	= std::make_shared<Ball>(constructBall());
		auto paddle = std::make_shared<Paddle>(constructPaddle());

		std::vector<std::shared_ptr<Brick>> bricks;
		std::vector<std::shared_ptr<Obstacle>> obstacles;

		if (isDebugMode) {
			bricks = {}; // empty
			paddle->setIsActiveDrawable(false);
			paddle->setIsActiveCollider(false);
			paddle->setIsActiveDraggable(false);
			obstacles = constructObstacles();
		} else {
			bricks	  = constructBricks(8, 12);
			obstacles = {}; // empty
		}

		return GameObjects(world, bricks, obstacles, ball, paddle);
	}

private:
	static Paddle constructPaddle() {
		return Paddle(
			Constants::Paddle::INITIAL_X, Constants::Paddle::INITIAL_Y, Constants::Paddle::WIDTH,
			Constants::Paddle::HEIGHT, Constants::Paddle::COLOR
		);
	}

	static Ball constructBall() {
		Point2D center(Constants::Ball::INITIAL_X, Constants::Ball::INITIAL_Y);
		double speed =
			RandomGenerator::nextDouble(Constants::Ball::MIN_SPEED, Constants::Ball::MAX_SPEED);
		Vector2D velocity = RandomGenerator::generateRandomVelocity(speed);
		return Ball(center, Constants::Ball::RADIUS, velocity, Constants::Ball::COLOR);
	}

	static std::vector<std::shared_ptr<Brick>> constructBricks(int rows, int columns) {
		std::vector<std::shared_ptr<Brick>> bricks;
		double totalWidth =
			columns * (Constants::Brick::WIDTH + Constants::Brick::HORIZONTAL_SPACING) -
			Constants::Brick::HORIZONTAL_SPACING;
		double left = 0.5 * (window_size.x - totalWidth);

		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < columns; ++j) {
				double x =
					j * (Constants::Brick::WIDTH + Constants::Brick::HORIZONTAL_SPACING) + left;
				double y = i * (Constants::Brick::HEIGHT + Constants::Brick::VERTICAL_SPACING) +
						   Constants::World::TOP_PADDING;
				ptgn::Color color = Constants::Brick::COLORS_PER_ROW.count(i) > 0
									  ? Constants::Brick::COLORS_PER_ROW.at(i)
									  : ptgn::color::White;

				bricks.emplace_back(std::make_shared<Brick>(
					x, y, Constants::Brick::WIDTH, Constants::Brick::HEIGHT, color
				));
			}
		}

		return bricks;
	}

	static std::vector<std::shared_ptr<Obstacle>> constructObstacles() {
		std::vector<std::shared_ptr<Obstacle>> obstacles;

		obstacles.emplace_back(std::make_shared<Obstacle>(
			std::vector<Point2D>{ Point2D(0, 720), Point2D(640, 720), Point2D(0, 500) },
			ptgn::color::White
		));

		obstacles.emplace_back(std::make_shared<Obstacle>(
			std::vector<Point2D>{ Point2D(640, 720), Point2D(1280, 720), Point2D(1280, 500) },
			ptgn::color::White
		));

		obstacles.emplace_back(std::make_shared<Obstacle>(200, 250, 100, 100, ptgn::color::White));
		obstacles.emplace_back(std::make_shared<Obstacle>(980, 250, 100, 100, ptgn::color::White));

		return obstacles;
	}
};

class CollisionEngine {
private:
	std::vector<std::shared_ptr<Collider>> colliders_;
	std::shared_ptr<Ball> ball_;

public:
	CollisionEngine(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball
	) :
		colliders_(colliders), ball_(ball) {}

	static std::vector<std::shared_ptr<Conflict>> findConflicts(
		const std::vector<std::shared_ptr<Collider>>& colliders, const Circle& circle
	) {
		std::vector<std::shared_ptr<Conflict>> conflicts;

		for (const auto& collider : colliders) {
			if (!collider->isActiveCollider()) {
				continue;
			}

			const auto& edges = collider->getEdges();

			for (const auto& edge : edges) {
				auto criticalOpt = CriticalPointFinder::findConflictingCriticalPoints(circle, edge);
				if (criticalOpt) {
					conflicts.push_back(std::make_shared<Conflict>(collider, edge, *criticalOpt));
				}
			}
		}

		return conflicts;
	}

	static std::vector<std::shared_ptr<Collision>> findCollisions(
		const std::vector<std::shared_ptr<Collider>>& colliders, const Circle& circle,
		const Vector2D& velocity, double deltaTime
	) {
		CollisionConstructor ctor(circle, velocity, deltaTime);
		std::vector<std::shared_ptr<Collision>> collisions;

		for (const auto& collider : colliders) {
			if (!collider->isActiveCollider()) {
				continue;
			}

			const auto& edges = collider->getEdges();

			for (const auto& edge : edges) {
				auto criticalOpt = CriticalPointFinder::findCriticalPointsAlongGivenDirection(
					circle, edge, velocity
				);
				if (criticalOpt) {
					auto collisionOpt = ctor.constructIfPossible(collider, edge, *criticalOpt);
					if (collisionOpt) {
						collisions.push_back(*collisionOpt);
					}
				}
			}
		}

		return collisions;
	}

	static std::vector<std::shared_ptr<CriticalPointPair>> findCriticalPointsAlongGivenDirection(
		const Circle& circle, const std::shared_ptr<Collider>& collider, const Vector2D& direction
	) {
		std::vector<std::shared_ptr<CriticalPointPair>> result;
		const auto& edges	  = collider->getEdges();
		const Point2D& center = circle.getCenter();

		for (const auto& edge : edges) {
			auto criticalOpt =
				CriticalPointFinder::findCriticalPointsAlongGivenDirection(circle, edge, direction);
			if (criticalOpt) {
				if (CollisionConstructor::isPointWithinCollisionTrajectory(
						center, (*criticalOpt)->getPointOnEdge(), direction
					)) {
					result.push_back(*criticalOpt);
				}
			}
		}

		return result;
	}

	// Finds the closest CriticalPointPair along a direction
	static std::optional<std::shared_ptr<CriticalPointPair>>
	findMostCriticalPointAlongGivenDirection(
		const Circle& circle, const std::shared_ptr<Collider>& collider, const Vector2D& direction
	) {
		auto criticalPoints = findCriticalPointsAlongGivenDirection(circle, collider, direction);

		std::sort(
			criticalPoints.begin(), criticalPoints.end(),
			[](const std::shared_ptr<CriticalPointPair>& p0,
			   const std::shared_ptr<CriticalPointPair>& p1) {
				return p0->getDistance() < p1->getDistance();
			}
		);

		if (criticalPoints.empty()) {
			return std::nullopt;
		}
		return criticalPoints.front();
	}

	template <typename T>
	static void sortEarliestToLatest(std::vector<std::shared_ptr<T>>& collisions) {
		std::sort(
			collisions.begin(), collisions.end(),
			[](const std::shared_ptr<T>& c0, const std::shared_ptr<T>& c1) {
				return c0->getTimeToCollision() < c1->getTimeToCollision();
			}
		);
	}

	template <typename T>
	static Vector2D calculateCollectiveCollisionNormal(
		const std::vector<std::shared_ptr<T>>& collisions, const Vector2D& velocity
	) {
		if (velocity.l2normValue() == 0.0) {
			throw std::invalid_argument("velocity must be non-zero vector!");
		}

		Vector2D result(0, 0);

		for (const auto& collision : collisions) {
			Vector2D normal = collision->getNormal();
			if (normal.dot(velocity) < -Util::EPSILON) {
				result = result.add(normal);
			}
		}

		return result.normalized();
	}

	std::vector<std::shared_ptr<Collision>> findCollisions(double deltaTime) const {
		return findCollisions(colliders_, *ball_, ball_->getVelocity(), deltaTime);
	}
};

// Enum for result types
enum class ResultType {
	APPLY_NET_FORCE,
	BALL_IS_SLIDING,
	BALL_IS_AT_EQUILIBRIUM,
	BALL_IS_AT_CORNER
};

// Result class to hold the result type and net force vector
struct Result {
	ResultType type;
	Vector2D netForce;

	Result(ResultType t, const Vector2D& force) : type(t), netForce(force) {}
};

// The main NetForceCalculator class
class NetForceCalculator {
private:
	const std::vector<std::shared_ptr<Collider>>& colliders_;
	Vector2D gravity_;
	double tolerance_;
	Ray2D rayFromCenterToGround_;

public:
	explicit NetForceCalculator(const std::vector<std::shared_ptr<Collider>>& colliders) :
		colliders_(colliders),
		gravity_(Vector2D(0.0, Constants::Physics::GRAVITY)),
		tolerance_(Constants::Physics::NET_FORCE_CALCULATOR_TOLERANCE) {}

	Result process(std::shared_ptr<Ball> ball, double deltaTime);

private:
	Result calculate(std::shared_ptr<Ball> ball);

	Result resolveSingleGravitySubject(
		const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
	);

	Result resolveMultipleGravitySubjects(
		const std::vector<std::shared_ptr<Conflict>>& conflicts, std::shared_ptr<Ball> ball
	);

	Result createSlidingResult(
		const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
	);

	Result createBallAtCornerResult(
		const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
	);

	bool isAtEquilibrium(
		const std::vector<std::shared_ptr<Conflict>>& conflicts, std::shared_ptr<Ball> ball
	);

	bool isBallAtCorner(const std::shared_ptr<Conflict>& conflict);
};

// Implementations

Result NetForceCalculator::process(std::shared_ptr<Ball> ball, double deltaTime) {
	Result result	  = calculate(ball);
	Vector2D netForce = result.netForce;

	switch (result.type) {
		case ResultType::APPLY_NET_FORCE: ball->move(netForce, deltaTime); break;
		case ResultType::BALL_IS_SLIDING: ball->slide(netForce, deltaTime); break;
		case ResultType::BALL_IS_AT_EQUILIBRIUM:
			ball->move(deltaTime); // move with current velocity
			break;
		case ResultType::BALL_IS_AT_CORNER: ball->move(netForce, deltaTime); break;
	}

	ball->setNetForce(netForce);
	return result;
}

Result NetForceCalculator::calculate(std::shared_ptr<Ball> ball) {
	tolerance_			   = Constants::Physics::NET_FORCE_CALCULATOR_TOLERANCE;
	rayFromCenterToGround_ = Ray2D(ball->getCenter(), gravity_);

	// enlarge ball by tolerance for collision checking
	auto conflicts = CollisionEngine::findConflicts(colliders_, ball->enlarge(tolerance_));

	std::vector<std::shared_ptr<Conflict>> gravitySubjects;
	for (const auto& conflict : conflicts) {
		if (gravity_.dot(conflict->getNormal()) <= 0.0) {
			gravitySubjects.push_back(conflict);
		}
	}

	if (gravitySubjects.empty()) {
		return Result(ResultType::APPLY_NET_FORCE, gravity_);
	} else if (gravitySubjects.size() == 1) {
		return resolveSingleGravitySubject(gravitySubjects.at(0), ball);
	} else {
		return resolveMultipleGravitySubjects(gravitySubjects, ball);
	}
}

Result NetForceCalculator::resolveSingleGravitySubject(
	const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
) {
	Vector2D velocity = ball->getVelocity();
	Vector2D normal	  = conflict->getNormal();

	bool sliding = Util::isFuzzyZero(velocity.dot(normal));

	if (sliding) {
		return createSlidingResult(conflict, ball);
	} else {
		if (ball->isStationary()) {
			if (isBallAtCorner(conflict)) {
				return createBallAtCornerResult(conflict, ball);
			} else {
				return Result(ResultType::BALL_IS_AT_EQUILIBRIUM, Vector2D::ZERO);
			}
		}
		Vector2D netForce = gravity_.rejectionOf(normal);
		return Result(ResultType::APPLY_NET_FORCE, netForce);
	}
}

Result NetForceCalculator::resolveMultipleGravitySubjects(
	const std::vector<std::shared_ptr<Conflict>>& conflicts, std::shared_ptr<Ball> ball
) {
	if (isAtEquilibrium(conflicts, ball)) {
		return Result(ResultType::BALL_IS_AT_EQUILIBRIUM, Vector2D::ZERO);
	}

	std::vector<std::shared_ptr<Conflict>> cornerSubjects;
	for (const auto& conflict : conflicts) {
		if (isBallAtCorner(conflict)) {
			cornerSubjects.push_back(conflict);
		}
	}

	if (cornerSubjects.empty()) {
		return Result(ResultType::BALL_IS_AT_EQUILIBRIUM, Vector2D::ZERO);
	} else if (cornerSubjects.size() == 1) {
		return createBallAtCornerResult(cornerSubjects.at(0), ball);
	} else {
		if (isAtEquilibrium(cornerSubjects, ball)) {
			return Result(ResultType::BALL_IS_AT_EQUILIBRIUM, Vector2D::ZERO);
		} else {
			// Sort cornerSubjects by projected distance along gravity vector
			Point2D center	= ball->getCenter();
			Vector2D ground = gravity_.normal();

			auto sorted = cornerSubjects;
			std::sort(
				sorted.begin(), sorted.end(),
				[&](const std::shared_ptr<Conflict>& c0, const std::shared_ptr<Conflict>& c1) {
					Vector2D centerToContact0 = c0->getContact()->getPointOnEdge().subtract(center);
					Vector2D centerToContact1 = c1->getContact()->getPointOnEdge().subtract(center);

					Vector2D p0 = centerToContact0.projectOnto(ground);
					Vector2D p1 = centerToContact1.projectOnto(ground);

					return p0.length() < p1.length();
				}
			);

			return createBallAtCornerResult(sorted.at(0), ball);
		}
	}
}

Result NetForceCalculator::createSlidingResult(
	const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
) {
	Vector2D velocity						  = ball->getVelocity();
	const std::shared_ptr<Collider>& collider = conflict->getCollider();
	Vector2D normal							  = conflict->getNormal();

	double frictionCoefficient = collider->getFrictionCoefficient();
	double resistanceMagnitude = gravity_.projectOnto(normal).length() * frictionCoefficient;

	Vector2D resistance = velocity.normalized().reversed().multiply(resistanceMagnitude);
	Vector2D rejection	= gravity_.rejectionOf(normal);
	Vector2D netForce	= rejection.add(resistance);

	return Result(ResultType::BALL_IS_SLIDING, netForce);
}

Result NetForceCalculator::createBallAtCornerResult(
	const std::shared_ptr<Conflict>& conflict, std::shared_ptr<Ball> ball
) {
	Point2D pointOnEdge	  = conflict->getContact()->getPointOnEdge();
	Vector2D circleToEdge = pointOnEdge.subtract(ball->getCenter());
	Vector2D projection	  = gravity_.projectOnto(circleToEdge);
	Vector2D netForce	  = gravity_.rejectionOf(circleToEdge).add(projection.reversed());

	return Result(ResultType::BALL_IS_AT_CORNER, netForce);
}

bool NetForceCalculator::isAtEquilibrium(
	const std::vector<std::shared_ptr<Conflict>>& conflicts, std::shared_ptr<Ball> ball
) {
	Point2D center	= ball->getCenter();
	Vector2D ground = gravity_.normal();

	for (const auto& conflict : conflicts) {
		bool ballIsOnCollider =
			rayFromCenterToGround_.findIntersection(conflict->getEdge()).has_value();
		if (ballIsOnCollider) {
			Vector2D rejection = gravity_.rejectionOf(conflict->getNormal());
			if (Util::isFuzzyZero(rejection.l2normValue())) {
				return true;
			}
		}
	}

	for (size_t i = 0; i < conflicts.size(); ++i) {
		for (size_t j = i + 1; j < conflicts.size(); ++j) {
			auto contact0 = conflicts.at(i)->getContact();
			auto contact1 = conflicts.at(j)->getContact();

			Vector2D centerToContact0 = contact0->getPointOnEdge().subtract(center);
			Vector2D centerToContact1 = contact1->getPointOnEdge().subtract(center);

			Vector2D p0 = centerToContact0.projectOnto(ground);
			Vector2D p1 = centerToContact1.projectOnto(ground);

			if (p0.dot(p1) < 0) {
				return true;
			}
		}
	}
	return false;
}

bool NetForceCalculator::isBallAtCorner(const std::shared_ptr<Conflict>& conflict) {
	return !rayFromCenterToGround_.findIntersection(conflict->getEdge()).has_value();
}

template <typename T>
class CollisionResolver {
protected:
	const std::vector<std::shared_ptr<Collider>>& colliders_;
	std::shared_ptr<Ball> ball_;
	bool isDebugMode_;

	std::vector<std::shared_ptr<Collision>> presents_;
	std::vector<std::shared_ptr<Collision>> inevitables_;
	std::vector<std::shared_ptr<Collision>> potentials_;

	NetForceCalculator netForceCalculator_;

public:
	CollisionResolver(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		colliders_(colliders),
		ball_(std::move(ball)),
		isDebugMode_(isDebugMode),
		netForceCalculator_(colliders) {}

	virtual ~CollisionResolver() = default;

	void load(const std::vector<std::shared_ptr<Collision>>& collisions) {
		presents_.clear();
		inevitables_.clear();
		potentials_.clear();

		for (const auto& collision : collisions) {
			if (auto present = std::dynamic_pointer_cast<PresentCollision>(collision)) {
				presents_.push_back(present);
			} else if (auto inevitable =
						   std::dynamic_pointer_cast<InevitableCollision>(collision)) {
				inevitables_.push_back(inevitable);
			} else if (auto potential = std::dynamic_pointer_cast<PotentialCollision>(collision)) {
				potentials_.push_back(potential);
			} else {
				throw std::runtime_error("Implement this branch!");
			}
		}
	}

	// Pure virtual methods to be implemented by subclasses
	virtual bool isApplicable() const = 0;

	// Use covariant return type if needed; otherwise, return Tick<T>
	virtual std::shared_ptr<Tick> resolve(double deltaTime) = 0;
};

class PresentCollisionResolver : public CollisionResolver<PresentCollision> {
public:
	PresentCollisionResolver(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		CollisionResolver(colliders, ball, isDebugMode) {}

	bool isApplicable() const override {
		return !presents_.empty() || ball_->isStationary();
	}

	std::shared_ptr<Tick> resolve(double deltaTime) override {
		if (ball_->isStationary()) {
			auto result = netForceCalculator_.process(ball_, deltaTime);
			return std::make_shared<StationaryTick>(presents_, deltaTime);
		}

		auto target		= presents_.front();
		Vector2D normal = target->getNormal();

		if (isDebugMode_) {
			ball_->collide(
				normal, Constants::Ball::RESTITUTION_FACTOR,
				target->getCollider()->getFrictionCoefficient()
			);
		} else {
			ball_->collide(normal);
		}

		return std::make_shared<CrashTick>(presents_, normal, 0.0);
	}
};

class PotentialCollisionResolver : public CollisionResolver<PotentialCollision> {
public:
	PotentialCollisionResolver(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		CollisionResolver(colliders, ball, isDebugMode) {}

	bool isApplicable() const override {
		return true;
	}

	std::shared_ptr<Tick> resolve(double deltaTime) override {
		if (isDebugMode_) {
			auto result = netForceCalculator_.process(ball_, deltaTime);
		} else {
			ball_->move(deltaTime);
		}

		return std::make_shared<FreeTick>(potentials_, deltaTime);
	}
};

class InevitableCollisionResolver : public CollisionResolver<InevitableCollision> {
public:
	InevitableCollisionResolver(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		CollisionResolver(colliders, ball, isDebugMode) {}

	bool isApplicable() const override {
		return !inevitables_.empty();
	}

	std::shared_ptr<Tick> resolve(double deltaTime) override {
		if (inevitables_.empty()) {
			throw std::runtime_error("Inevitable collisions empty on resolve call");
		}

		// Sort by time to collision ascending
		std::vector<std::shared_ptr<Collision>> sorted = inevitables_;
		std::sort(
			sorted.begin(), sorted.end(),
			[](const std::shared_ptr<Collision>& a, const std::shared_ptr<Collision>& b) {
				return a->getTimeToCollision() < b->getTimeToCollision();
			}
		);

		auto earliest = sorted.front();
		auto collider = earliest->getCollider();

		// Filter collisions with same collider
		std::vector<std::shared_ptr<Collision>> filtered;
		std::copy_if(
			sorted.begin(), sorted.end(), std::back_inserter(filtered),
			[&](const std::shared_ptr<Collision>& collision) {
				return collision->getCollider() == collider;
			}
		);

		double ttc		  = earliest->getTimeToCollision();
		Vector2D velocity = ball_->getVelocity();
		Vector2D normal	  = CollisionEngine::calculateCollectiveCollisionNormal(filtered, velocity);

		if (isDebugMode_) {
			auto result = netForceCalculator_.process(ball_, ttc);

			if (normal.l2normValue() != 0) {
				ball_->collide(
					normal, Constants::Ball::RESTITUTION_FACTOR, collider->getFrictionCoefficient()
				);
			}
		} else {
			ball_->move(deltaTime);
			ball_->collide(normal);
		}

		return std::make_shared<CrashTick>(filtered, normal, ttc);
	}
};

class TickProcessor {
public:
	TickProcessor(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		collisionEngine_(colliders, ball),
		inevitableCollisionResolver_(colliders, ball, isDebugMode),
		presentCollisionResolver_(colliders, ball, isDebugMode),
		potentialCollisionResolver_(colliders, ball, isDebugMode) {}

	std::shared_ptr<TickBase> process(double deltaTime) {
		auto collisions = collisionEngine_.findCollisions(deltaTime);

		presentCollisionResolver_.load(collisions);
		inevitableCollisionResolver_.load(collisions);
		potentialCollisionResolver_.load(collisions);

		if (presentCollisionResolver_.isApplicable()) {
			return presentCollisionResolver_.resolve(deltaTime);
		}

		if (inevitableCollisionResolver_.isApplicable()) {
			return inevitableCollisionResolver_.resolve(deltaTime);
		}

		return potentialCollisionResolver_.resolve(deltaTime);
	}

	CollisionEngine collisionEngine_;
	InevitableCollisionResolver inevitableCollisionResolver_;
	PresentCollisionResolver presentCollisionResolver_;
	PotentialCollisionResolver potentialCollisionResolver_;
};

class Simulator {
public:
	Simulator(
		const std::vector<std::shared_ptr<Collider>>& colliders, std::shared_ptr<Ball> ball,
		bool isDebugMode
	) :
		processor_(colliders, ball, isDebugMode), isDebugMode_(isDebugMode), simulationTime_(0.0) {}

	std::shared_ptr<TickBase> process(double deltaTime) {
		auto result		 = processor_.process(deltaTime);
		simulationTime_ += result->getTimeSpent();
		result->setSimulationTime(simulationTime_);
		return result;
	}

	bool isDebugMode() const {
		return isDebugMode_;
	}

	double getSimulationTime() const {
		return simulationTime_;
	}

	const TickProcessor& getProcessor() const {
		return processor_;
	}

private:
	TickProcessor processor_;
	bool isDebugMode_;
	double simulationTime_;
};

class ProtegonCanvas : public Canvas {
public:
	virtual double getWidth() const {
		return window_size.x;
	}

	virtual double getHeight() const {
		return window_size.y;
	}
};

class VisualDebugger {
public:
	static inline bool ENABLED							= true;
	static constexpr double INDICATE_COLLISION_DISTANCE = 100.0;

	VisualDebugger(std::shared_ptr<Ball> ball, std::shared_ptr<PaintCommandHandler> handler) :
		ball_(std::move(ball)), handler_(std::move(handler)) {}

	void clear() {
		handler_->clear();
	}

	void paint(const std::shared_ptr<TickBase>& result) {
		if (!ENABLED) {
			return;
		}

		// Check if it's a StationaryTick via dynamic_cast
		if (result->isStationary()) {
			handler_->fill(ball_, ptgn::color::Green);
		}

		double minimumDistance = result->getMinimumDistanceToCollision();

		if (minimumDistance < INDICATE_COLLISION_DISTANCE) {
			double normalized = minimumDistance / INDICATE_COLLISION_DISTANCE;
			double alpha	  = 1.0 - normalized;
			handler_->fill(ball_, ptgn::Color(255, 0, 0, std::uint8_t(255.0 * alpha)));
		}
	}

	void paint() {
		if (!ENABLED) {
			return;
		}

		Point2D center = ball_->getCenter();

		// Velocity indicator
		if (ball_->getSpeed() != 0) {
			Vector2D velocity = ball_->getVelocity();
			Point2D p0		  = center.add(velocity.multiply(0.1));
			handler_->drawLine(center, p0, ptgn::color::Cyan, 2);
		}

		// Net force indicator
		Vector2D netForce = ball_->getNetForce();
		if (netForce.length() != 0) {
			Point2D q0 = center.add(netForce);
			handler_->drawLine(center, q0, ptgn::color::Magenta, 2);
		}
	}

private:
	std::shared_ptr<Ball> ball_;
	std::shared_ptr<PaintCommandHandler> handler_;
};

class ProtegonGraphicsContext : public GraphicsContext {
public:
	ProtegonGraphicsContext() {}

	void save() override {
		matrixStack.push(currentTransform);
	}

	void restore() override {
		currentTransform = matrixStack.top();
		matrixStack.pop();
	}

	void scale(double sx, double sy) override {
		ptgn::SetScale(currentTransform, ptgn::V2_float{ sx, sy });
	}

	void setStroke(const ptgn::Color& color) override {
		strokeColor = color;
	}

	void setFill(const ptgn::Color& color) override {
		fillColor = color;
	}

	void setLineWidth(double width) override {
		line_width = (float)width;
	}

	void clearRect(double x, double y, double w, double h) override {
		setFill(bgColor);
		fillRect(x, y, w, h);
	}

	void strokeLine(double x1, double y1, double x2, double y2) override {
		drawLine(x1, y1, x2, y2);
	}

	void fillRect(double x, double y, double w, double h) override {
		drawRect(x, y, w, h, true);
	}

	void fillRoundRect(double x, double y, double w, double h, double arcW, double arcH) override {
		drawFilledRoundedRect({ x, y }, (float)w, (float)h, (float)arcW, (float)arcH, fillColor);
	}

	void strokeRect(double x, double y, double w, double h) override {
		drawRect(x, y, w, h, false);
	}

	void fillOval(double x, double y, double w, double h) override {
		drawOval(x, y, w, h, true);
	}

	void strokeOval(double x, double y, double w, double h) override {
		drawOval(x, y, w, h, false);
	}

	void beginPath() override {
		pathVertices.clear();
	}

	void moveTo(double x, double y) override {
		pathVertices.push_back({ (float)x, (float)y });
	}

	void lineTo(double x, double y) override {
		pathVertices.push_back({ (float)x, (float)y });
	}

	void closePath() override {
		if (!pathVertices.empty()) {
			pathVertices.push_back(pathVertices.front());
		}
	}

	void fill() override {
		drawPath(true);
	}

	void stroke() override {
		drawPath(false);
	}

private:
	float line_width{ 1.0f };
	ptgn::Camera currentTransform;
	std::stack<ptgn::Camera> matrixStack;
	ptgn::Color fillColor, strokeColor, bgColor;

	std::vector<ptgn::V2_float> pathVertices;

	void drawFilledRoundedRect(
		const ptgn::V2_float& position, float width, float height, float arcWidth, float arcHeight,
		const ptgn::Color& color, int segments = 8
	) {
		std::vector<ptgn::Triangle> triangles;

		const float x = position.x;
		const float y = position.y;

		const float r  = arcWidth;
		const float ry = arcHeight;

		const float right  = x + width;
		const float bottom = y + height;

		const ptgn::V2_float topLeft	 = { x + r, y + ry };
		const ptgn::V2_float topRight	 = { right - r, y + ry };
		const ptgn::V2_float bottomLeft	 = { x + r, bottom - ry };
		const ptgn::V2_float bottomRight = { right - r, bottom - ry };

		// Draw the center and side rectangles
		auto addRect = [&](float x0, float y0, float x1, float y1) {
			triangles.emplace_back(
				ptgn::V2_float{ x0, y0 }, ptgn::V2_float{ x1, y0 }, ptgn::V2_float{ x1, y1 }
			);
			triangles.emplace_back(
				ptgn::V2_float{ x0, y0 }, ptgn::V2_float{ x1, y1 }, ptgn::V2_float{ x0, y1 }
			);
		};

		// Center
		addRect(x + r, y + ry, right - r, bottom - ry);

		// Top and Bottom
		addRect(x + r, y, right - r, y + ry);			// Top
		addRect(x + r, bottom - ry, right - r, bottom); // Bottom

		// Left and Right
		addRect(x, y + ry, x + r, bottom - ry);			// Left
		addRect(right - r, y + ry, right, bottom - ry); // Right

		// Quarter circle approximation
		auto addCorner = [&](float cx, float cy, float startAngleDeg) {
			float angleStep = ptgn::pi<float> / 2.0f / segments;

			for (int i = 0; i < segments; ++i) {
				float angle0 = ptgn::DegToRad(startAngleDeg) + angleStep * i;
				float angle1 = ptgn::DegToRad(startAngleDeg) + angleStep * (i + 1);

				ptgn::V2_float p0 = { cx, cy };
				ptgn::V2_float p1 = { cx + std::cos(angle0) * r, cy + std::sin(angle0) * ry };
				ptgn::V2_float p2 = { cx + std::cos(angle1) * r, cy + std::sin(angle1) * ry };

				triangles.emplace_back(p0, p1, p2);
			}
		};

		// Top-left
		addCorner(x + r, y + ry, 180.0f);

		// Top-right
		addCorner(right - r, y + ry, 270.0f);

		// Bottom-right
		addCorner(right - r, bottom - ry, 0.0f);

		// Bottom-left
		addCorner(x + r, bottom - ry, 90.0f);

		for (auto& triangle : triangles) {
			ptgn::DrawDebugTriangle(triangle.GetLocalVertices(), color, -1.0f, currentTransform);
		}
	}

	void drawRect(double x, double y, double w, double h, bool filled) {
		ptgn::DrawDebugRect(
			ptgn::V2_float{ x, y }, ptgn::V2_float{ w, h }, filled ? fillColor : strokeColor,
			ptgn::Origin::TopLeft, filled ? -1.0f : line_width, 0.0f, currentTransform
		);
	}

	void drawOval(double x, double y, double w, double h, bool filled) {
		float cx = float(x + w / 2), cy = float(y + h / 2), rx = float(w / 2), ry = float(h / 2);
		ptgn::DrawDebugEllipse(
			{ cx, cy }, { rx, ry }, filled ? fillColor : strokeColor, filled ? -1.0f : line_width,
			0.0f, currentTransform
		);
	}

	void drawLine(double x1, double y1, double x2, double y2) {
		ptgn::DrawDebugLine({ x1, y1 }, { x2, y2 }, strokeColor, line_width, currentTransform);
	}

	void drawFilledPath(const std::vector<ptgn::V2_float>& vertices, const ptgn::Color& color) {
		if (vertices.size() < 3) {
			return;
		}

		std::vector<ptgn::Triangle> triangles;

		const auto& center = vertices.at(0);
		for (size_t i = 1; i + 1 < vertices.size(); ++i) {
			triangles.emplace_back(center, vertices.at(i), vertices.at(i + 1));
		}
		for (auto& triangle : triangles) {
			ptgn::DrawDebugTriangle(triangle.GetLocalVertices(), color, -1.0f, currentTransform);
		}
	}

	void drawStrokedPath(
		const std::vector<ptgn::V2_float>& vertices, const ptgn::Color& color, float width
	) {
		if (vertices.size() < 2) {
			return;
		}

		std::vector<ptgn::Triangle> strokeTriangles;

		for (size_t i = 0; i < vertices.size(); ++i) {
			const ptgn::V2_float& p0 = vertices.at(i);
			const ptgn::V2_float& p1 = vertices.at((i + 1) % vertices.size());

			// Direction and perpendicular
			ptgn::V2_float dir	  = (p1 - p0).Normalized();
			ptgn::V2_float normal = ptgn::V2_float(-dir.y, dir.x) * (width * 0.5f);

			// Create quad (2 triangles)
			ptgn::V2_float a = p0 + normal;
			ptgn::V2_float b = p1 + normal;
			ptgn::V2_float c = p1 - normal;
			ptgn::V2_float d = p0 - normal;

			// Triangle 1: a, b, c
			strokeTriangles.emplace_back(a, b, c);
			// Triangle 2: a, c, d
			strokeTriangles.emplace_back(a, c, d);
		}
		for (auto& triangle : strokeTriangles) {
			ptgn::DrawDebugTriangle(triangle.GetLocalVertices(), color, width, currentTransform);
		}
	}

	void drawPath(bool filled) {
		if (pathVertices.empty()) {
			return;
		}

		if (filled) {
			drawFilledPath(pathVertices, fillColor);
		} else {
			drawStrokedPath(pathVertices, strokeColor, line_width);
		}
	}
};

class GraphicsEngine /*: public Manager*/ {
public:
	GraphicsEngine(std::shared_ptr<GameObjects> objects, bool isDebugMode) :
		objects_(std::move(objects)),
		ball_(objects_->getBall()),
		width_(objects_->getWorld()->getWidth()),
		height_(objects_->getWorld()->getHeight()),
		isDebugMode_(isDebugMode),
		canvas_(std::make_shared<ProtegonCanvas>()),
		context{ std::make_shared<ProtegonGraphicsContext>() },
		painter_(nullptr),		 // init later
		visualDebugger_(nullptr) // init later
	{
		// Assume Painter takes a canvas or graphics context

		painter_ = std::make_shared<Painter>(context, width_, height_);

		// visualDebugger requires a painter handler - createHandler() returns shared_ptr
		visualDebugger_ = std::make_shared<VisualDebugger>(ball_, createHandler());
	}

	static std::shared_ptr<PaintCommandHandler> createHandler() {
		auto handler = std::make_shared<PaintCommandHandler>();
		handlers_.push_back(handler);
		return handler;
	}

	void update(std::shared_ptr<TickBase> result) {
		if (isPaused()) {
			return;
		}

		if (isDebugMode_) {
			visualDebugger_->clear();
			visualDebugger_->paint(result);
			visualDebugger_->paint();
		}

		painter_->save();
		painter_->clear();

		// Background
		painter_->fillBackground(objects_->getWorld()->getColor());

		// Ball
		painter_->fill(*ball_);

		// Paddle
		auto paddle = objects_->getPaddle();
		if (paddle->isActiveDrawable()) {
			painter_->fillRoundRectangle(
				*paddle, Constants::Paddle::ARC_RADIUS, Constants::Paddle::ARC_RADIUS
			);
		}

		// Bricks
		auto bricks = objects_->getBricks();
		for (const auto& brick : bricks) {
			if (brick->isActiveDrawable()) {
				painter_->fillRoundRectangle(
					*brick, Constants::Brick::ARC_RADIUS, Constants::Brick::ARC_RADIUS
				);
			}
		}

		// Obstacles
		auto obstacles = objects_->getObstacles();
		for (const auto& obstacle : obstacles) {
			if (obstacle->isActiveDrawable()) {
				painter_->fill(*obstacle);
			}
		}

		// Process commands
		for (auto& handler : handlers_) {
			painter_->processCommands(*handler);
		}

		painter_->restore();
	}

	const std::shared_ptr<Canvas>& getCanvas() const {
		return canvas_;
	}

	void pause() {
		paused = true;
	}

	void resume() {
		paused = false;
	}

private:
	bool isPaused() const {
		// Implement your pause logic here
		return paused;
	}

	bool paused{ false };

	static std::vector<std::shared_ptr<PaintCommandHandler>> handlers_;

	std::shared_ptr<GameObjects> objects_;
	std::shared_ptr<Ball> ball_;
	double width_;
	double height_;
	bool isDebugMode_;

	std::shared_ptr<GraphicsContext> context;
	std::shared_ptr<Canvas> canvas_;

	std::shared_ptr<Painter> painter_;
	std::shared_ptr<VisualDebugger> visualDebugger_;
};

// Static member initialization
std::vector<std::shared_ptr<PaintCommandHandler>> GraphicsEngine::handlers_;

class TrajectoryPlotter {
public:
	// Plot trajectory
	static void plot(
		void* caller, const std::vector<std::shared_ptr<Collider>>& colliders,
		const Point2D& center, double radius, const Vector2D& velocity
	) {
		auto& impl = getInstance(caller);
		impl.plot(colliders, center, radius, velocity);
	}

	static void clear(void* caller) {
		auto& impl = getInstance(caller);
		impl.clear();
	}

	static void show(void* caller) {
		auto& impl = getInstance(caller);
		impl.show();
	}

	static void hide(void* caller) {
		auto& impl = getInstance(caller);
		impl.hide();
	}

private:
	class TrajectoryPlotterInner {
	public:
		explicit TrajectoryPlotterInner(std::shared_ptr<PaintCommandHandler> painter) :
			painter_(std::move(painter)) {}

		void plot(
			const std::vector<std::shared_ptr<Collider>>& colliders, const Point2D& center,
			double radius, const Vector2D& velocity
		) {
			std::shared_ptr<Ball> ball =
				std::make_shared<Ball>(center, radius, velocity, ptgn::color::White);

			Simulator simulator(colliders, ball, true);

			std::vector<Point2D> vertices;
			vertices.reserve(500);

			painter_->clear();
			painter_->stroke(ball, ptgn::color::Green, 2);

			int numberOfCollisions = 0;
			int numberOfIterations = 0;

			while (numberOfIterations < 500) {
				vertices.push_back(ball->getCenter());
				const double deltaTime = Constants::Physics::SIMULATION_RATIO;

				auto result = simulator.process(deltaTime);

				// Dynamic cast to check if result is CrashTick
				if (result->isCrash()) {
					++numberOfCollisions;
					painter_->stroke(ball, ptgn::color::Green, 2);

					if (numberOfCollisions >= 10) {
						break;
					}
				}
				++numberOfIterations;
			}
			std::shared_ptr<Path> path = std::make_shared<Path>(vertices, ptgn::color::Red);
			painter_->stroke(path, 2);

			lastCommands_ = painter_->copyCommands();
		}

		void show() {
			painter_->setCommands(lastCommands_);
		}

		void hide() {
			painter_->clear();
		}

		void clear() {
			painter_->clear();
		}

	private:
		std::shared_ptr<PaintCommandHandler> painter_;
		std::vector<std::shared_ptr<PaintCommandHandler::PaintCommand>> lastCommands_;
	};

	static TrajectoryPlotterInner& getInstance(void* caller) {
		static std::unordered_map<void*, std::unique_ptr<TrajectoryPlotterInner>> instances;

		auto it = instances.find(caller);
		if (it != instances.end()) {
			return *it->second;
		}

		// Assume GraphicsEngine::createHandler() returns std::unique_ptr<PaintCommandHandler>
		auto painter				= GraphicsEngine::createHandler();
		auto impl					= std::make_unique<TrajectoryPlotterInner>(painter);
		TrajectoryPlotterInner& ref = *impl;
		instances.emplace(caller, std::move(impl));
		return ref;
	}
};

class ThrowEventHandler {
public:
	static bool PLOT_TRAJECTORY_ENABLED;

	explicit ThrowEventHandler(const std::shared_ptr<GameObjects>& objects) :
		painter(GraphicsEngine::createHandler()),
		ball(objects->getBall()),
		colliders(objects->getColliders()),
		isEnabled(true) {
		if (PLOT_TRAJECTORY_ENABLED) {
			TrajectoryPlotter::show(this);
		}
	}

	void update() {
		if (ball->isFreeze()) {
			painter->clear();
			painter->drawLine(ball->getCenter(), cursorPosition, ptgn::color::Yellow, 2);
			auto velocity = calculateVelocity(cursorPosition);
			plotTrajectory(velocity);
		}

		if (!PLOT_TRAJECTORY_ENABLED) {
			TrajectoryPlotter::hide(this);
		}

		if (!isEnabled) {
			return;
		}

		updateCursorPositionIfApplicable();

		if (ptgn::game.input.MouseDown(ptgn::Mouse::Left)) {
			freezeBallIfApplicable();
		} else if (ptgn::game.input.MouseUp(ptgn::Mouse::Left)) {
			throwBallIfApplicable();
			painter->clear();
		}
	}

	void setEnabled(bool value) {
		isEnabled = value;
	}

	bool getEnabled() const {
		return isEnabled;
	}

	void plotTrajectory(const Vector2D& velocity) {
		TrajectoryPlotter::plot(this, colliders, ball->getCenter(), ball->getRadius(), velocity);
	}

private:
	std::shared_ptr<PaintCommandHandler> painter;
	std::shared_ptr<Ball> ball;
	std::vector<std::shared_ptr<Collider>> colliders;
	Point2D cursorPosition = { 0, 0 };
	bool isEnabled;

	void updateCursorPositionIfApplicable() {
		auto event{ ptgn::game.input.GetMousePosition(ptgn::ViewportType::WindowTopLeft) };
		if (ball->isFreeze()) {
			cursorPosition = TransformationHelper::fromCanvasToWorld(event.x, event.y);
			// Event consumption if applicable
		}
	}

	void freezeBallIfApplicable() {
		auto event{ ptgn::game.input.GetMousePosition(ptgn::ViewportType::WindowTopLeft) };
		auto worldPos = TransformationHelper::fromCanvasToWorld(event.x, event.y);
		if (ball->contains(worldPos, 4)) {
			cursorPosition = worldPos;
			ball->setFreeze(true);
		}
	}

	void throwBallIfApplicable() {
		if (ball->isFreeze()) {
			auto velocity = calculateVelocity(cursorPosition);
			ball->setFreeze(false);
			ball->setVelocity(velocity);
			plotTrajectory(velocity);
		}
	}

	Vector2D calculateVelocity(const Point2D& cursorpos) const {
		return cursorpos.subtract(ball->getCenter()).multiply(5.0f);
	}
};

// Static initialization
bool ThrowEventHandler::PLOT_TRAJECTORY_ENABLED = true;

class DragEventHandler {
public:
	explicit DragEventHandler(const std::shared_ptr<GameObjects>& objects) : objects_(objects) {}

	virtual ~DragEventHandler() = default;

	// To be called each frame or tick
	virtual void update() {
		// TODO: Fix.
	}

protected:
	void dragLater(const std::shared_ptr<Draggable>& target, const Point2D& delta) {
		translate(target, delta);
	}

	void translate(const std::shared_ptr<Draggable>& node, const Point2D& delta);

protected:
	Point2D calculateAllowedTranslation(
		const Point2D& contactPointOnEdge, const Point2D& contactPointOnBall, const Point2D& delta
	) const;

	std::shared_ptr<GameObjects> objects_;
};

void DragEventHandler::translate(const std::shared_ptr<Draggable>& node, const Point2D& delta) {
	auto ball												  = objects_->getBall();
	std::optional<std::shared_ptr<CriticalPointPair>> closest = std::nullopt;

	// If the node is also a Collider, check for possible collisions
	auto collider = std::dynamic_pointer_cast<Collider>(node);
	if (collider) {
		Vector2D velocity = delta.multiply(-1);
		auto result		  = CollisionEngine::findMostCriticalPointAlongGivenDirection(
			  *ball.get(), collider, velocity
		  );
		if (result.has_value()) {
			closest = result.value();
		}
	}

	Point2D allowedTranslation;
	if (!closest) {
		allowedTranslation = delta;
	} else {
		const Point2D& contactPointOnEdge = closest.value()->getPointOnEdge();
		const Point2D& contactPointOnBall = closest.value()->getPointOnCircle();
		allowedTranslation =
			calculateAllowedTranslation(contactPointOnEdge, contactPointOnBall, delta);
	}

	// Clamp paddle movement only along x-axis
	auto paddle = std::dynamic_pointer_cast<Paddle>(node);
	if (paddle) {
		double paddleLeftCurrent		= paddle->getX();
		double paddleLeftRequestedDelta = allowedTranslation.getX();
		double paddleLeftMin			= 0.0;
		double paddleLeftMax			= objects_->getWorld()->getWidth() - paddle->getWidth();
		double paddleLeftRequested		= paddleLeftCurrent + paddleLeftRequestedDelta;
		double paddleLeftClamped = Util::clamp(paddleLeftMin, paddleLeftRequested, paddleLeftMax);
		double paddleLeftClampedDelta = paddleLeftClamped - paddleLeftCurrent;

		paddle->translate({ paddleLeftClampedDelta, 0.0 });
	} else {
		node->translate(allowedTranslation);
	}
}

Point2D DragEventHandler::calculateAllowedTranslation(
	const Point2D& contactPointOnEdge, const Point2D& contactPointOnBall, const Point2D& delta
) const {
	Vector2D maxAllowed = contactPointOnBall.subtract(contactPointOnEdge);
	Vector2D requested	= delta.toVector2D();
	Vector2D projection = maxAllowed.projectOnto(requested);

	double maxDist = projection.length();
	double reqDist = requested.length();

	double allowedDist = Util::clamp(0.0, reqDist, maxDist - 1.0);
	Vector2D direction = requested.normalized();
	return direction.multiply(allowedDist).toPoint2D();
}

class DebuggerDragEventHandler : public DragEventHandler {
public:
	explicit DebuggerDragEventHandler(std::shared_ptr<GameObjects> objects);

	// Implement the event listener interface
	void update() override;

private:
	std::vector<std::shared_ptr<Draggable>> draggables_;
	std::shared_ptr<PaintCommandHandler> painter_;

	std::shared_ptr<Draggable> target_ = nullptr;
	Point2D delta_;

	void acceptIfDrawable(const std::shared_ptr<Draggable>& target, bool accept);
	void paintIfDrawable(const std::shared_ptr<Draggable>& target);
	std::shared_ptr<Draggable> locateDraggable(const Point2D& query);
};

DebuggerDragEventHandler::DebuggerDragEventHandler(std::shared_ptr<GameObjects> objects) :
	DragEventHandler(std::move(objects)),
	draggables_(this->objects_->getDraggables()),
	painter_(GraphicsEngine::createHandler()) {}

void DebuggerDragEventHandler::update() {
	ptgn::V2_float cur{ ptgn::game.input.GetMousePosition(ptgn::ViewportType::WindowTopLeft) };
	ptgn::V2_float prev{ ptgn::game.input.GetMousePositionPrevious(ptgn::ViewportType::WindowTopLeft
	) };
	if (ptgn::game.input.MousePressed(ptgn::Mouse::Left)) {
		Point2D worldPos = TransformationHelper::fromCanvasToWorld(cur.x, cur.y);
		auto located	 = locateDraggable(worldPos);
		if (located) {
			target_ = located;
			acceptIfDrawable(target_, true);
			paintIfDrawable(target_);

			delta_ = Point2D(0, 0);
		}
	} else if (ptgn::game.input.MouseReleased(ptgn::Mouse::Left)) {
		if (target_) {
			acceptIfDrawable(target_, false);
		}
		target_ = nullptr;
	}
	if (cur != prev && ptgn::game.input.MousePressed(ptgn::Mouse::Left)) {
		if (target_) {
			Point2D current	 = TransformationHelper::fromCanvasToWorld(cur.x, cur.y);
			Point2D previous = TransformationHelper::fromCanvasToWorld(prev.x, prev.y);
			Point2D added	 = delta_.add(current.subtract(previous));

			translate(target_, added);
			paintIfDrawable(target_);

			delta_ = Point2D(0, 0);
		}
	}
}

void DebuggerDragEventHandler::acceptIfDrawable(
	const std::shared_ptr<Draggable>& target, bool accept
) {
	auto drawable = std::dynamic_pointer_cast<Drawable>(target);
	if (drawable) {
		drawable->setIsActiveDrawable(!accept);
		painter_->clear();
	}
}

void DebuggerDragEventHandler::paintIfDrawable(const std::shared_ptr<Draggable>& target) {
	auto drawable = std::dynamic_pointer_cast<Drawable>(target);
	if (drawable) {
		painter_->clear();
		// Assuming Color class has static factory method fromRGBA
		painter_->fill(drawable, ptgn::Color(255, 255, 255, std::uint8_t(255.0 * 0.6)));
	}
}

std::shared_ptr<Draggable> DebuggerDragEventHandler::locateDraggable(const Point2D& query) {
	for (const auto& draggable : draggables_) {
		if (!draggable->isActiveDraggable()) {
			continue;
		}
		if (draggable->contains(query)) {
			return draggable;
		}
	}
	return nullptr;
}

class BreakoutDragEventHandler : public DragEventHandler {
public:
	explicit BreakoutDragEventHandler(std::shared_ptr<GameObjects> objects);

	void update() override;

private:
	std::shared_ptr<Paddle> paddle_;
	bool focused_		  = false;
	bool ignoreMouseMove_ = false;
	Point2D delta_;

	void updateCursor();
	void moveMouse();
};

BreakoutDragEventHandler::BreakoutDragEventHandler(std::shared_ptr<GameObjects> objects) :
	DragEventHandler(std::move(objects)),
	paddle_(this->objects_->getPaddle()),
	focused_(false),
	ignoreMouseMove_(false),
	delta_(0, 0) {}

void BreakoutDragEventHandler::update() {
	if (ignoreMouseMove_) {
		ignoreMouseMove_ = false;
		return;
	}

	ptgn::V2_float cur{ ptgn::game.input.GetMousePosition(ptgn::ViewportType::WindowTopLeft) };
	ptgn::V2_float prev{ ptgn::game.input.GetMousePositionPrevious(ptgn::ViewportType::WindowTopLeft
	) };
	if (ptgn::game.input.MouseDown(ptgn::Mouse::Left)) {
		focused_ = !focused_;

		if (focused_) {
			delta_ = Point2D(0, 0);
		}

		updateCursor();
	}
	if (cur != prev) {
		if (focused_) {
			Point2D current	 = TransformationHelper::fromCanvasToWorld(cur.x, cur.y);
			Point2D previous = TransformationHelper::fromCanvasToWorld(prev.x, prev.y);
			Point2D added	 = delta_.add(current.subtract(previous));

			ignoreMouseMove_ = true;
			moveMouse();
			delta_ = Point2D(0, 0);

			translate(paddle_, added);
		}
	}
}

void BreakoutDragEventHandler::updateCursor() {
	// Hypothetical API to set cursor visibility for your window or widget
	/*if (focused_) {
		CursorManager::setCursorVisible(false);
	} else {
		CursorManager::setCursorVisible(true);
	}*/
}

void BreakoutDragEventHandler::moveMouse() {
	// Assuming you have a way to get screen coords for the center of your canvas/window
	/*auto sceneCenter	 = TransformationHelper::getCanvasCenter();
	auto screenCoordsOpt = sceneCenter;

	if (screenCoordsOpt.has_value()) {
		CursorManager::moveMouse(screenCoordsOpt->x, screenCoordsOpt->y);
	} else {
		CursorManager::moveMouse(event.getScreenX(), event.getScreenY());
	}*/
}

class PhysicsManager : public Manager {
public:
	PhysicsManager(std::shared_ptr<GameObjects> objects, bool isDebugMode);
	std::shared_ptr<TickBase> update();
	void next();

private:
	std::shared_ptr<TickBase> updatePrivate();
	void updateBricks(std::shared_ptr<TickBase> result);

	std::unique_ptr<Simulator> simulator;
	std::atomic<bool> nextFlag = false;
	std::shared_ptr<TickBase> result;

	bool isPaused() const; // Implement as needed or inherit from Manager
};

PhysicsManager::PhysicsManager(std::shared_ptr<GameObjects> objects, bool isDebugMode) {
	const std::vector<std::shared_ptr<Collider>>& colliders = objects->getColliders();
	std::shared_ptr<Ball> ball								= objects->getBall();

	simulator = std::make_unique<Simulator>(colliders, ball, isDebugMode);
	result	  = std::make_shared<PausedTick>();
}

std::shared_ptr<TickBase> PhysicsManager::update() {
	if (nextFlag.load()) {
		nextFlag.store(false);
		return updatePrivate();
	}

	if (isPaused()) {
		return result;
	}

	return updatePrivate();
}

std::shared_ptr<TickBase> PhysicsManager::updatePrivate() {
	const double deltaTime = Constants::Physics::SIMULATION_RATIO;
	result				   = simulator->process(deltaTime);

	updateBricks(result);

	return result;
}

void PhysicsManager::updateBricks(std::shared_ptr<TickBase> res) {
	// Assuming CrashTick inherits from TickBase
	if (!res->isCrash()) {
		return;
	}

	auto crashTick = std::dynamic_pointer_cast<CrashTick>(res);

	const auto& collisions = crashTick->getCollisions();
	for (const auto& collision : collisions) {
		std::shared_ptr<Collider> collider = collision->getCollider();

		auto brick = std::dynamic_pointer_cast<Brick>(collider);
		if (brick) {
			brick->setHit(true);
		}
	}
}

void PhysicsManager::next() {
	nextFlag.store(true);
}

bool PhysicsManager::isPaused() const {
	// Implement your paused logic here or override from Manager
	return false;
}

class EventProcessor {
public:
	EventProcessor(std::shared_ptr<GameObjects> objects, bool isDebugMode) {
		throwEventHandler = std::make_shared<ThrowEventHandler>(objects);
		throwEventHandler->setEnabled(isDebugMode);

		if (isDebugMode) {
			dragEventHandler = std::make_shared<DebuggerDragEventHandler>(objects);
		} else {
			dragEventHandler = std::make_shared<BreakoutDragEventHandler>(objects);
		}
	}

	void update() {
		throwEventHandler->update();
		dragEventHandler->update();
	}

private:
	std::shared_ptr<DragEventHandler> dragEventHandler;
	std::shared_ptr<ThrowEventHandler> throwEventHandler;
};

class Controller {
public:
	explicit Controller(bool isDebugMode);
	~Controller();

	void start();
	void pause();
	void resume();
	void stop();

	void handleEvent();
	void update();

private:
	void setupResizing();
	void onWindowMinimized(bool minimized);

private:
	bool isDebugMode;

	std::shared_ptr<GameObjects> objects;
	std::shared_ptr<GraphicsEngine> graphics;
	std::shared_ptr<PhysicsManager> engine;
	std::shared_ptr<EventProcessor> eventProcessor;
	std::shared_ptr<Canvas> canvas; // raw pointer if managed by root or graphics

	bool running = false;

	// Simple timer using std::chrono, could be replaced by framework-specific timer
	std::function<void()> timerCallback;
	std::chrono::steady_clock::time_point lastUpdateTime;
};

Controller::Controller(bool debugMode) : isDebugMode(debugMode) {
	// Initialize game objects and components
	// GameObjectConstructor::construct equivalent assumed
	objects =
		std::make_shared<GameObjects>(std::move(GameObjectConstructor::construct(isDebugMode)));

	graphics = std::make_shared<GraphicsEngine>(objects, isDebugMode);

	canvas = graphics->getCanvas();

	TransformationHelper::initialize(objects->getWorld(), canvas);

	engine = std::make_shared<PhysicsManager>(objects, isDebugMode);

	eventProcessor = std::make_shared<EventProcessor>(objects, isDebugMode);

	// Setup event listening -- depends on framework
	// Example: canvas->setEventHandler([this](const Event& e){ this->handleEvent(e); });

	setupResizing();

	running		   = true;
	lastUpdateTime = std::chrono::steady_clock::now();

	// Setup timer callback (simulate AnimationTimer)
	timerCallback = [this]() {
		auto now = std::chrono::steady_clock::now();
		// Could add frame rate limiting here if needed
		update();
		lastUpdateTime = now;
	};
}

Controller::~Controller() {
	stop();
}

void Controller::start() {
	// If you have a window, setup minimize/maximize callbacks
	// e.g. onWindowMinimized callback setup to call pause()/resume()

	if (isDebugMode) {
		// gui->show(); // Blocks until ready (if applicable)
	}

	// Setup window title, size, center on screen, show window, etc.
	// All depend on your UI framework, pseudo:

	running = true;

	// Start timer loop - depends on framework;
	// e.g. call timerCallback repeatedly on each frame/event loop tick
}

void Controller::pause() {
	engine->pause();
	graphics->pause();
}

void Controller::resume() {
	engine->resume();
	graphics->resume();
}

void Controller::stop() {
	running = false;
	// gui->close();
}

void Controller::handleEvent() {
	// dispatcher->receiveEvent(event);
}

void Controller::update() {
	auto result = engine->update();
	eventProcessor->update();
	graphics->update(result);
	// gui->update(result);
}

void Controller::setupResizing() {
	// Similar logic to JavaFX resize listener:

	// Pseudo-code since C++ UI frameworks differ:

	// root->onResize([this]() {
	//     auto bounds = canvas->getLayoutBounds();
	//     double scale = std::min(root->getWidth() / bounds.width, root->getHeight() /
	//     bounds.height); canvas->setScale(scale);
	// });

	// You must implement this with your chosen framework's resize callbacks
}

void Controller::onWindowMinimized(bool minimized) {
	if (minimized) {
		pause();
	} else {
		resume();
	}
}

struct SandboxScene : public ptgn::Scene {
	Controller controller{ true };

	void Enter() override {
		// Initialize and start your app (replace with actual UI initialization)
		controller.start();
	}

	void Update() override {
		controller.update();
	}

	void Exit() override {
		controller.stop();
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	ptgn::game.Init("SandboxScene", window_size);
	ptgn::game.scene.Enter<SandboxScene>("");
	return 0;
}