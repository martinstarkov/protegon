#pragma once

#include "../../ECS/Components.h"

#define IDLE_START_VELOCITY LOWEST_VELOCITY // idle starts when velocity is less than or equal to this value
#define RUN_START_FRACTION 0.6 // run starts when velocity is this fraction of terminal velocity