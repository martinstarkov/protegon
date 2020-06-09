#pragma once

#include "BaseState.h"

#include "../Entity.h"
#include "../Components.h"

template <class TState>
class State : public BaseState {
public:
	State() {
		_id = static_cast<StateID>(typeid(TState).hash_code());
	}
protected:
	StateID _id;
};