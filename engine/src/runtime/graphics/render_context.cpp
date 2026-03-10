#include "runtime/graphics/render_context.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/graphics/draw.h"
#include "runtime/scene/scene.h"

namespace ptgn {

DrawContext::DrawContext(Renderer& renderer) : renderer_{ renderer } {}

void DrawContext::DrawLine(
	impl::ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
) {
	renderer_.DrawLine(shader, positions, tint, depth);
}

void DrawContext::DrawTriangle(
	impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
) {
	renderer_.DrawTriangle(shader, positions, tint, depth);
}

void DrawContext::DrawQuad(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions,
	const std::array<float, 4>& user_data, Color tint, float depth
) {
	renderer_.DrawQuad(shader, positions, user_data, tint, depth);
}

void DrawContext::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, const std::array<V2_float, 4>& tex_coords
) {
	renderer_.DrawTexture(shader, texture, positions, tint, depth, tex_coords);
}

void DrawContext::DrawTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	const std::array<V2_float, 4>& tex_coords
) {
	auto quad_shader{ GetShader("quad") };
	DrawTexture(quad_shader, texture, positions, tint, depth, tex_coords);
}

void DrawContext::DrawQuad(const std::array<V2_float, 4>& positions, Color tint, float depth) {
	auto white_texture{ GetWhiteTexture() };
	DrawTexture(white_texture, positions, tint, depth, impl::GetDefaultTextureCoordinates(false));
}

void DrawContext::BindScreenTarget() {
	return renderer_.BindScreenTarget();
}

void DrawContext::SetViewport(Viewport viewport) {
	renderer_.SetViewport(viewport);
}

void DrawContext::SetViewProjection(const Matrix4& view_projection) {
	renderer_.SetViewProjection(view_projection);
}

void DrawContext::SetBlend(BlendMode mode, bool enabled) {
	renderer_.SetBlend(mode, enabled);
}

void DrawContext::SetDepth(const DepthState& depth) {
	renderer_.SetDepth(depth);
}

void DrawContext::SetStencil(const StencilState& stencil) {
	renderer_.SetStencil(stencil);
}

void DrawContext::SetRaster(const RasterState& raster) {
	renderer_.SetRaster(raster);
}

void DrawContext::SetScissor(const ScissorState& scissor) {
	renderer_.SetScissor(scissor);
}

void DrawContext::SetColorMask(const ColorMaskState& color_mask) {
	renderer_.SetColorMask(color_mask);
}

impl::RenderPass DrawContext::BeginPass(const impl::RenderTargetData& scene_target) {
	return renderer_.BeginPass(scene_target);
}

impl::TextureId DrawContext::GetWhiteTexture() const {
	return renderer_.GetWhiteTexture();
}

impl::ShaderId DrawContext::GetShader(std::string_view name) const {
	return renderer_.GetShader(name);
}

void DrawContext::Draw(const impl::ManualCommand& command, float depth) {
	std::visit(
		[&](auto& draw) {
			using T = std::decay_t<decltype(draw)>;
			if constexpr (std::is_same_v<T, impl::TextureCommand>) {
				DrawTexture(
					draw.shader, draw.texture, draw.positions, draw.tint, depth, draw.tex_coords
				);
			} else if constexpr (std::is_same_v<T, impl::QuadCommand>) {
				DrawQuad(draw.positions, draw.color, depth);
			} else if constexpr (std::is_same_v<T, impl::LineCommand>) {
				DrawLine(draw.shader, draw.positions, draw.color, depth);
			} else if constexpr (std::is_same_v<T, impl::TriangleCommand>) {
				DrawTriangle(draw.shader, draw.positions, draw.color, depth);
			} else {
				PTGN_ERROR("Invalid draw command type");
			}
		},
		command
	);
}

