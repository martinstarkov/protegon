#pragma once

struct HopperComponent {
	HopperComponent(double inertia) : inertia{ inertia } {};
	double inertia;
	double theta_d = 0;
	double theta_dd = 0;
};