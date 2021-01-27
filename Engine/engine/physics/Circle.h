#pragma once

#include "Shape.h"

class Circle : public Shape {
public:
    Circle() = delete;
    Circle(double radius) : radius{ radius } {}
    virtual void CollisionCheck(Manifold* manifold, Circle* circle_shape) override final;
    virtual void CollisionCheck(Manifold* manifold, Polygon* polygon_shape) override final;
    virtual double GetRadius() const override final;
    virtual void SetRadius(double new_radius) override final;
    virtual Shape* Clone() const override final;
    virtual void Initialize() override final;
    virtual void ComputeMass(double density) override final;
    virtual void Draw(engine::Color color = engine::RED) const override final;
    virtual ShapeType GetType() const override final;
private:
    double radius{ 0.0 };
};