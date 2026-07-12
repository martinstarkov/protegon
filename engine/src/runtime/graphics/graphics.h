#pragma once

#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

class DrawContext;
class Scene;

namespace impl {

struct GraphicsCommand {
	Transform transform;
	Shape shape;
	Color color;
	FillStyle line_width;

	PTGN_REFLECT(GraphicsCommand, transform, shape, color, line_width)
};

struct GraphicsData {
	void AddCommand(Transform transform, const Shape& shape, bool fill);

	Color fill_color_{ color::White };
	Color stroke_color_{ color::White };
	FillStyle line_width_;
	std::vector<GraphicsCommand> commands_;

	PTGN_REFLECT(GraphicsData, fill_color_, stroke_color_, line_width_, commands_)
};

} // namespace impl

class Graphics : public Entity {
public:
	Graphics() = default;
	explicit Graphics(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	Graphics& Clear();

	Graphics& SetFillColor(Color color);
	Graphics& SetStrokeColor(Color color);
	Graphics& SetLineWidth(FillStyle width);

	Graphics& Line(V2_float start, V2_float end);
	Graphics& Line(ptgn::Line line);

	Graphics& FillRect(Transform transform, Rect rect);
	Graphics& StrokeRect(Transform transform, Rect rect);

	Graphics& FillCircle(V2_float center, Circle circle);
	Graphics& StrokeCircle(V2_float center, Circle circle);

	Graphics& FillPolygon(const Polygon& polygon);
	Graphics& StrokePolygon(const Polygon& polygon);
};

PTGN_REGISTER_DRAWABLE(Graphics);

Graphics CreateGraphics(Scene& scene, Transform transform = {});

} // namespace ptgn