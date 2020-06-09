#pragma once

class Entity;

using StateID = unsigned int;

class BaseState {
public:
	virtual void enter(Entity& entity) = 0;
	virtual void exit(Entity& entity) = 0;
};

