#pragma once

struct StateVectorComponent {
	StateVectorComponent() = default;
	StateVectorComponent(double theta) : theta{ theta } {}
	double theta = 0.0;
};