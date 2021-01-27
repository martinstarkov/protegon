#pragma once

#include "utils/math/Vector2.h"

class Shape;

struct Body {
    Body(Shape* shape, const V2_double& position);
    ~Body();
    void ApplyForce(const V2_double& applied_force);
    void ApplyImpulse(const V2_double& impulse, const V2_double& contact_vector);
    void SetStatic();

    void SetOrientation(double radians);

    V2_double position{ 0, 0 };
    V2_double velocity{ 0, 0 };

    double angular_velocity{ 0 };
    double torque{ 0 };
    double orientation{ 0 }; // radians

    // TEMPORARY:
    int name = 0;

    V2_double force{ 0, 0 };

    // Set by shape
    double inertia{ 0 };  // moment of inertia
    double inverse_inertia{ 0 }; // inverse inertia
    double mass{ 0 };  // mass
    double inverse_mass{ 0 }; // inverse masee

    // http://gamedev.tutsplus.com/tutorials/implementation/how-to-create-a-custom-2d-physics-engine-friction-scene-and-jump-table/
    double static_friction{ 0 };
    double dynamic_friction{ 0 };
    double restitution{ 0 };

    // Shape interface
    Shape* shape = nullptr;
};