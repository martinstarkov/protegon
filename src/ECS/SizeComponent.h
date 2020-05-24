#pragma once
#include "Components.h"
#include "../Vec2D.h"

class SizeComponent : public Component {
public:
	SizeComponent(Vec2D size = Vec2D()) {
		_size = size;
	}
	Vec2D getSize() { return _size; }
	void setSize(Vec2D size) {
		_size = size;
	}
private:
	Vec2D _size;
};