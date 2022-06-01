#pragma once

#include "core/Camera.h"
#include "core/Engine.h"
#include "core/Scene.h"
#include "core/SceneManager.h"
#include "core/Window.h"
#include "core/ECS.h"

#include "debugging/AllocationMetrics.h"
#include "debugging/FileManagement.h"
#include "debugging/Logger.h"
#include "debugging/DebugRenderer.h"

#include "components/CameraComponent.h"
#include "components/ColorComponent.h"
#include "components/HitboxComponent.h"
#include "components/LifetimeComponent.h"
#include "components/RigidBodyComponent.h"
#include "components/ShapeComponent.h"
#include "components/SpriteComponent.h"
#include "components/TagComponent.h"
#include "components/Tags.h"
#include "components/TransformComponent.h"

#include "event/EventHandler.h"
#include "event/InputHandler.h"
#include "event/Inputs.h"

#include "math/Math.h"
#include "math/Noise.h"
#include "math/RNG.h"
#include "math/Vector2.h"

#include "physics/Manifold.h"
#include "physics/RigidBody.h"
#include "physics/Transform.h"
#include "physics/collision/Collision.h"
#include "physics/collision/PointvsAABB.h"
#include "physics/collision/LinevsAABB.h"
#include "physics/collision/AABBvsAABB.h"
#include "physics/collision/CirclevsAABB.h"
#include "physics/collision/DynamicAABBvsAABB.h"
#include "physics/collision/CirclevsCircle.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "physics/shapes/Shape.h"

#include "renderer/Color.h"
#include "renderer/Colors.h"
#include "renderer/ScreenRenderer.h"
#include "renderer/WorldRenderer.h"
//#include "renderer/Surface.h"
#include "renderer/Texture.h"
#include "renderer/TextureManager.h"
#include "renderer/PixelFormat.h"
#include "renderer/particle/Particle.h"
#include "renderer/particle/ParticleManager.h"
#include "renderer/sprite/Animation.h"
#include "renderer/sprite/AnimationMap.h"
#include "renderer/sprite/Flip.h"
#include "renderer/text/Font.h"
#include "renderer/text/FontManager.h"
#include "renderer/text/FontRenderMode.h"
#include "renderer/text/FontStyle.h"
#include "renderer/text/Text.h"

#include "systems/DrawShapeSystem.h"
#include "systems/StaticCollisionSystem.h"
#include "systems/MovementSystem.h"
#include "systems/LifetimeSystem.h"

#include "utils/Countdown.h"
#include "utils/Direction.h"
#include "utils/Singleton.h"
#include "utils/Timer.h"
#include "utils/TypeTraits.h"

#include "world/Chunk.h"
#include "world/ChunkManager.h"
#include "world/Level.h"
#include "world/LevelManager.h"