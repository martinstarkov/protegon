#include "runtime/graphics/draw.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/flip.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "renderer/renderer.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_handler.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FillStyle::FillStyle(float line_width) : style{ impl::Hollow{ line_width } } {
	if (line_width == -1.0f) {
		style = impl::Solid{};
	} else if (line_width >= kMinLineWidth) {
		style = impl::Hollow{ line_width };
	} else {
		PTGN_ERROR("Invalid line width for fill style: ", line_width);
	}
}

FillStyle::FillStyle(impl::Solid) : style{ impl::Solid{} } {}

FillStyle FillStyle::Hollow(float line_width) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Hollow line width must be >= kMinLineWidth");
	return FillStyle{ line_width };
}

FillStyle FillStyle::Solid() {
	return FillStyle{ impl::Solid{} };
}

namespace impl {

void SetDraw(Entity entity, std::string_view drawable_name) {
	entity.Add<IDrawable>(drawable_name);
}

EntityDepthCompare::EntityDepthCompare(bool ascending) : ascending{ ascending } {}

bool EntityDepthCompare::operator()(Entity a, Entity b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return ascending ? a.WasCreatedBefore(b) : !a.WasCreatedBefore(b);
	}
	return ascending ? (depth_a < depth_b) : (depth_a > depth_b);
}

template <ShapeType T>
void DrawShape(DrawContext& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());
	renderer.SetBlend(GetBlendMode(entity));
	renderer.DrawShape(
		entity.Get<T>(), GetDrawTransform(entity), GetTint(entity),
		entity.GetOrDefault<FillStyle>(), GetDrawOrigin(entity), GetDepth(entity)
	);
}

void CapsuleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Capsule>(renderer, entity);
}

void CircleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Circle>(renderer, entity);
}

void EllipseDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Ellipse>(renderer, entity);
}

void ArcDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Arc>(renderer, entity);
}

void PolygonDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Polygon>(renderer, entity);
}

void RectDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Rect>(renderer, entity);
}

void RoundedRectDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<RoundedRect>(renderer, entity);
}

void TriangleDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Triangle>(renderer, entity);
}

void LineDraw::Draw(DrawContext& renderer, Entity entity) {
	DrawShape<Line>(renderer, entity);
}

} // namespace impl

bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>();
}

void RemoveDraw(Entity entity) {
	entity.Remove<impl::IDrawable>();
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, impl::EntityDepthCompare{ ascending });
}

void SetDrawOrigin(Entity entity, Origin origin) {
	entity.Add<Origin>(origin);
}

Origin GetDrawOrigin(Entity entity) {
	return entity.GetOrDefault<Origin>(Origin::Center);
}

void SetVisible(Entity entity, bool visible, bool emit_visibility_event) {
	if (visible) {
		if (entity.Has<impl::Visible>()) {
			return;
		}
		entity.Add<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			EntityShow show;
			entity.GetScene().app().event.Emit(show);
		}
	} else {
		if (!entity.Has<impl::Visible>()) {
			return;
		}
		entity.Remove<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			EntityHide hide;
			entity.GetScene().app().event.Emit(hide);
		}
	}
}

void Show(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, true, emit_visibility_event);
}

void Hide(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, false, emit_visibility_event);
}

bool IsVisible(Entity entity) {
	return entity.Has<impl::Visible>();
}

void SetDepth(Entity entity, Depth depth) {
	entity.Add<Depth>(depth);
}

Depth GetDepth(Entity entity) {
	// TODO: This was causing a bug with the mitosis disk background (rock texture) thing in GMTK
	// 2025. Figure out how to fix relative depths.
	/*Depth parent_depth{};
	if (HasParent(entity)) {
		auto parent{ GetParent(entity) };
		if (parent != entity && parent.Has<Depth>()) {
			parent_depth = GetDepth(parent);
		}
	}
	return parent_depth +*/
	return entity.GetOrDefault<Depth>();
}

void SetBlendMode(Entity entity, BlendMode blend_mode) {
	entity.Add<BlendMode>(blend_mode);
}

