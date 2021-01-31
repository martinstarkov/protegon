#include "Body.h"

#include "Shape.h"

Body::Body(Shape* shape, const V2_double& position) : shape{ shape->Clone() }, position{ position } {
	this->shape->body = this;
	this->shape->Initialize();
}

Body::~Body() {
    delete shape;
    shape = nullptr;
}
void Body::ApplyForce(const V2_double& applied_force) {
    force += applied_force;
}
void Body::ApplyImpulse(const V2_double& impulse, const V2_double& contact_vector) {
    velocity += inverse_mass * impulse;
    angular_velocity += inverse_inertia * contact_vector.CrossProduct(impulse);
}
void Body::SetStatic() {
    inertia = 0.0;
    inverse_inertia = 0.0;
    mass = 0.0;
    inverse_mass = 0.0;
}

void Body::SetOrientation(double radians) {
	orientation = radians;
    assert(shape != nullptr && "Body shape cannot be nullptr");
	shape->SetOrientation(radians);
}