void DrawContext::DrawTexture(
	Texture texture, Transform transform, V2_float size, Origin draw_origin, Color tint,
	float depth, const std::array<V2_float, 4>& texture_coordinates
) {
	auto positions{ Rect{ size }.GetWorldVertices(transform, draw_origin) };
	DrawTexture(texture, positions, tint, depth, texture_coordinates);
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

void DrawContext::DrawLines(
	std::span<const V2_float> points, float line_width, Transform transform, Color tint, float depth
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Invalid line width for lines");

	for (std::size_t i = 0; i < points.size(); ++i) {
		Line l{ points[i], points[(i + 1) % points.size()] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		DrawQuad(line_points, tint, depth);
	}
}

void DrawContext::DrawShape(
	const Shape& shape, Transform transform, Color tint, FillStyle fill_style, Origin draw_origin,
	float depth
) {
	float line_width{ 0.0f };

	if (std::holds_alternative<impl::Solid>(fill_style.style)) {
		PTGN_ASSERT(
			!std::holds_alternative<Line>(shape),
			"Cannot draw a solid line, use FillStyle::Hollow(line_width)"
		);
		line_width = -1.0f;
	} else {
		if (auto width{ std::get<impl::Hollow>(fill_style.style).line_width }; width >= 1.0f) {
			line_width = width;
		} else {
			return;
		}
	}

	PTGN_ASSERT(line_width != 0.0f);

	std::visit(
		[&](const auto& s) {
			using T = std::decay_t<decltype(s)>;

			if constexpr (std::is_same_v<T, Rect>) {
				if (auto size{ s.GetSize(transform) }; !size.BothAboveZero()) {
					return;
				}

				if (line_width == -1.0f) {
					DrawQuad(s.GetWorldVertices(transform, draw_origin), tint, depth);
				} else {
					DrawLines(
						s.GetWorldVertices({}, draw_origin), line_width, transform, tint, depth
					);
				}
			} else if constexpr (std::is_same_v<T, Circle>) {
				DrawShape(
					Ellipse{ V2_float{ s.GetRadius() } }, transform, tint, fill_style, draw_origin,
					depth
				);
			} else if constexpr (std::is_same_v<T, Line>) {
				DrawLines(s.GetLocalVertices(), line_width, transform, tint, depth);
			} else if constexpr (std::is_same_v<T, Triangle>) {
				auto triangle{ s.GetWorldVertices(transform) };
				std::array<V2_float, 4> points{ triangle[0], triangle[1], triangle[2],
												triangle[0] };
				if (line_width == -1.0f) {
					DrawQuad(points, tint, depth);
				} else {
					DrawLines(s.GetLocalVertices(), line_width, transform, tint, depth);
				}

			} else if constexpr (std::is_same_v<T, Polygon>) {
				auto vertices{ s.GetLocalVertices() };

				if (vertices.size() < 3) {
					if (vertices.empty()) {
					} else if (vertices.size() == 1) {
						DrawShape(
							vertices.front(), transform, tint, fill_style, draw_origin, depth
						);
					} else if (vertices.size() == 2) {
						DrawShape(
							Line{ vertices[0], vertices[1] }, transform, tint, fill_style,
							draw_origin, depth
						);
						return;
					}
				}

				if (line_width == -1.0f) {
					auto points{ s.GetWorldVertices(transform) };
					auto triangles{ impl::Triangulate(points) };
					for (const auto& triangle : triangles) {
						DrawTriangle(GetShader("color"), triangle, tint, depth);
					}
				} else {
					DrawLines(vertices, line_width, transform, tint, depth);
				}
			} else if constexpr (std::is_same_v<T, V2_float>) {
				DrawQuad(
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

				DrawQuad(GetShader("capsule"), positions, data, tint, depth);
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

				DrawQuad(GetShader("arc"), positions, data, tint, depth);
			} else if constexpr (std::is_same_v<T, RoundedRect>) {
				auto size = s.GetSize(transform);

				if (!size.BothAboveZero()) {
					return;
				}

				float radius = s.GetRadius(transform);

				if (radius <= 0.0f) {
					DrawShape(Rect{ s.GetSize() }, transform, tint, fill_style, draw_origin, depth);
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

				DrawQuad(GetShader("rounded_rect"), positions, data, tint, depth);
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

				DrawQuad(GetShader("circle"), positions, data, tint, depth);
			}
		},
		shape
	);
}

void RenderContext::Init(Scene& scene, Renderer& renderer) {
	scene_	  = &scene;
	renderer_ = &renderer;
}

} // namespace ptgn