BlendMode GetBlendMode(Entity entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

void SetTint(Entity entity, Color color) {
	if (color != impl::Tint{}) {
		entity.Add<impl::Tint>(color);
	} else {
		entity.Remove<impl::Tint>();
	}
}

Color GetTint(Entity entity) {
	return entity.GetOrDefault<impl::Tint>();
}

void SetTextureSize(Entity entity, V2_float size) {
	entity.Add<impl::TextureSize>(size);
}

V2_int GetTextureSize(Entity entity) {
	std::optional<V2_int> size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_int{ entity.Get<impl::TextureSize>() };
	} else if (entity.Has<Texture>()) {
		size = entity.Get<Texture>().GetSize();
	}

	PTGN_ASSERT(size.has_value(), "Entity does not have a texture");
	PTGN_ASSERT(!(*size).IsZero(), "Texture does not have a valid size");

	return *size;
}

V2_int GetCroppedTextureSize(Entity entity) {
	if (entity.Has<impl::TextureCrop>()) {
		const auto& crop{ entity.Get<impl::TextureCrop>() };
		return crop.size;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity entity, V2_float display_size) {
	entity.Add<impl::TextureSize>(display_size);
}

V2_float GetDisplaySize(Entity entity) {
	PTGN_ASSERT(entity.Has<Texture>());

	return GetCroppedTextureSize(entity) * GetScale(entity);
}

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates(flip_vertically) };

	if (!entity) {
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize(entity) };

	if (texture_size.IsZero()) {
		return tex_coords;
	}

	if (entity.Has<impl::TextureCrop>()) {
		const auto& crop{ entity.Get<impl::TextureCrop>() };
		if (crop != impl::TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
			if (flip_vertically) {
				impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
			}
		}
	}

	auto scale{ GetScale(entity) };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (entity.Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, entity.Get<Flip>());
	}

	return tex_coords;
}

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

} // namespace ptgn

