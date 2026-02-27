#include "runtime/ecs/components/draw.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "app/context.h"
#include "camera_component.h"
#include "core/assert.h"
#include "core/component.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
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
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_handler.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FillStyle::FillStyle(float line_width) : style{ impl::Hollow{ line_width } } {
	if (line_width == -1.0f) {
		style = impl::Solid{};
	} else if (line_width >= kMinLineWidth) {
		style = impl::Hollow{ line_width };
	} else {
		PTGN_ERROR("Invalid line width for fill style");
	}
}

FillStyle::FillStyle(impl::Solid) : style{ impl::Solid{} } {}

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

void DrawQuadTexture(
	Renderer& renderer, Texture texture, Transform transform, V2_float size, Origin draw_origin,
	Color tint, Depth depth, BlendMode blend_mode,
	const std::array<V2_float, 4>& texture_coordinates
) {
	renderer.SetBlend(blend_mode);
	auto positions{ Rect{ size }.GetWorldVertices(transform, draw_origin) };
	renderer.DrawQuadTexture(
		texture, positions, tint, static_cast<float>(depth.GetValue()), false, texture_coordinates
	);
}

static float GetFade(float diameter_y) {
	PTGN_ASSERT(diameter_y > 0.0f, "Diameter cannot be negative or zero");
	constexpr float fade_scaling_constant{ 0.12f };
	return fade_scaling_constant / diameter_y;
}

static float GetFade(V2_float diameter) {
	return GetFade(diameter.y);
}

static float NormalizeArcLineWidthToThickness(float line_width, float fade, V2_float radii) {
	if (line_width == -1.0f) {
		// Internally line width for a filled SDF is 1.0f.
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for circle");

		// Internally line width for a completely hollow ellipse is 0.0f.
		line_width = fade + line_width / std::min(radii.x, radii.y);
	}
	return line_width;
}

static float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

static float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return std::clamp(normalized_radius, 0.0f, 1.0f);
}

