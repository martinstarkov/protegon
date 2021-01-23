#pragma once

#include "Shape.h"

struct Manifold;
struct Body;

typedef void (*CollisionCallback)(Manifold* m, Body* a, Body* b);

extern CollisionCallback Dispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)];

void CircleVsCircle(Manifold* m, Body* a, Body* b);
void CircleVsPolygon(Manifold* m, Body* a, Body* b);
void PolygonVsCircle(Manifold* m, Body* a, Body* b);
void PolygonVsPolygon(Manifold* m, Body* a, Body* b);