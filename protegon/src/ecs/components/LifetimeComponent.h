#pragma once

namespace engine {

struct LifetimeComponent {
	LifetimeComponent() = default;
	LifetimeComponent(double lifetime, bool is_dying = true) : lifetime{ lifetime }, is_dying{ is_dying } {}
	double lifetime{ 1.0 }; // Unit: Seconds.
	bool is_dying{ true }; // Makes the entity lose lifetime.
};

} // namespace engine