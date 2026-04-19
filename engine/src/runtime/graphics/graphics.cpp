#include "runtime/graphics/graphics.h"

#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void GraphicsData::AddCommand(Transform transform, const Shape& shape, bool fill) {
	Command cmd;

	if (fill) {
		cmd.color	   = fill_color_;
		cmd.line_width = Solid{};
	} else {
		cmd.color	   = stroke_color_;
		cmd.line_width = line_width_;
	}

	cmd.transform = transform;
	cmd.shape	  = shape;

	commands_.emplace_back(cmd);
}

void GraphicsData::Draw(
	DrawContext& renderer, Transform transform, Depth depth, BlendMode blend_mode
) const {
	for (const auto& cmd : commands_) {
		renderer.DrawShape(
			cmd.shape, cmd.transform.RelativeTo(transform), cmd.color, cmd.line_width,
			Origin::Center, depth, blend_mode
		);
	}
}

} // namespace impl

Graphics::Graphics(Entity entity) : Entity{ entity } {}

void Graphics::Draw(DrawContext& renderer, Entity entity) {
	const auto& instance{ entity.Get<impl::GraphicsData>() };

	const auto& transform{ GetDrawTransform(entity) };

	instance.Draw(renderer, transform, GetDepth(entity), GetBlendMode(entity));
}

Graphics& Graphics::Clear() {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.commands_.clear();
	return *this;
}

Graphics& Graphics::SetFillColor(Color color) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.fill_color_ = color;
	return *this;
}

Graphics& Graphics::SetStrokeColor(Color color) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.stroke_color_ = color;
	return *this;
}

Graphics& Graphics::SetLineWidth(FillStyle width) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.line_width_ = width;
	return *this;
}

Graphics& Graphics::Line(V2_float start, V2_float end) {
	return Graphics::Line(ptgn::Line{ start, end });
}

Graphics& Graphics::Line(ptgn::Line line) {
	Get<impl::GraphicsData>().AddCommand({}, line, false);
	return *this;
}

Graphics& Graphics::FillRect(Transform transform, Rect rect) {
	Get<impl::GraphicsData>().AddCommand(transform, rect, true);
	return *this;
}

Graphics& Graphics::StrokeRect(Transform transform, Rect rect) {
	Get<impl::GraphicsData>().AddCommand(transform, rect, false);
	return *this;
}

Graphics& Graphics::FillCircle(V2_float position, Circle circle) {
	Get<impl::GraphicsData>().AddCommand(position, circle, true);
	return *this;
}

Graphics& Graphics::StrokeCircle(V2_float position, Circle circle) {
	Get<impl::GraphicsData>().AddCommand(position, circle, false);
	return *this;
}

Graphics& Graphics::FillPolygon(const Polygon& polygon) {
	Get<impl::GraphicsData>().AddCommand({}, polygon, true);
	return *this;
}

Graphics& Graphics::StrokePolygon(const Polygon& polygon) {
	Get<impl::GraphicsData>().AddCommand({}, polygon, false);
	return *this;
}

Graphics CreateGraphics(Scene& scene, V2_float position) {
	Graphics graphics{ scene.CreateEntity() };

	graphics.Add<impl::GraphicsData>();
	SetPosition(graphics, position);
	SetDraw<Graphics>(graphics);
	Show(graphics, false);

	return graphics;
}

} // namespace ptgn