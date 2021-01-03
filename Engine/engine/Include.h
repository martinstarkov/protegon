#pragma once

#include "core/Engine.h"
#include "core/Scene.h"
#include "core/Chunk.h"

#include "ecs/ECS.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"

#include "statemachine/StateMachine.h"
#include "statemachine/states/State.h"

#include "renderer/Window.h"
#include "renderer/Renderer.h"
#include "renderer/TextureManager.h"
#include "renderer/FontManager.h"
#include "renderer/SpriteMap.h"

#include "parsing/ImageProcessor.h"

#include "physics/collision/CollisionFunctions.h"

#include "event/InputHandler.h"
#include "event/EventHandler.h"
#include "event/Events.h"

#include "ui/UI.h"
#include "ui/UIComponents.h"

#include "utils/Vector2.h"
#include "utils/Math.h"
#include "utils/RNG.h"
#include "utils/PerlinNoise.h"
#include "utils/Hasher.h"
#include "utils/Logger.h"
#include "utils/Timer.h"
#include "utils/Defines.h"
#include "utils/AllocationMetrics.h"