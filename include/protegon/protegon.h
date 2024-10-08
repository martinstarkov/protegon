#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include "components/collider.h"
#include "components/lifetime.h"
#include "components/rigid_body.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "protegon/a_star.h"
#include "protegon/audio.h"
#include "protegon/buffer.h"
#include "protegon/button.h"
#include "protegon/circle.h"
#include "protegon/collision.h"
#include "protegon/color.h"
#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/file.h"
#include "protegon/font.h"
#include "protegon/game.h"
#include "protegon/grid.h"
#include "protegon/hash.h"
#include "protegon/layer.h"
#include "protegon/line.h"
#include "protegon/log.h"
#include "protegon/math.h"
#include "protegon/matrix4.h"
#include "protegon/noise.h"
#include "protegon/polygon.h"
#include "protegon/quaternion.h"
#include "protegon/rng.h"
#include "protegon/scene.h"
#include "protegon/shader.h"
#include "protegon/surface.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "protegon/timer.h"
#include "protegon/tween.h"
#include "protegon/vector2.h"
#include "protegon/vector3.h"
#include "protegon/vector4.h"
#include "protegon/vertex_array.h"
