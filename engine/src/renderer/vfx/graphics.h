#pragma once

#include <vector>

#include "core/app/manager.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/drawable.h"
#include "core/ecs/entity.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "renderer/api/color.h"

namespace ptgn {

struct Transform;

namespace impl {

struct GraphicsInstance {
	struct Command {
		Transform transform;
		Shape shape;
		Color color;
		LineWidth line_width;
	};

	void AddCommand(const Transform& transform, const Shape& shape, bool fill);

	void Draw(const Transform& transform) const;

	std::vector<Command> commands_;
	Color fill_color_{ color::White };
	Color stroke_color_{ color::White };
	LineWidth line_width_;
};

} // namespace impl

class Graphics : public Entity {
public:
	Graphics() = default;
	Graphics(const Entity& entity);

	static void Draw(const Entity& entity);

	void Clear();

	void SetFillColor(const Color& color);
	void SetStrokeColor(const Color& color);
	void SetLineWidth(const LineWidth& width);

	void Line(const V2_float& start, const V2_float& end);
	void Line(const ptgn::Line& line);

	void FillRect(const Transform& transform, const Rect& rect);
	void StrokeRect(const Transform& transform, const Rect& rect);

	void FillCircle(const V2_float& center, const Circle& circle);
	void StrokeCircle(const V2_float& center, const Circle& circle);

	void FillPolygon(const Polygon& polygon);
	void StrokePolygon(const Polygon& polygon);
};

PTGN_DRAWABLE_REGISTER(Graphics);

Graphics CreateGraphics(Manager& manager, const V2_float& position = {});

} // namespace ptgn