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
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/game.h"
#include "input/input_handler.h"
#include "math/vector2.h"
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

	double A, B, C; // line equation coefficients: Ax + By + C = 0

public:
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
	A_				  = coefficients[0];
	B_				  = coefficients[1];
	C_				  = coefficients[2];

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
	Point2D p0					   = calculatePointAt(parameters[0]);
	Point2D p1					   = calculatePointAt(parameters[1]);
	return { p0, p1 };
}

bool Circle::doesIntersect(const Line2D& line) const {
	bool intersects				= false;
	std::vector<Point2D> points = findPointsForGivenSlope(line.getSlope());
	Line2D perpendicularLine(points[0], points[1]);
	std::optional<Point2D> intersection = perpendicularLine.findIntersection(line);
	if (intersection.has_value()) {
		intersects = isPointInsideCircle(intersection.value());
	}
	return intersects;
}

Point2D Circle::findPointOnCircleClosestToLine(const Line2D& line) const {
	double slope				= line.getSlope();
	std::vector<Point2D> points = findPointsForGivenSlope(slope);
	Point2D p0					= points[0];
	Point2D p1					= points[1];
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
	A			= coeffs[0];
	B			= coeffs[1];
	C			= coeffs[2];

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
	A				  = coefficients[0];
	B				  = coefficients[1];
	C				  = coefficients[2];
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
			const std::string& identifier = identifiers[i];
			const Point2D& P			  = vertices[i];
			const Point2D& Q			  = vertices[(i + 1) % numberOfVertices];
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
			double yi = vertices[i].getY();
			double xi = vertices[i].getX();
			double yj = vertices[j].getY();
			double xj = vertices[j].getX();
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
			const std::string& identifier = identifiers[i];
			const Point2D& P			  = newVertices[i];
			const Point2D& Q			  = newVertices[(i + 1) % numberOfVertices];
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

class Event {
public:
	virtual ~Event() = default;
};

class MouseEvent : public Event {
public:
	bool isConsumed() const {
		return consumed;
	}

	void consume() {
		consumed = true;
	}

private:
	bool consumed = false;
};

class EventListener {
public:
	virtual ~EventListener() = default;

	virtual void listen(const Event& event) {}

	virtual void listen(const MouseEvent& event) = 0;
};

class EventDispatcher {
public:
	void receiveEvent(const Event& event) {
		// Check if event is MouseEvent using dynamic_cast
		if (auto mouseEvent = dynamic_cast<const MouseEvent*>(&event)) {
			dispatch(*mouseEvent);
		} else {
			dispatch(event);
		}
	}

	void addEventListener(EventListener* listener) {
		listeners.push_back(listener);
	}

	void removeEventListener(EventListener* listener) {
		listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
	}

private:
	std::vector<EventListener*> listeners;

	void dispatch(const Event& event) {
		for (auto* listener : listeners) {
			listener->listen(event);
		}
	}

	void dispatch(const MouseEvent& event) {
		for (auto* listener : listeners) {
			if (!event.isConsumed()) {
				listener->listen(event);
			}
		}
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

class BreakoutSimpleFloatProperty {
	float data[1];
	std::function<void(float)> listener;

public:
	explicit BreakoutSimpleFloatProperty(float initialValue) {
		data[0] = initialValue;
	}

	float get() const {
		return data[0];
	}

	void set(float value) {
		if (data[0] != value) {
			data[0] = value;
			if (listener) {
				listener(value);
			}
		}
	}

	float* getAsArray() {
		return data;
	}

	void update() {
		set(data[0]);
	}

	void addListener(std::function<void(float)> func) {
		listener = func;
	}
};

class Constants {
public:
	struct World {
		static constexpr double WIDTH		= 1280.0;
		static constexpr double HEIGHT		= 720.0;
		static constexpr double TOP_PADDING = 64.0;
		static const ptgn::Color BACKGROUND_COLOR;
		static constexpr float FRICTION_COEFFICIENT = { 0.05f };
	};

	struct Ball {
		static constexpr double RADIUS	  = 12.0;
		static constexpr double MIN_SPEED = 500.0;
		static constexpr double MAX_SPEED = 700.0;
		static constexpr double INITIAL_X = 0.5 * World::WIDTH;
		static constexpr double INITIAL_Y = 0.5 * World::HEIGHT;
		static const ptgn::Color COLOR;
		static float RESTITUTION_FACTOR;
		static float DO_NOT_BOUNCE_SPEED_THRESHOLD;
	};

	struct Paddle {
		static constexpr double WIDTH	  = 192.0;
		static constexpr double HEIGHT	  = 28.0;
		static constexpr double INITIAL_X = 0.5 * (World::WIDTH - WIDTH);
		static constexpr double INITIAL_Y = World::HEIGHT - 100.0;
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
		static constexpr float SIMULATION_RATIO = { 0.0125f };
		static BreakoutSimpleFloatProperty GRAVITY;
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

BreakoutSimpleFloatProperty Constants::Physics::GRAVITY(500.0f);

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
	Painter(GraphicsContext& context, double width, double height);

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
	GraphicsContext& gc;
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

Painter::Painter(GraphicsContext& context, double width, double height) :
	gc(context), width(width), height(height) {}

void Painter::scale(double scale) {
	gc.scale(scale, scale);
}

void Painter::clear() {
	gc.clearRect(0, 0, width, height);
}

void Painter::fillBackground(const ptgn::Color& color) {
	gc.save();
	gc.setFill(color);
	gc.fillRect(0, 0, width, height);
	gc.restore();
}

void Painter::save() {
	gc.save();
}

void Painter::restore() {
	gc.restore();
}

void Painter::drawLine(
	const Point2D& p0, const Point2D& p1, const ptgn::Color& color, double thickness
) {
	gc.setStroke(color);
	gc.setLineWidth(thickness);
	gc.strokeLine(p0.getX(), p0.getY(), p1.getX(), p1.getY());
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
	gc.setFill(color);
	double left = center.getX() - radius;
	double top	= center.getY() - radius;
	gc.fillOval(left, top, 2 * radius, 2 * radius);
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
	gc.setStroke(color);
	gc.setLineWidth(width_);
	double left = center.getX() - radius;
	double top	= center.getY() - radius;
	gc.strokeOval(left, top, 2 * radius, 2 * radius);
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
	gc.setFill(color);
	gc.fillRect(x, y, w, h);
}

void Painter::fillRoundRectangle(
	double x, double y, double width_, double height_, double arcWidth, double arcHeight,
	const ptgn::Color& color
) {
	gc.setFill(color);
	gc.fillRoundRect(x, y, width_, height_, arcWidth, arcHeight);
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
	gc.setStroke(color);
	gc.setLineWidth(width_);
	gc.strokeRect(x, y, w, h);
}

void Painter::fillPolygon(const std::vector<Point2D>& vertices, const ptgn::Color& color) {
	gc.setFill(color);
	gc.beginPath();
	if (vertices.empty()) {
		return;
	}
	gc.moveTo(vertices[0].getX(), vertices[0].getY());

	for (size_t i = 1; i < vertices.size(); ++i) {
		gc.lineTo(vertices[i].getX(), vertices[i].getY());
	}

	gc.closePath();
	gc.fill();
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
	gc.setStroke(color);
	gc.setLineWidth(width_);

	gc.beginPath();
	gc.moveTo(vertices[0].getX(), vertices[0].getY());

	for (size_t i = 1; i < vertices.size(); ++i) {
		gc.lineTo(vertices[i].getX(), vertices[i].getY());
	}

	if (closePath) {
		gc.closePath();
	}

	gc.stroke();
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
				result = std::make_shared<TangentialCriticalPoint>(pointsOnLineSegment[0]);
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
			result = std::make_shared<TangentialCriticalPoint>(pointsOnLineSegment[0]);
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

	Vector2D getNormal() const {
		return collider->getNormalOf(edge);
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

	double getTimeToCollision() const {
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

template <typename T>
class Tick {
	static_assert(std::is_base_of<Collision, T>::value, "T must be a subclass of Collision");

protected:
	std::vector<std::shared_ptr<T>> collisions_;
	double timeSpent_;
	double minimumTimeToCollision_;
	double minimumDistanceToCollision_;
	int numberOfSeparateCriticalPointPairs_;
	int numberOfTangentialCriticalPoints_;
	int numberOfCuttingCriticalPointPairs_;

	double simulationTime_ = 0.0;

public:
	Tick(std::vector<std::shared_ptr<T>> collisions, double timeSpent) :
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
	const std::vector<std::shared_ptr<T>>& getCollisions() const {
		return collisions_;
	}

	double getTimeSpent() const {
		return timeSpent_;
	}

	double getMinimumTimeToCollision() const {
		return minimumTimeToCollision_;
	}

	double getMinimumDistanceToCollision() const {
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
		std::vector<std::shared_ptr<InevitableCollision>> inevs;

		for (const auto& collision : collisions_) {
			auto inev = std::dynamic_pointer_cast<InevitableCollision>(collision);
			if (inev) {
				inevs.push_back(inev);
			}
		}

		if (inevs.empty()) {
			return std::numeric_limits<double>::max();
		}

		auto minIter = std::min_element(
			inevs.begin(), inevs.end(),
			[](const std::shared_ptr<InevitableCollision>& a,
			   const std::shared_ptr<InevitableCollision>& b) {
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
			[](const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) {
				return a->getContact()->getDistance() < b->getContact()->getDistance();
			}
		);

		return (*minIter)->getContact()->getDistance();
	}
};

template <typename T>
class StationaryTick : public Tick<T> {
public:
	StationaryTick(std::vector<std::shared_ptr<T>> collisions, double timeSpent) :
		Tick<T>(std::move(collisions), timeSpent) {}

	std::string getChildName() const override {
		return "Stationary Tick";
	}
};

class PausedTick : public Tick<Collision> {
public:
	PausedTick() : Tick<Collision>(std::vector<std::shared_ptr<Collision>>(), 0.0) {}

	std::string getChildName() const override {
		return "Paused Tick";
	}
};

template <typename T>
class FreeTick : public Tick<T> {
public:
	FreeTick(std::vector<std::shared_ptr<T>> collisions, double timeSpent) :
		Tick<T>(std::move(collisions), timeSpent) {}

	std::string getChildName() const override {
		return "Free Tick";
	}
};

template <typename T>
class CrashTick : public Tick<T> {
private:
	Vector2D normal_;

public:
	CrashTick(std::vector<std::shared_ptr<T>> collisions, Vector2D normal, double timeSpent) :
		Tick<T>(std::move(collisions), timeSpent), normal_(normal) {}

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
	Ball copy() const {
		return Ball(center, radius, velocity, getColor());
	}

	// Enlarge circle (delegate)
	Circle enlarge(double factor) const override {
		return DrawableCircle::enlarge(factor);
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

class CanvasNode {
public:
	virtual ~CanvasNode()			 = default;
	virtual double getWidth() const	 = 0;
	virtual double getHeight() const = 0;
};

class TransformationHelper {
public:
	static void initialize(const World& world, const CanvasNode& node) {
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
		TransformationHelperInner(const World& world_, const CanvasNode& node_) :
			world(world_), node(node_) {}

		Point2D fromWorldToCanvas(double x, double y) const {
			double ww = world.getWidth();
			double wh = world.getHeight();
			double gw = node.getWidth();
			double gh = node.getHeight();

			double nx = x / ww; // [0, 1]
			double ny = y / wh; // [0, 1]

			return Point2D(nx * gw, ny * gh);
		}

		Point2D fromWorldToCanvas(const Point2D& p) const {
			return fromWorldToCanvas(p.getX(), p.getY());
		}

		Point2D fromCanvasToWorld(double x, double y) const {
			double ww = world.getWidth();
			double wh = world.getHeight();
			double gw = node.getWidth();
			double gh = node.getHeight();

			double nx = x / gw; // [0, 1]
			double ny = y / gh; // [0, 1]

			return Point2D(nx * ww, ny * wh);
		}

		Point2D fromCanvasToWorld(const Point2D& p) const {
			return fromCanvasToWorld(p.getX(), p.getY());
		}

		Point2D getWorldCenter() const {
			return Point2D(0.5 * world.getWidth(), 0.5 * world.getHeight());
		}

		Point2D getCanvasCenter() const {
			return Point2D(0.5 * node.getWidth(), 0.5 * node.getHeight());
		}

	private:
		const World& world;
		const CanvasNode& node;
	};

	static std::unique_ptr<TransformationHelperInner> impl;
};

std::unique_ptr<TransformationHelper::TransformationHelperInner> TransformationHelper::impl =
	nullptr;

class GameObjects {
private:
	std::shared_ptr<World> world;
	std::unordered_set<std::shared_ptr<Brick>> bricks;
	std::unordered_set<std::shared_ptr<Obstacle>> obstacles;
	std::shared_ptr<Ball> ball;
	std::shared_ptr<Paddle> paddle;

	// Polymorphic interface collections use shared_ptr to base classes
	std::unordered_set<std::shared_ptr<Collider>> colliders;
	std::unordered_set<std::shared_ptr<Draggable>> draggables;

public:
	GameObjects(
		std::shared_ptr<World> w, std::unordered_set<std::shared_ptr<Brick>> b,
		std::unordered_set<std::shared_ptr<Obstacle>> o, std::shared_ptr<Ball> ba,
		std::shared_ptr<Paddle> p
	) :
		world(std::move(w)),
		bricks(std::move(b)),
		obstacles(std::move(o)),
		ball(std::move(ba)),
		paddle(std::move(p)) {
		colliders.insert(world);

		for (auto& brick : bricks) {
			colliders.insert(brick);
		}
		for (auto& obstacle : obstacles) {
			colliders.insert(obstacle);
		}
		colliders.insert(paddle);

		for (auto& obstacle : obstacles) {
			draggables.insert(obstacle);
		}
		draggables.insert(paddle);
	}

	// Getters
	std::shared_ptr<World> getWorld() const {
		return world;
	}

	const std::unordered_set<std::shared_ptr<Brick>>& getBricks() const {
		return bricks;
	}

	const std::unordered_set<std::shared_ptr<Obstacle>>& getObstacles() const {
		return obstacles;
	}

	std::shared_ptr<Ball> getBall() const {
		return ball;
	}

	std::shared_ptr<Paddle> getPaddle() const {
		return paddle;
	}

	const std::unordered_set<std::shared_ptr<Collider>>& getColliders() const {
		return colliders;
	}

	const std::unordered_set<std::shared_ptr<Draggable>>& getDraggables() const {
		return draggables;
	}
};

class GameObjectConstructor {
public:
	static GameObjects construct(bool isDebugMode) {
		auto world = std::make_shared<World>(
			0, 0, Constants::World::WIDTH, Constants::World::HEIGHT,
			Constants::World::BACKGROUND_COLOR
		);

		auto ball	= std::make_shared<Ball>(constructBall());
		auto paddle = std::make_shared<Paddle>(constructPaddle());

		std::unordered_set<std::shared_ptr<Brick>> bricks;
		std::unordered_set<std::shared_ptr<Obstacle>> obstacles;

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

	static std::unordered_set<std::shared_ptr<Brick>> constructBricks(int rows, int columns) {
		std::unordered_set<std::shared_ptr<Brick>> bricks;
		double totalWidth =
			columns * (Constants::Brick::WIDTH + Constants::Brick::HORIZONTAL_SPACING) -
			Constants::Brick::HORIZONTAL_SPACING;
		double left = 0.5 * (Constants::World::WIDTH - totalWidth);

		for (int i = 0; i < rows; ++i) {
			for (int j = 0; j < columns; ++j) {
				double x =
					j * (Constants::Brick::WIDTH + Constants::Brick::HORIZONTAL_SPACING) + left;
				double y = i * (Constants::Brick::HEIGHT + Constants::Brick::VERTICAL_SPACING) +
						   Constants::World::TOP_PADDING;
				ptgn::Color color = Constants::Brick::COLORS_PER_ROW.count(i) > 0
									  ? Constants::Brick::COLORS_PER_ROW.at(i)
									  : ptgn::color::White;

				bricks.insert(std::make_shared<Brick>(
					x, y, Constants::Brick::WIDTH, Constants::Brick::HEIGHT, color
				));
			}
		}

		return bricks;
	}

	static std::unordered_set<std::shared_ptr<Obstacle>> constructObstacles() {
		std::unordered_set<std::shared_ptr<Obstacle>> obstacles;

		obstacles.insert(std::make_shared<Obstacle>(
			std::vector<Point2D>{ Point2D(0, 720), Point2D(640, 720), Point2D(0, 500) },
			ptgn::color::White
		));

		obstacles.insert(std::make_shared<Obstacle>(
			std::vector<Point2D>{ Point2D(640, 720), Point2D(1280, 720), Point2D(1280, 500) },
			ptgn::color::White
		));

		obstacles.insert(std::make_shared<Obstacle>(200, 250, 100, 100, ptgn::color::White));
		obstacles.insert(std::make_shared<Obstacle>(980, 250, 100, 100, ptgn::color::White));

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

	static Vector2D calculateCollectiveCollisionNormal(
		const std::vector<std::shared_ptr<ProspectiveCollision>>& collisions,
		const Vector2D& velocity
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

struct SandboxScene : public ptgn::Scene {
	void Enter() override {}

	void Update() override {}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	ptgn::game.Init("SandboxScene", window_size);
	ptgn::game.scene.Enter<SandboxScene>("");
	return 0;
}