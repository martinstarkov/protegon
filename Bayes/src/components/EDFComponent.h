#pragma once

struct EDFComponent {
	EDFComponent(double thrust_force) : thrust_force{ thrust_force } {}
	double thrust_force = 0.0;
};