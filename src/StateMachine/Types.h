#pragma once

#include "../common.h"

#define UNKNOWN_STATE "unknown state"

class BaseStateMachine;
class BaseState;

using StateName = std::string;
using StateMachineName = std::string;

using StateMachineMap = std::map<StateMachineName, std::unique_ptr<BaseStateMachine>>;
using StateMap = std::map<StateName, std::unique_ptr<BaseState>>;