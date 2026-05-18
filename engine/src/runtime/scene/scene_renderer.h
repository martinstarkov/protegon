#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn {

namespace impl {} // namespace impl

} // namespace ptgn