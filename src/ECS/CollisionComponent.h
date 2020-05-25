#pragma once
#include "Components.h"

class CollisionComponent : public Component {
public:
	CollisionComponent() {
		_colliding = false;
	}
	bool isColliding() { return _colliding; };
	void setColliding(bool colliding) { _colliding = colliding; }
private:
	bool _colliding;
};