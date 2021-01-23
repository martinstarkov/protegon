#pragma once

#include <vector>
#include <cstdint>

#include "utils/Vector2.h"
#include "Shape.h"

class Polygon : public Shape {
public:
    virtual void Initialize() override final;
    virtual Shape* Clone() const override final;
    virtual void CollisionCheck(Manifold* manifold, Circle* circle_shape) override final;
    virtual void CollisionCheck(Manifold* manifold, Polygon* polygon_shape) override final;
    virtual void ComputeMass(double density) override final;
    virtual void SetOrientation(double radians) override final;
    virtual Matrix<double, 2, 2> GetRotationMatrix() const override final;
    virtual void Draw(engine::Color color = engine::BLUE) const override final;
    virtual ShapeType GetType() const override final;
    virtual const std::vector<V2_double>* GetVertices() const override final { return &vertices; };
    // Half width and half height
    void SetBox(double half_width, double half_height);
    void Set(std::vector<V2_double> vertices);
    // The extreme point along a direction within a polygon
    V2_double GetSupport(const V2_double& direction);
    // TODO: Change to arrays and templates?
    std::vector<V2_double> vertices;
    std::vector<V2_double> normals;
    Matrix<double, 2, 2> rotation_matrix;
};
