#pragma once
#include "Components.h"

class CollisionComponent : public Component {
public:
	CollisionComponent() {
		_colliding = false;
	}
	void init() override {
		entity->addGroup(Groups::colliders);
	}
	bool isColliding() { return _colliding; };
	void setColliding(bool colliding) { _colliding = colliding; }
private:
	bool _colliding;
};