#include <engine/Include.h>

#include "states/FiringState.h"
#include "states/StandbyState.h"
#include "states/DisabledState.h"

class FiringStateMachine : public engine::StateMachine {
public:
	FiringStateMachine(ecs::Entity entity) {
		AddState("disabled", std::make_shared<DisabledState>());
		AddState("standby", std::make_shared<StandbyState>());
		AddState("firing", std::make_shared<FiringState>());
		Init(entity);
	}
};