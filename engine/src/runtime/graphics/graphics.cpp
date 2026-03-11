#include "runtime/graphics/graphics.h"

#include <vector>

#include "core/math/geometry/circle.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void GraphicsData::AddCommand(Transform transform, const Shape& shape, bool fill) {
	Command cmd;

	if (fill) {
		cmd.color	   = fill_color_;
		cmd.line_width = FillStyle::Solid();
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

void Graphics::Draw(DrawContext& renderer, Entity entity, [[maybe_unused]] Camera) {
	const auto& instance{ entity.Get<impl::GraphicsData>() };

	const auto& transform{ GetDrawTransform(entity) };

	instance.Draw(renderer, transform, GetDepth(entity), GetBlendMode(entity));
}

void Graphics::Clear() {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.commands_.clear();
}

void Graphics::SetFillColor(Color color) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.fill_color_ = color;
}

void Graphics::SetStrokeColor(Color color) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.stroke_color_ = color;
}

void Graphics::SetLineWidth(FillStyle width) {
	auto& instance{ Get<impl::GraphicsData>() };
	instance.line_width_ = width;
}

void Graphics::Line(V2_float start, V2_float end) {
	Graphics::Line(ptgn::Line{ start, end });
}

void Graphics::Line(ptgn::Line line) {
	Get<impl::GraphicsData>().AddCommand({}, line, false);
}

void Graphics::FillRect(Transform transform, Rect rect) {
	Get<impl::GraphicsData>().AddCommand(transform, rect, true);
}

void Graphics::StrokeRect(Transform transform, Rect rect) {
	Get<impl::GraphicsData>().AddCommand(transform, rect, false);
}

void Graphics::FillCircle(V2_float position, Circle circle) {
	Get<impl::GraphicsData>().AddCommand(position, circle, true);
}

void Graphics::StrokeCircle(V2_float position, Circle circle) {
	Get<impl::GraphicsData>().AddCommand(position, circle, false);
}

void Graphics::FillPolygon(const Polygon& polygon) {
	Get<impl::GraphicsData>().AddCommand({}, polygon, true);
}

void Graphics::StrokePolygon(const Polygon& polygon) {
	Get<impl::GraphicsData>().AddCommand({}, polygon, false);
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