void DrawLines(
	Renderer& renderer, std::span<const V2_float> points, float line_width,
	const Transform& transform, Color tint, float depth
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for lines");

	for (std::size_t i = 0; i < points.size(); ++i) {
		Line l{ points[i], points[(i + 1) % points.size()] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		renderer.DrawQuad(line_points, tint, depth);
	}
}

void DrawShape(
	Renderer& renderer, const Shape& shape, Transform transform, Color tint, FillStyle fill_style,
	Origin draw_origin, Depth depth_component, BlendMode blend_mode
) {
	float line_width{ 0.0f };

	if (std::holds_alternative<Solid>(fill_style.style)) {
		if (std::holds_alternative<Line>(shape)) {
			return;
		}
		line_width = -1.0f;
	} else {
		if (auto width{ std::get<Hollow>(fill_style.style).line_width }; width >= 1.0f) {
			line_width = width;
		} else {
			return;
		}
	}

	PTGN_ASSERT(line_width != 0.0f);

	auto depth{ static_cast<float>(depth_component.GetValue()) };

	std::visit(
		[&](const auto& s) {
			using T = std::decay_t<decltype(s)>;

			if constexpr (std::is_same_v<T, Rect>) {
				if (auto size{ s.GetSize(transform) }; !size.BothAboveZero()) {
					return;
				}

				if (line_width == -1.0f) {
					renderer.DrawQuad(s.GetWorldVertices(transform, draw_origin), tint, depth);
				} else {
					DrawLines(renderer, s.GetLocalVertices(), line_width, transform, tint, depth);
				}
			} else if constexpr (std::is_same_v<T, Circle>) {
				DrawShape(
					renderer, Ellipse{ V2_float{ s.GetRadius() } }, transform, tint, fill_style,
					draw_origin, depth_component, blend_mode
				);
			} else if constexpr (std::is_same_v<T, Line>) {
				DrawLines(renderer, s.GetLocalVertices(), line_width, transform, tint, depth);
			} else if constexpr (std::is_same_v<T, Triangle>) {
				auto triangle{ s.GetWorldVertices(transform) };
				std::array<V2_float, 4> points{ triangle[0], triangle[1], triangle[2],
												triangle[0] };
				if (line_width == -1.0f) {
					renderer.DrawQuad(points, tint, depth);
				} else {
					DrawLines(renderer, s.GetLocalVertices(), line_width, transform, tint, depth);
				}

			} else if constexpr (std::is_same_v<T, Polygon>) {
				auto vertices{ s.GetLocalVertices() };

				if (vertices.size() < 3) {
					if (vertices.empty()) {
					} else if (vertices.size() == 1) {
						DrawShape(
							renderer, vertices.front(), transform, tint, fill_style, draw_origin,
							depth_component, blend_mode
						);
					} else if (vertices.size() == 2) {
						DrawShape(
							renderer, Line{ vertices[0], vertices[1] }, transform, tint, fill_style,
							draw_origin, depth_component, blend_mode
						);
						return;
					}
				}

				if (line_width == -1.0f) {
					auto points{ s.GetWorldVertices(transform) };
					auto triangles{ Triangulate(points) };
					for (const auto& triangle : triangles) {
						renderer.DrawTriangle(renderer.GetShader("color"), triangle, tint, depth);
					}
				} else {
					DrawLines(renderer, vertices, line_width, transform, tint, depth);
				}
			} else if constexpr (std::is_same_v<T, V2_float>) {
				renderer.DrawQuad(
					Rect{ V2_float{ 1.0f } }.GetWorldVertices(transform, Origin::Center), tint,
					depth
				);
			} else if constexpr (std::is_same_v<T, Capsule>) {
				auto radius{ s.GetRadius(transform) };

				if (radius <= 0.0f) {
					return;
				}

				V2_float size;
				auto positions{ s.GetWorldQuadVertices(transform, &size) };

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
				float aspect_ratio{ GetAspectRatio(size) };
				auto thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};

				std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

				renderer.DrawQuad(renderer.GetShader("capsule"), positions, data, tint, depth);
			} else if constexpr (std::is_same_v<T, Arc>) {
				auto radius{ s.GetRadius(transform) };

				if (radius <= 0.0f) {
					return;
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				float aperture{ s.GetAperture() };
				float direction{ s.clockwise ? 1.0f : -1.0f };
				std::array<float, 4> data{ thickness, fade, aperture, direction };

				transform.Rotate(s.GetStartAngle());

				auto positions{ s.GetWorldQuadVertices(transform) };

				renderer.DrawQuad(renderer.GetShader("arc"), positions, data, tint, depth);
			} else if constexpr (std::is_same_v<T, RoundedRect>) {
				auto size = s.GetSize(transform);

				if (!size.BothAboveZero()) {
					return;
				}

				float radius = s.GetRadius(transform);

				if (radius <= 0.0f) {
					DrawShape(
						renderer, Rect{ s.GetSize() }, transform, tint, fill_style, draw_origin,
						depth_component, blend_mode
					);
					return;
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
				float aspect_ratio{ GetAspectRatio(size) };
				auto thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				std::array<float, 4> data{ thickness, fade, normalized_radius, aspect_ratio };

				auto positions{ s.GetWorldQuadVertices(transform, draw_origin) };

				renderer.DrawQuad(renderer.GetShader("rounded_rect"), positions, data, tint, depth);
			} else if constexpr (std::is_same_v<T, Ellipse>) {
				auto radius = s.GetRadius(transform);

				if (!radius.BothAboveZero()) {
					return;
				}

				auto diameter{ 2.0f * radius };
				float fade{ GetFade(diameter) };
				float thickness{
					NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius })
				};
				std::array<float, 4> data{ thickness, fade, 0.0f, 0.0f };

				auto positions{ s.GetWorldQuadVertices(transform) };

				renderer.DrawQuad(renderer.GetShader("circle"), positions, data, tint, depth);
			}
		},
		shape
	);
}

template <ShapeType T>
void DrawShape(Renderer& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());
	DrawShape(
		renderer, entity.Get<T>(), GetDrawTransform(entity), GetTint(entity),
		entity.GetOrDefault<FillStyle>(), GetDrawOrigin(entity), GetDepth(entity),
		GetBlendMode(entity)
	);
}

void CapsuleDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Capsule>(renderer, entity);
}

void CircleDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Circle>(renderer, entity);
}

void EllipseDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Ellipse>(renderer, entity);
}

void ArcDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Arc>(renderer, entity);
}

void PolygonDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Polygon>(renderer, entity);
}

void RectDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Rect>(renderer, entity);
}

void RoundedRectDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<RoundedRect>(renderer, entity);
}

void TriangleDraw::Draw(Renderer& renderer, Entity entity) {
	DrawShape<Triangle>(renderer, entity);
}

void LineDraw::Draw(Renderer& renderer, Entity entity) {
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
			entity.GetScene().app().events.Emit(show);
		}
	} else {
		if (!entity.Has<impl::Visible>()) {
			return;
		}
		entity.Remove<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			EntityHide hide;
			entity.GetScene().app().events.Emit(hide);
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

V2_int GetCroppedSize(Entity entity) {
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

	return GetCroppedSize(entity) * GetScale(entity);
}

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}
	};

	if (!entity) {
		check_vertical_flip();
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize(entity) };

	if (texture_size.IsZero()) {
		check_vertical_flip();
		return tex_coords;
	}

	if (entity.Has<impl::TextureCrop>()) {
		const auto& crop{ entity.Get<impl::TextureCrop>() };
		if (crop != impl::TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
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

	check_vertical_flip();

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
