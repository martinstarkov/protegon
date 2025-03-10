#pragma once

#include "audio/audio.h"
#include "components/camera_shake.h"
#include "components/draw.h"
#include "components/generic.h"
#include "components/lifetime.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/event.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "math/collision/collider.h"
#include "math/collision/collision.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/hash.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/noise.h"
#include "math/quaternion.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "math/vector4.h"
#include "physics/movement.h"
#include "physics/physics.h"
#include "physics/rigid_body.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "serialization/json.h"
#include "serialization/json_manager.h"
#include "tile/a_star.h"
#include "tile/grid.h"
#include "tile/layer.h"
#include "ui/button.h"
#include "ui/dropdown.h"
#include "ui/plot.h"
#include "utility/file.h"
#include "utility/log.h"
#include "utility/profiling.h"
#include "utility/timer.h"
#include "utility/tween.h"
#include "utility/utility.h"
#include "vfx/light.h"
#include "vfx/particle.h"