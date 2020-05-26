#pragma once
#include "Components.h"

class ShootComponent: public Component {
public:
	ShootComponent(int maxBullets = 10) {
		_shooting = false;
		_bullets = 0;
		_maxBullets = maxBullets;
	}
	void init() override {
		entity->addGroup(Groups::shooters);
	}
	int getBulletCount() { return _bullets; }
	void changeBulletCount(int delta) { 
		if (abs(_bullets - delta) > 0 && abs(_bullets - delta) < _maxBullets) {
			_bullets = _bullets + delta;
		} else {
			_bullets = 0;
		}
	}
	bool isShooting() { return _shooting; };
	void setShooting(bool shooting) { _shooting = shooting; }
private:
	bool _shooting;
	int _bullets;
	int _maxBullets;
};