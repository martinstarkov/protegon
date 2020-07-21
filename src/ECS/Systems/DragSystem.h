#pragma once

#include "System.h"

struct DragComponent;
struct MotionComponent;

class DragSystem : public System<DragComponent, MotionComponent> {
public:
	virtual void update() override final;
};