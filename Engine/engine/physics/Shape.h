#pragma once

#include "utils/Matrix.h"

#include "renderer/Color.h"

#include "Body.h"

#include <vector>

class Polygon;
class Circle;
struct Manifold;

enum class ShapeType {
	CIRCLE,
	POLYGON,
	COUNT
};

class Shape {
public:
	virtual ~Shape() = default;
	virtual Shape* Clone() const = 0;
	virtual void Initialize() = 0;
	virtual void ComputeMass(double density) = 0;
	virtual void CollisionCheck(Manifold* manifold, Circle* circle_shape) = 0;
	virtual void CollisionCheck(Manifold* manifold, Polygon* polygon_shape) = 0;
	virtual void SetOrientation(double radians) {};
	virtual void Draw(engine::Color color = engine::BLACK) const = 0;
	virtual ShapeType GetType() const = 0;
	virtual double GetRadius() const { return 0; };
	virtual const std::vector<V2_double>* GetVertices() const { return nullptr; };
	virtual Matrix<double, 2, 2> GetRotationMatrix() const { return {}; };
	Body* body = nullptr;
};