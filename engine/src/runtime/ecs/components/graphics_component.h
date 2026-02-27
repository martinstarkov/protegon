#pragma once

#include <vector>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Renderer;
class Scene;

namespace impl {

struct GraphicsInstance {
	struct Command {
		Transform transform;
		Shape shape;
		Color color;
		FillStyle line_width;
	};

	void AddCommand(Transform transform, const Shape& shape, bool fill);

	void Draw(Renderer& renderer, Transform transform, Depth depth, BlendMode blend_mode) const;

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

	static void Draw(Renderer& renderer, Entity entity);

	void Clear();

	void SetFillColor(Color color);
	void SetStrokeColor(Color color);
	void SetLineWidth(FillStyle width);

	void Line(V2_float start, V2_float end);
	void Line(ptgn::Line line);

	void FillRect(Transform transform, Rect rect);
	void StrokeRect(Transform transform, Rect rect);

	void FillCircle(V2_float center, Circle circle);
	void StrokeCircle(V2_float center, Circle circle);

	void FillPolygon(const Polygon& polygon);
	void StrokePolygon(const Polygon& polygon);
};

PTGN_REGISTER_DRAWABLE(Graphics);

Graphics CreateGraphics(Scene& scene, V2_float position = {});

} // namespace ptgn