/*


template <ShapeType T>
static std::optional<QuadInfo> GetQuadInfo(Renderer& ctx, DrawShapeCommand& cmd, const T& shape) {
	QuadInfo info;

	const auto set_shader = [](DrawShapeCommand& c, std::string_view shader_name) {
		if (c.render_state.shader_pass.has_value() && *c.render_state.shader_pass != ShaderPass{}) {
			return;
		}
		c.render_state.shader_pass = Application::Get().shader.Get(shader_name);
	};

	if constexpr (std::is_same_v<T, V2_float>) {
		Transform translated = cmd.transform;
		translated.Translate(shape);

		Rect r{ V2_float{ 1.0f } };

		info.points = r.GetWorldVertices(translated, Origin::Center);
	} else if constexpr (std::is_same_v<T, Line>) {
		if (cmd.line_width < kMinLineWidth) {
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.line_width);
	} else if constexpr (std::is_same_v<T, Capsule>) {
		auto radius{ shape.GetRadius(cmd.transform) };

		if (radius <= 0.0f) {
			return std::nullopt;
		}

		V2_float size;

		info.points = shape.GetWorldQuadVertices(cmd.transform, &size);
		info.data	= GetData(shape, radius, cmd.line_width, size);

		set_shader(cmd, "capsule");
	} else if constexpr (std::is_same_v<T, Arc>) {
		auto radius{ shape.GetRadius(cmd.transform) };

		if (radius <= 0.0f) {
			return std::nullopt;
		}

		Transform rotated{ cmd.transform };
		rotated.Rotate(shape.GetStartAngle());

		info.points = shape.GetWorldQuadVertices(rotated);
		info.data	= GetData(shape, radius, cmd.line_width, {});

		set_shader(cmd, "arc");
	} else if constexpr (std::is_same_v<T, RoundedRect>) {
		auto size = shape.GetSize(cmd.transform);

		if (!size.BothAboveZero()) {
			return std::nullopt;
		}

		float radius = shape.GetRadius(cmd.transform);

		if (radius <= 0.0f) {
			cmd.render_state.shader_pass = std::nullopt;
			cmd.shape					 = Rect{ shape.GetSize() };
			ctx.DrawCommand(cmd);
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.origin);
		info.data	= GetData(shape, radius, cmd.line_width, size);

		set_shader(cmd, "rounded_rect");
	} else if constexpr (std::is_same_v<T, Ellipse>) {
		auto radius = shape.GetRadius(cmd.transform);

		if (!radius.BothAboveZero()) {
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform);
		info.data	= GetData(shape, radius, cmd.line_width, {});

		set_shader(cmd, "circle");
	} else {
		return std::nullopt;
	}

	return info;
}


template <ShapeType T>
static void DrawShape(Renderer& ctx, DrawShapeCommand cmd, const T& shape) {
	if constexpr (IsAnyOf<T, V2_float, Line, Capsule, Arc, RoundedRect, Ellipse>) {
		auto info{ GetQuadInfo(ctx, cmd, shape) };

		if (!info.has_value()) {
			return;
		}

		const auto& [points, data] = *info;

		auto quad_vertices{
			Vertex::GetQuad(points, cmd.tint, cmd.depth, data, GetDefaultTextureCoordinates())
		};

		ctx.SetState(cmd.render_state);
		ctx.AddVertices(quad_vertices, quad_indices);
	} else if constexpr (std::is_same_v<T, Circle>) {
		cmd.shape = Ellipse{ V2_float{ shape.GetRadius() } };
		ctx.DrawCommand(cmd);
	} else if constexpr (std::is_same_v<T, Rect>) {
		if (auto size{ shape.GetSize(cmd.transform) }; !size.BothAboveZero()) {
			return;
		}

		auto points = shape.GetWorldVertices(cmd.transform, cmd.origin);
		auto vertices =
			Vertex::GetQuad(points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());

		ctx.SetState(cmd.render_state);

		if (cmd.line_width == -1.0f) {
			ctx.AddVertices(vertices, quad_indices);
		} else {
			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
		}

	} else if constexpr (std::is_same_v<T, Triangle>) {
		auto points	  = shape.GetWorldVertices(cmd.transform);
		auto vertices = Vertex::GetTriangle(points, cmd.tint, cmd.depth);

		ctx.SetState(cmd.render_state);

		if (cmd.line_width == -1.0f) {
			ctx.AddVertices(vertices, triangle_indices);
		} else {
			ctx.AddLinesImpl(vertices, triangle_indices, points, cmd.line_width, {});
		}
	} else if constexpr (std::is_same_v<T, Polygon>) {
		ctx.SetState(cmd.render_state);

		if (shape.vertices.size() < 3) {
			if (shape.vertices.empty()) {
				return;
			} else if (shape.vertices.size() == 1) {
				cmd.shape = V2_float{ shape.vertices.front() };
				ctx.DrawCommand(cmd);
				return;
			} else if (shape.vertices.size() == 2) {
				cmd.shape = Line{ shape.vertices[0], shape.vertices[1] };
				ctx.DrawCommand(cmd);
				return;
			}
		}

		auto points = shape.GetWorldVertices(cmd.transform);

		if (cmd.line_width == -1.0f) {
			auto triangles{ Triangulate(points) };
			for (const auto& triangle : triangles) {
				auto vertices = Vertex::GetTriangle(triangle, cmd.tint, cmd.depth);
				ctx.AddVertices(vertices, triangle_indices);
			}
		} else {
			auto vertices =
				Vertex::GetQuad({}, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
		}
	}
}
void Renderer::DrawLines(const DrawLinesCommand& cmd) {
	std::size_t count = cmd.points.size();

	PTGN_ASSERT(cmd.line_width >= kMinLineWidth);

	PTGN_ASSERT(
		(cmd.connect_last_to_first && count >= 3) || (!cmd.connect_last_to_first && count >= 2)
	);

	std::size_t vertex_modulo = count;
	if (!cmd.connect_last_to_first) {
		vertex_modulo -= 1;
	}

	SetState(cmd.render_state);

	for (std::size_t i = 0; i < count; ++i) {
		Line l{ cmd.points[i], cmd.points[(i + 1) % vertex_modulo] };
		auto quad_points   = l.GetWorldQuadVertices(cmd.transform, cmd.line_width);
		auto quad_vertices = Vertex::GetQuad(
			quad_points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates()
		);
		AddVertices(quad_vertices, quad_indices);
	}
}

void Renderer::AddLinesImpl(
	std::span<Vertex> line_vertices, std::span<const Index> line_indices,
	std::span<const V2_float> points, float line_width, const Transform& transform
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for lines");

	for (std::size_t i = 0; i < points.size(); ++i) {
		Line l{ points[i], points[(i + 1) % points.size()] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		PTGN_ASSERT(line_vertices.size() <= line_points.size());

		for (std::size_t j = 0; j < line_vertices.size(); ++j) {
			line_vertices[j].position[0] = line_points[j].x;
			line_vertices[j].position[1] = line_points[j].y;
		}

		AddVertices(line_vertices, line_indices);
	}
}
*/
