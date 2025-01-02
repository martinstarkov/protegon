#include "renderer/render_data.h"

#include "renderer/vertices.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/vertex_array.h"
#include "utility/utility.h"
#include "core/game.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "math/geometry/polygon.h"
#include "math/geometry/line.h"
#include "math/geometry/circle.h"

namespace ptgn {

namespace impl {

Batch::Batch(const Texture& texture) {
	textures_.emplace_back(texture);
}

void Batch::BindTextures() const {
	for (std::int32_t i{ 0 }; i < static_cast<std::int32_t>(textures_.size()); i++) {
		// Save first texture slot for empty white texture.
		std::int32_t slot{ i + 1 };
		textures_[i].Bind(slot);
	}
}

float Batch::GetAvailableTextureIndex(const Texture& texture) {
	PTGN_ASSERT(!RenderData::IsBlank(texture));
	PTGN_ASSERT(texture.IsValid());
	// Texture exists in batch, therefore do not add it again.
	for (std::int32_t i{ 0 }; i < static_cast<std::int32_t>(textures_.size()); i++) {
		if (textures_[i] == texture) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	if (static_cast<std::uint32_t>(textures_.size()) == game.renderer.max_texture_slots_ - 1) {
		// Texture does not exist in batch and batch is full.
		return 0.0f;
	}
	// Texture does not exist in batch but can be added.
	textures_.emplace_back(texture);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(textures_.size());
}

RenderData::RenderData() : batch_capacity_{ game.renderer.batch_capacity_ } {
	PTGN_ASSERT(batch_capacity_ >= 1);
	quad_vao_	  = VertexArray{ PrimitiveMode::Triangles,
						VertexBuffer{ (std::array<QuadVertex, 4>*)nullptr,
								batch_capacity_, BufferUsage::StreamDraw },
						quad_vertex_layout, game.renderer.quad_ib_ };
	circle_vao_	  = VertexArray{ PrimitiveMode::Triangles,
						VertexBuffer{ (std::array<CircleVertex, 4>*)nullptr,
									batch_capacity_, BufferUsage::StreamDraw },
						circle_vertex_layout, game.renderer.quad_ib_ };
	triangle_vao_ = VertexArray{ PrimitiveMode::Triangles,
						VertexBuffer{ (std::array<ColorVertex, 3>*)nullptr,
									batch_capacity_, BufferUsage::StreamDraw },
						color_vertex_layout, game.renderer.triangle_ib_ };
	line_vao_	  = VertexArray{ PrimitiveMode::Lines,
						VertexBuffer{ (std::array<ColorVertex, 2>*)nullptr,
								batch_capacity_, BufferUsage::StreamDraw },
						color_vertex_layout, game.renderer.line_ib_ };
	point_vao_	  = VertexArray{ PrimitiveMode::Points,
						VertexBuffer{ (std::array<ColorVertex, 1>*)nullptr,
									batch_capacity_, BufferUsage::StreamDraw },
						color_vertex_layout, game.renderer.point_ib_ };
}

// Assumes view_projection_ is updated externally.
void RenderData::Flush() {
	PTGN_ASSERT(game.renderer.quad_shader_.IsValid());
	PTGN_ASSERT(game.renderer.circle_shader_.IsValid());
	PTGN_ASSERT(game.renderer.color_shader_.IsValid());

	game.renderer.white_texture_.Bind(0);

	if (transparent_layers_.empty()) {
		return;
	}

	if (refresh_view_projection_) {
		game.renderer.circle_shader_.Bind();
		game.renderer.circle_shader_.SetUniform("u_ViewProjection", view_projection_);
		game.renderer.color_shader_.Bind();
		game.renderer.color_shader_.SetUniform("u_ViewProjection", view_projection_);
		game.renderer.quad_shader_.Bind();
		game.renderer.quad_shader_.SetUniform("u_ViewProjection", view_projection_);
		refresh_view_projection_ = false;
	}

	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	// auto was_depth_testing{ GLRenderer::IsDepthTestingEnabled() };
	// auto was_depth_writing{ GLRenderer::IsDepthWritingEnabled() };
	// if (!was_depth_testing) {
	//	GLRenderer::EnableDepthTesting();
	// }
	// if (!was_depth_writing) {
	//	GLRenderer::EnableDepthWriting();
	// }
	// FlushBatches(opaque_batches_);
	// opaque_batches_.clear();
	// TODO: Check which of these is necessary to be disabled for the transparent batch below.
	// GLRenderer::DisableDepthWriting();
	// GLRenderer::DisableDepthTesting();
	// TODO: Make sure to uncomment re-enabling of depth writing after transparent batch is done
	// flushing (see below).
	// PTGN_ASSERT(!new_view_projection_, "Opaque batch should have handled view projection
	// reset");

	// Flush transparent layers in order of render layer.
	for (const auto& [render_layer, batches] : transparent_layers_) {
		FlushBatches(batches);
	}

	// TODO: Re-enable when opaque batching is figured out
	// if (was_depth_testing) {
	//	GLRenderer::EnableDepthTesting();
	// }
	// if (was_depth_writing) {
	//	GLRenderer::EnableDepthWriting();
	// }

	transparent_layers_.clear();
}

void RenderData::SetViewProjection(const M4_float& view_projection) {
	view_projection_		 = view_projection;
	refresh_view_projection_ = true;
}

void RenderData::AddTexture(
	const std::array<V2_float, 4>& vertices, const Texture& texture,
	const std::array<V2_float, 4>& tex_coords, const V4_float& tint_color,
	std::int32_t render_layer
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw uninitialized or destroyed texture");
	AddPrimitiveQuad(vertices, render_layer, tint_color, tex_coords, texture);
}

void RenderData::AddEllipse(
	const Ellipse& ellipse, const V4_float& color, float line_width, float fade,
	std::int32_t render_layer
) {
	PTGN_ASSERT(line_width >= 0.0f || line_width == -1.0f, "Cannot draw negative line width");

	Rect rect{ ellipse.center, ellipse.radius * 2.0f, Origin::Center, 0.0f };

	// Internally line width for a filled ellipse is 1.0f and a completely hollow one is 0.0f,
	// but in the API the line width is expected in pixels.
	// TODO: Check that dividing by std::max(radius.x, radius.y) does not cause any unexpected
	// bugs.
	line_width = NearlyEqual(line_width, -1.0f)
					? 1.0f
					: fade + line_width / std::min(ellipse.radius.x, ellipse.radius.y);

	AddPrimitiveCircle(
		rect.GetVertices(V2_float{ 0.5f, 0.5f }), render_layer, color, line_width, fade
	);
}

void RenderData::AddLine(
	const Line& line, const V4_float& color, float line_width, std::int32_t render_layer
) {
	PTGN_ASSERT(line_width >= 0.0f, "Cannot draw negative line width");

	if (line_width <= 1.0f) {
		AddPrimitiveLine({ line.a, line.b }, render_layer, color);
		return;
	}

	V2_float dir{ line.Direction() };
	// TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	Rect rect{ line.a + dir * 0.5f, V2_float{ dir.Magnitude(), line_width }, Origin::Center,
				dir.Angle() };
	AddRect(rect.GetVertices(V2_float{ 0.5f, 0.5f }), color, -1.0f, render_layer);
}

void RenderData::AddPoint(
	const V2_float& point, const V4_float& color, float radius, float fade,
	std::int32_t render_layer
) {
	if (radius <= 1.0f) {
		AddPrimitivePoint({ point }, render_layer, color);
	} else {
		AddEllipse({ point, { radius, radius } }, color, -1.0f, fade, render_layer);
	}
}

void RenderData::AddTriangle(
	const Triangle& triangle, const V4_float& color, float line_width, std::int32_t render_layer
) {
	if (line_width == -1.0f) {
		AddPrimitiveTriangle({ triangle.a, triangle.b, triangle.c }, render_layer, color);
	} else {
		PTGN_ASSERT(line_width >= 0.0f, "Cannot draw negative thickness triangle");
		std::array<V2_float, 3> vertices{ triangle.a, triangle.b, triangle.c };
		AddPolygon(Polygon{ ToVector(vertices) }, color, line_width, render_layer);
	}
}

void RenderData::AddRect(
	const std::array<V2_float, 4>& vertices, const V4_float& color, float line_width,
	std::int32_t render_layer
) {
	if (line_width == -1.0f) {
		AddTexture(
			vertices, game.renderer.white_texture_,
			{
				V2_float{ 0.0f, 0.0f },
				V2_float{ 1.0f, 0.0f },
				V2_float{ 1.0f, 1.0f },
				V2_float{ 0.0f, 1.0f },
			},
			color, render_layer
		);
	} else {
		for (std::size_t i{ 0 }; i < vertices.size(); i++) {
			AddLine(
				{ vertices[i], vertices[(i + 1) % vertices.size()] }, color, line_width,
				render_layer
			);
		}
	}
}

void RenderData::AddRoundedRect(
	const RoundedRect& rrect, const V4_float& color, float line_width,
	const V2_float& rotation_center, float fade, std::int32_t render_layer
) {
	PTGN_ASSERT(
		2.0f * rrect.radius < rrect.size.x,
		"Cannot draw rounded rectangle with larger radius than half its width"
	);
	PTGN_ASSERT(
		2.0f * rrect.radius < rrect.size.y,
		"Cannot draw rounded rectangle with larger radius than half its height"
	);

	V2_float offset{ GetOffsetFromCenter(rrect.size, rrect.origin) };

	// TODO: Consider adding this as a GetRect() function for RoundedRect.
	Rect inner_rect{ rrect.position - offset, rrect.size - rrect.radius * 2.0f, Origin::Center,
						rrect.rotation };

	bool filled{ line_width == -1.0f };

	float length{ rrect.radius };

	if (filled) {
		length = rrect.radius / 2.0f;
	};

	V2_float t{ V2_float(length, 0.0f).Rotated(rrect.rotation - half_pi<float>) };
	V2_float r{ V2_float(length, 0.0f).Rotated(rrect.rotation + 0.0f) };
	V2_float b{ V2_float(length, 0.0f).Rotated(rrect.rotation + half_pi<float>) };
	V2_float l{ V2_float(length, 0.0f).Rotated(rrect.rotation - pi<float>) };

	auto inner_vertices{ inner_rect.GetVertices(rotation_center) };

	AddArc(
		{ inner_vertices[0], rrect.radius, rrect.rotation - pi<float>,
			rrect.rotation - half_pi<float> },
		false, color, line_width, fade, render_layer
	);
	AddArc(
		{ inner_vertices[1], rrect.radius, rrect.rotation - half_pi<float>,
			rrect.rotation + 0.0f },
		false, color, line_width, fade, render_layer
	);
	AddArc(
		{ inner_vertices[2], rrect.radius, rrect.rotation + 0.0f,
			rrect.rotation + half_pi<float> },
		false, color, line_width, fade, render_layer
	);
	AddArc(
		{ inner_vertices[3], rrect.radius, rrect.rotation + half_pi<float>,
			rrect.rotation + pi<float> },
		false, color, line_width, fade, render_layer
	);

	float line_thickness{ line_width };

	if (filled) {
		AddRect(inner_vertices, color, line_width, render_layer);
		line_thickness = rrect.radius;
	}

	AddLine(
		{ inner_vertices[0] + t, inner_vertices[1] + t }, color, line_thickness, render_layer
	);
	AddLine(
		{ inner_vertices[1] + r, inner_vertices[2] + r }, color, line_thickness, render_layer
	);
	AddLine(
		{ inner_vertices[2] + b, inner_vertices[3] + b }, color, line_thickness, render_layer
	);
	AddLine(
		{ inner_vertices[3] + l, inner_vertices[0] + l }, color, line_thickness, render_layer
	);
}

void RenderData::AddArc(
	const Arc& arc, bool clockwise, const V4_float& color, float line_width, float fade,
	std::int32_t render_layer
) {
	PTGN_ASSERT(arc.radius >= 0.0f, "Cannot draw filled arc with negative radius");

	float start_angle{ ClampAngle2Pi(arc.start_angle) };
	float end_angle{ ClampAngle2Pi(arc.end_angle) };

	// Edge case where arc is a point.
	if (NearlyEqual(arc.radius, 0.0f)) {
		AddPoint(arc.center, color, 1.0f, fade, render_layer);
		return;
	}

	bool filled{ line_width == -1.0f };

	PTGN_ASSERT(filled || line_width > 0.0f, "Cannot draw arc with zero line thickness");

	// Edge case where start and end angles match (considered a full rotation).
	if (float range{ start_angle - end_angle };
		NearlyEqual(range, 0.0f) || NearlyEqual(range, two_pi<float>)) {
		AddEllipse(
			{ arc.center, { arc.radius, arc.radius } }, color, line_width, fade, render_layer
		);
	}

	if (start_angle > end_angle) {
		end_angle += two_pi<float>;
	}

	float arc_angle{ end_angle - start_angle };

	PTGN_ASSERT(arc_angle >= 0.0f);

	// Number of triangles the arc is divided into.
	std::size_t resolution{
		std::max(static_cast<std::size_t>(360), static_cast<std::size_t>(30.0f * arc.radius))
	};

	std::size_t n{ resolution };

	float delta_angle{ arc_angle / static_cast<float>(n) };

	if (n > 1) {
		std::vector<V2_float> v(n);

		for (std::size_t i{ 0 }; i < n; i++) {
			float angle_radians{ start_angle };
			float delta{ static_cast<float>(i) * delta_angle };
			if (clockwise) {
				angle_radians -= delta;
			} else {
				angle_radians += delta;
			}

			v[i] = { arc.center.x + arc.radius * std::cos(angle_radians),
						arc.center.y + arc.radius * std::sin(angle_radians) };
		}

		if (filled) {
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				AddTriangle({ arc.center, v[i], v[i + 1] }, color, line_width, render_layer);
			}
		} else {
			PTGN_ASSERT(
				line_width >= 0.0f, "Must provide valid line width when drawing hollow arc"
			);
			for (std::size_t i{ 0 }; i < v.size() - 1; i++) {
				AddLine({ v[i], v[i + 1] }, color, line_width, render_layer);
			}
		}
	} else {
		AddPoint(arc.center, color, 1.0f, fade, render_layer);
	}
}

void RenderData::AddCapsule(
	const Capsule& capsule, const V4_float& color, float line_width, float fade,
	std::int32_t render_layer
) {
	V2_float dir{ capsule.line.Direction() };
	const float angle_radians{ dir.Angle() + half_pi<float> };
	const float dir2{ dir.Dot(dir) };

	V2_float tangent_r;

	// Note that dir2 is an int.
	if (NearlyEqual(dir2, 0.0f)) {
		AddEllipse(
			{ capsule.line.a, { capsule.radius, capsule.radius } }, color, line_width, fade,
			render_layer
		);
		return;
	} else {
		V2_float tmp{ dir.Skewed() / std::sqrt(dir2) * capsule.radius };
		tangent_r = { Floor(tmp) };
	}

	float start_angle{ angle_radians };
	float end_angle{ angle_radians };

	if (line_width == -1.0f) {
		// Draw central line.
		AddLine(capsule.line, color, capsule.radius * 2.0f, render_layer);

		// How many radians into the line the arc protrudes.
		constexpr float delta{ DegToRad(0.5f) };
		start_angle -= delta;
		end_angle	+= delta;
	} else {
		// Draw edge lines.
		AddLine(
			{ capsule.line.a + tangent_r, capsule.line.b + tangent_r }, color, line_width,
			render_layer
		);
		AddLine(
			{ capsule.line.a - tangent_r, capsule.line.b - tangent_r }, color, line_width,
			render_layer
		);
	}

	// Draw edge arcs.
	AddArc(
		{ capsule.line.a, capsule.radius, start_angle, end_angle + pi<float> }, false, color,
		line_width, fade, render_layer
	);
	AddArc(
		{ capsule.line.b, capsule.radius, start_angle + pi<float>, end_angle }, false, color,
		line_width, fade, render_layer
	);
}

void RenderData::AddPolygon(
	const Polygon& polygon, const V4_float& color, float line_width, std::int32_t render_layer
) {
	if (line_width == -1.0f) {
		auto triangles{ Triangulate(polygon.vertices.data(), polygon.vertices.size()) };

		for (const auto& t : triangles) {
			AddTriangle(t, color, line_width, render_layer);
		}
	} else {
		for (std::size_t i{ 0 }; i < polygon.vertices.size(); i++) {
			AddLine(
				{ polygon.vertices[i], polygon.vertices[(i + 1) % polygon.vertices.size()] },
				color, line_width, render_layer
			);
		}
	}
}

void RenderData::AddPrimitiveQuad(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, const Texture& texture
) {
	AddPrimitive<BatchType::Quad, QuadVertex>(
		positions, render_layer, color, tex_coords, texture, 0.0f, 0.0f
	);
}

void RenderData::AddPrimitiveCircle(
	const std::array<V2_float, 4>& positions, std::int32_t render_layer, const V4_float& color,
	float line_width, float fade
) {
	AddPrimitive<BatchType::Circle, CircleVertex>(
		positions, render_layer, color, {}, {}, line_width, fade
	);
}

void RenderData::AddPrimitiveTriangle(
	const std::array<V2_float, 3>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Triangle, ColorVertex>(positions, render_layer, color);
}

void RenderData::AddPrimitiveLine(
	const std::array<V2_float, 2>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Line, ColorVertex>(positions, render_layer, color);
}

void RenderData::AddPrimitivePoint(
	const std::array<V2_float, 1>& positions, std::int32_t render_layer, const V4_float& color
) {
	AddPrimitive<BatchType::Point, ColorVertex>(positions, render_layer, color);
}

bool RenderData::IsBlank(const Texture& texture) {
	return texture == game.renderer.white_texture_;
}

std::vector<Batch>& RenderData::GetLayerBatches(std::int32_t render_layer, [[maybe_unused]] float alpha) {
	// TODO: Add opaque batches back once you figure out how to do it using depth testing.
	/*
	// Transparent object.
	if (NearlyEqual(alpha, 1.0f)) { // opaque object
		if (opaque_batches_.size() == 0) {
			opaque_batches_.emplace_back(max_texture_slots_);
		}
		return opaque_batches_;
	}
	*/
	// Transparent object.
	if (auto it{ transparent_layers_.find(render_layer) }; it != transparent_layers_.end()) {
		return it->second;
	}
	return transparent_layers_.emplace(render_layer, std::vector<Batch>(1)).first->second;
}

void RenderData::FlushBatches(const std::vector<Batch>& batches) {
	game.renderer.quad_shader_.Bind();

	FlushType<BatchType::Quad>(batches);

	game.renderer.circle_shader_.Bind();

	FlushType<BatchType::Circle>(batches);

	// Triangles, points, and lines all use color shader so only one bind is necessary.
	game.renderer.color_shader_.Bind();

	FlushType<BatchType::Triangle>(batches);
	FlushType<BatchType::Line>(batches);
	FlushType<BatchType::Point>(batches);
}

} // namespace impl

} // namespace ptgn