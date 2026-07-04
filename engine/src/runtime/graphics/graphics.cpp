#include "runtime/graphics/graphics.h"

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
#include "renderer/draw_context.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
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

} // namespace impl

Graphics::Graphics(Entity entity) : Entity{ entity } {}

void Graphics::Draw(DrawContext& ctx, Entity entity) {
	if (!entity.Has<impl::GraphicsData>()) {
		return;
	}

	const auto& instance{ entity.Get<impl::GraphicsData>() };

	auto transform{ GetDrawTransform(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	ctx.SetBlendMode(blend_mode);
	for (const auto& cmd : instance.commands_) {
		auto cmd_transform{ cmd.transform.RelativeTo(transform) };

		ctx.DrawShape(
			cmd_transform, cmd.shape, cmd.color,
			{ .depth	  = GetDepth(entity),
			  .fill_style = cmd.line_width,
			  .entity_id  = entity.GetUUID(),
			  .effects	  = impl::GetEffectParams(entity) }
		);
	}
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

Graphics CreateGraphics(Scene& scene, Transform transform) {
	Graphics graphics{ scene.CreateEntity() };
	PTGN_DEFAULT_NAME(graphics, "Graphics");

	graphics.Add<impl::GraphicsData>();
	SetTransform(graphics, transform);
	SetDraw<Graphics>(graphics);
	graphics.Add<impl::Visible>(true);

	return graphics;
}

} // namespace ptgn