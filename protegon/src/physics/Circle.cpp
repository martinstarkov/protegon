#include "Circle.h"

#include "renderer/TextureManager.h"
#include "Collision.h"
#include "Polygon.h"

void Circle::CollisionCheck(Manifold* manifold, Circle* circle_shape) {
    CircleVsCircle(manifold, body, circle_shape->body);
}

void Circle::CollisionCheck(Manifold* manifold, Polygon* polygon_shape) {
    CircleVsPolygon(manifold, body, polygon_shape->body);
}

double Circle::GetRadius() const {
    return radius;
}
void Circle::SetRadius(double new_radius) {
    radius = new_radius;
}

Shape* Circle::Clone() const {
    return new Circle(radius);
}

void Circle::Initialize() {
    ComputeMass(1.0);
}

void Circle::ComputeMass(double density) {
    auto circle_area = engine::math::PI<double> * radius * radius;
    body->mass = circle_area * density;
    body->inverse_mass = body->mass ? 1.0 / body->mass : 0.0;
    body->inertia = body->mass * radius * radius;
    body->inverse_inertia = body->inertia ? 1.0 / body->inertia : 0.0;
}
void Circle::Draw(engine::Color color) const {
    color = engine::RED;
    // Move elsewhere later.
    engine::TextureManager::DrawCircle(body->position, engine::math::Round(radius), color);
}

ShapeType Circle::GetType() const {
    return ShapeType::CIRCLE;
}