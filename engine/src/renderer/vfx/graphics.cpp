#include "renderer/vfx/graphics.h"

#include <vector>

#include "core/app/game.h"
#include "core/app/manager.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "debug/runtime/assert.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

void GraphicsInstance::AddCommand(const Transform& transform, const Shape& shape, bool fill) {
	Command cmd;

	if (fill) {
		cmd.color	   = fill_color_;
		cmd.line_width = -1.0f;
	} else {
		cmd.color	   = stroke_color_;
		cmd.line_width = line_width_;
	}

	cmd.transform = transform;
	cmd.shape	  = shape;

	commands_.emplace_back(cmd);
}

void GraphicsInstance::Draw(const Transform& transform) const {
	for (const auto& cmd : commands_) {
		game.renderer.DrawShape(
			cmd.transform.RelativeTo(transform), cmd.shape, cmd.color, cmd.line_width
		);
	}
}

} // namespace impl

Graphics::Graphics(const Entity& entity) : Entity{ entity } {}

void Graphics::Draw(const Entity& entity) {
	const auto& instance{ entity.Get<impl::GraphicsInstance>() };

	const auto& transform{ GetDrawTransform(entity) };

	instance.Draw(transform);
}

void Graphics::Clear() {
	auto& instance{ Get<impl::GraphicsInstance>() };
	instance.commands_.clear();
}

void Graphics::SetFillColor(const Color& color) {
	auto& instance{ Get<impl::GraphicsInstance>() };
	instance.fill_color_ = color;
}

void Graphics::SetStrokeColor(const Color& color) {
	auto& instance{ Get<impl::GraphicsInstance>() };
	instance.stroke_color_ = color;
}

void Graphics::SetLineWidth(const LineWidth& width) {
	auto& instance{ Get<impl::GraphicsInstance>() };
	PTGN_ASSERT(width == -1.0f || width >= impl::min_line_width, "Invalid graphics line width");
	instance.line_width_ = width;
}

void Graphics::Line(const V2_float& start, const V2_float& end) {
	Graphics::Line(ptgn::Line{ start, end });
}

void Graphics::Line(const ptgn::Line& line) {
	Get<impl::GraphicsInstance>().AddCommand({}, line, false);
}

void Graphics::FillRect(const Transform& transform, const Rect& rect) {
	Get<impl::GraphicsInstance>().AddCommand(transform, rect, true);
}

void Graphics::StrokeRect(const Transform& transform, const Rect& rect) {
	Get<impl::GraphicsInstance>().AddCommand(transform, rect, false);
}

void Graphics::FillCircle(const V2_float& position, const Circle& circle) {
	Get<impl::GraphicsInstance>().AddCommand(position, circle, true);
}

void Graphics::StrokeCircle(const V2_float& position, const Circle& circle) {
	Get<impl::GraphicsInstance>().AddCommand(position, circle, false);
}

void Graphics::FillPolygon(const Polygon& polygon) {
	Get<impl::GraphicsInstance>().AddCommand({}, polygon, true);
}

void Graphics::StrokePolygon(const Polygon& polygon) {
	Get<impl::GraphicsInstance>().AddCommand({}, polygon, false);
}

Graphics CreateGraphics(Manager& manager, const V2_float& position) {
	Graphics graphics{ manager.CreateEntity() };

	graphics.Add<impl::GraphicsInstance>();
	SetPosition(graphics, position);
	SetDraw<Graphics>(graphics);
	Show(graphics);

	return graphics;
}

} // namespace ptgn