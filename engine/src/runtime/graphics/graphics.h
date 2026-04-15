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
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;
class Scene;

namespace impl {

struct GraphicsData {
	struct Command {
		Transform transform;
		Shape shape;
		Color color;
		FillStyle line_width;
	};

	void AddCommand(Transform transform, const Shape& shape, bool fill);

	void Draw(DrawContext& renderer, Transform transform, Depth depth, BlendMode blend_mode) const;

	std::vector<Command> commands_;
	Color fill_color_{ color::White };
	Color stroke_color_{ color::White };
	FillStyle line_width_;
};

} // namespace impl

class Graphics : public Entity {
public:
	Graphics() = default;
	explicit Graphics(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);

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

Graphics CreateGraphics(Scene& scene, V2_float position = {});

} // namespace ptgn