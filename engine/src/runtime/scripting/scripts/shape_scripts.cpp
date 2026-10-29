#include "runtime/scripting/scripts/shape_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void ShapeSetColorScript::OnStart() {
	if (!target) {
		return;
	}
	target.Add<Color>(selected_color);
}

void ShapeSetFillStyleScript::OnStart() {
	if (!target) {
		return;
	}
	SetFillStyle(target, option ? FillStyle{ Solid{} } : FillStyle{ std::max(1.0f, number) });
}

void ShapeSetStrokeWidthScript::OnStart() {
	if (!target) {
		return;
	}
	SetFillStyle(target, FillStyle{ std::max(1.0f, number) });
}

void ShapeSetGeometryScript::OnStart() {
	if (!target) {
		return;
	}
	selected_shape.Visit([&]<typename T>(const T& geometry) {
		if constexpr (!std::same_as<T, V2_float>) {
			if (auto* shape{ target.TryGet<T>() }) {
				*shape = geometry;
			}
		}
	});
}

void ShapeRectSetBoundsOrSizeScript::OnStart() {
	if (!target) {
		return;
	}
	auto* rect{ target.TryGet<Rect>() };
	if (!rect || (vector2 - vector).HasNegative()) {
		return;
	}
	*rect = Rect{ vector, vector2 };
}

void ShapeRectSetCenterScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* rect{ target.TryGet<Rect>() }) {
		*rect = rect->Translated(vector - rect->GetCenter());
	}
}

void ShapeCircleSetDiameterScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Circle>() }) {
		shape->radius = std::max(0.0f, number) * 0.5f;
	}
}

void ShapeRoundedRectSetCornerRadiusScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<RoundedRect>() }) {
		shape->radius = std::max(0.0f, number);
	}
}

void ShapePolygonSetVerticesScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Polygon>() }; shape && vertices.size() >= 3) {
		shape->vertices = vertices;
	}
}

void ShapePolygonSetVertexScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Polygon>() };
		shape && index >= 0 && static_cast<std::size_t>(index) < shape->vertices.size()) {
		shape->vertices[static_cast<std::size_t>(index)] = vector;
	}
}

void ShapeEllipseSetRadiiScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Ellipse>() }) {
		shape->radius = vector;
	}
}

void ShapeEllipseSetWidthScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Ellipse>() }) {
		shape->radius.x = number * 0.5f;
	}
}

void ShapeEllipseSetHeightScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Ellipse>() }) {
		shape->radius.y = number * 0.5f;
	}
}

void ShapeTriangleSetVerticesScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Triangle>() }; shape && vertices.size() == 3) {
		std::copy(vertices.begin(), vertices.end(), shape->vertices.begin());
	}
}

void ShapeTriangleSetVertexScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Triangle>() }; shape && index >= 0 && index < 3) {
		shape->vertices[static_cast<std::size_t>(index)] = vector;
	}
}

void ShapeLineSetStartScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Line>() }) {
		shape->start = vector;
	}
}

void ShapeLineSetEndScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Line>() }) {
		shape->end = vector;
	}
}

void ShapeLineSetEndpointsScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Line>() }) {
		*shape = Line{ vector, vector2 };
	}
}

void ShapeLineSetWidthScript::OnStart() {
	if (!target) {
		return;
	}
	SetFillStyle(target, FillStyle{ std::max(1.0f, number) });
}

void ShapeCapsuleSetStartScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Capsule>() }) {
		shape->line.start = vector;
	}
}

void ShapeCapsuleSetEndScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Capsule>() }) {
		shape->line.end = vector;
	}
}

void ShapeCapsuleSetRadiusScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Capsule>() }) {
		shape->radius = std::max(0.0f, number);
	}
}

void ShapeCapsuleSetLengthScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Capsule>() }) {
		V2_float direction{ shape->line.GetDirection() };
		float length{ direction.Magnitude() };
		if (length > 0.0f) {
			shape->line.end = shape->line.start + direction * (std::max(0.0f, number) / length);
		}
	}
}

void ShapeArcSetRadiusScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Arc>() }) {
		shape->radius = std::max(0.0f, number);
	}
}

void ShapeArcSetStartAngleScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Arc>() }) {
		shape->start_angle = Degrees{ number }.ToRad();
	}
}

void ShapeArcSetEndAngleScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Arc>() }) {
		shape->end_angle = Degrees{ number }.ToRad();
	}
}

void ShapeArcSetDirectionScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* shape{ target.TryGet<Arc>() }) {
		shape->clockwise = option;
	}
}

void ShapeSetSizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (vector.HasNegative()) {
		return;
	}
	if (auto* rect{ target.TryGet<Rect>() }) {
		V2_float center{ rect->GetCenter() };
		*rect = Rect{ center - vector * 0.5f, center + vector * 0.5f };
	} else if (auto* rounded{ target.TryGet<RoundedRect>() }) {
		V2_float center{ rounded->rect.GetCenter() };
		rounded->rect = Rect{ center - vector * 0.5f, center + vector * 0.5f };
	} else if (auto* circle{ target.TryGet<Circle>() }) {
		circle->radius = std::max(0.0f, vector.x) * 0.5f;
	} else if (auto* ellipse{ target.TryGet<Ellipse>() }) {
		ellipse->radius = vector * 0.5f;
	}
}

void ShapeSetRadiusScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* rounded{ target.TryGet<RoundedRect>() }) {
		rounded->radius = std::max(0.0f, number);
	} else if (auto* circle{ target.TryGet<Circle>() }) {
		circle->radius = std::max(0.0f, number);
	} else if (auto* ellipse{ target.TryGet<Ellipse>() }) {
		ellipse->radius = V2_float{ std::max(0.0f, number) };
	} else if (auto* capsule{ target.TryGet<Capsule>() }) {
		capsule->radius = std::max(0.0f, number);
	} else if (auto* arc{ target.TryGet<Arc>() }) {
		arc->radius = std::max(0.0f, number);
	}
}

} // namespace ptgn
