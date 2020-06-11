#include "StateMachine.h"

#include "States.h"

StateMachine::StateMachine(Entity& entity) : _entity(entity) {
	create<StateFactory>(AliveState(), DeadState());
}
