#pragma once

#include "SDL.h"

class Component {
public:

private:

};

enum Components {
	COMPONENT_NONE = 0,
	COMPONENT_DISPLACEMENT = 1 << 0,
	COMPONENT_VELOCITY = 1 << 1,
	COMPONENT_APPEARANCE = 1 << 2
};

struct Displacement : public Component {
	float x;
	float y;
};

struct Velocity : public Component {
	float x;
	float y;
};

struct Appearance : public Component {
	const char* name;
	SDL_Texture* texture;
};


//#include "ECS.h"
//#include "TransformComponent.h"
//#include "SizeComponent.h"
//#include "ColorComponent.h"
//#include "MotionComponent.h"
//#include "SpriteComponent.h"