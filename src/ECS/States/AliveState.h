#pragma once

#include "State.h"

class AliveState : public State<AliveState> {
public:
	virtual void enter(Entity& entity) override final {
		entity.addSignature<AliveState>();
	}
	virtual void exit(Entity& entity) override final {
		entity.removeSignature<AliveState>();
	}
};