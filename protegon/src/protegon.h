#pragma once

#include "core/Camera.h"
#include "core/Engine.h"
#include "core/Scene.h"
#include "core/SceneManager.h"
#include "core/Window.h"
//#include "core/World.h"

#include "debugging/AllocationMetrics.h"
#include "debugging/FileManagement.h"
#include "debugging/Logger.h"

#include "ecs/Components.h"
#include "ecs/ECS.h"
#include "ecs/Systems.h"

#include "event/EventHandler.h"
#include "event/InputHandler.h"
#include "event/Inputs.h"

#include "math/Hasher.h"
#include "math/Math.h"
#include "math/Matrix.h"
#include "math/Noise.h"
#include "math/RNG.h"
#include "math/Vector2.h"

//#include "parsing/Image.h"
//#include "parsing/ImageProcessor.h"

#include "physics/Manifold.h"
#include "physics/RigidBody.h"
#include "physics/Transform.h"
#include "physics/collision/Collision.h"
//#include "physics/collision/dynamic/DynamicAABBvsAABB.h"
//#include "physics/collision/static/AABBvsAABB.h"
//#include "physics/collision/static/CirclevsAABB.h"
//#include "physics/collision/static/CirclevsCircle.h"
//#include "physics/collision/static/LinevsAABB.h"
//#include "physics/collision/static/PointvsAABB.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "physics/shapes/Shape.h"

#include "procedural/Chunk.h"
#include "procedural/ChunkManager.h"

#include "renderer/Color.h"
#include "renderer/Colors.h"
#include "renderer/Renderer.h"
#include "renderer/Surface.h" // TEMPORARY?
#include "renderer/Texture.h" // TEMPORARY?
#include "renderer/TextureManager.h"
//#include "renderer/particles/Particle.h"
//#include "renderer/particles/ParticleManager.h"
//#include "renderer/sprites/Animation.h"
#include "renderer/sprites/Flip.h"
#include "renderer/sprites/PixelFormat.h"
//#include "renderer/sprites/SpriteMap.h"
#include "renderer/text/Font.h" // TEMPORARY?
#include "renderer/text/FontManager.h"
#include "renderer/text/FontRenderMode.h"
#include "renderer/text/FontStyle.h"
#include "renderer/text/Text.h"

//#include "statemachine/State.h"
//#include "statemachine/StateMachine.h"
// MORE?

#include "utils/Direction.h"
#include "utils/Singleton.h"
#include "utils/Timer.h"
#include "utils/TypeTraits.h"