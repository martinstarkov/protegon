#include "renderer/render_data.h"

#include <array>
#include <cstdint>
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility>
#include <vector>

#include "components/draw.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/blend_mode.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/utility.h"
#include "vfx/light.h"

namespace ptgn::impl {

void RenderData::Init() {
	max_texture_slots = GLRenderer::GetMaxTextureSlots();

	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };

	PTGN_ASSERT(quad_shader.IsValid());
	PTGN_ASSERT(game.shader.Get<ShapeShader::Circle>().IsValid());
	PTGN_ASSERT(game.shader.Get<ScreenShader::Default>().IsValid());
	PTGN_ASSERT(game.shader.Get<OtherShader::Light>().IsValid());

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader.Bind();
	quad_shader.SetUniform(
		"u_Texture", samplers.data(), static_cast<std::int32_t>(samplers.size())
	);

	IndexBuffer quad_ib{ nullptr, Batch::index_batch_capacity,
						 static_cast<std::uint32_t>(sizeof(Batch::IndexType)),
						 BufferUsage::DynamicDraw };
	VertexBuffer quad_vb{ nullptr, Batch::vertex_batch_capacity,
						  static_cast<std::uint32_t>(sizeof(Vertex)), BufferUsage::DynamicDraw };

	triangle_vao = VertexArray(
		PrimitiveMode::Triangles, std::move(quad_vb), quad_vertex_layout, std::move(quad_ib)
	);

	white_texture = Texture(static_cast<const void*>(&color::White), { 1, 1 });

	// TODO: Once window resizing is implemented, get rid of this.
	lights = RenderTarget{ { 1, 1 }, color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { lights.GetTexture().Resize(e.size); })
	);
}

/*
std::pair<std::size_t, std::size_t> RenderData::GetVertexIndexCount(const ecs::Entity& e) {
	if (e.HasAny<TextureKey, Text, RenderTarget, Rect, Line, Circle, Ellipse, Point>()) {
		// Lines are rotated quads.
		// Points are either circles or quads.
		return { Batch::quad_vertex_count, Batch::quad_index_count };
	} else if (e.Has<Polygon>()) {
		const auto& polygon{ e.Get<Polygon>() };
		if (!e.Has<LineWidth>() || (e.Has<LineWidth>() && e.Get<LineWidth>() == -1.0f)) {
			// TODO: Figure out a better way to determine how many solid triangles this polygon
			// will have. I have not looked into the triangulation formula. It may just work
			// like a triangle fan.
			auto triangles{ polygon.Triangulate() };
			return { triangles.size() * Batch::triangle_vertex_count,
					 triangles.size() * Batch::triangle_index_count };
		} else {
			// Hollow polygon.
			// Every line is a rotated quad.
			auto vertices{ polygon.GetVertices().size() };
			return { vertices * Batch::quad_vertex_count, vertices * Batch::quad_index_count };
		}
	} else if (e.Has<Triangle>()) {
		return { Batch::triangle_vertex_count, Batch::triangle_index_count };

	} else if (e.Has<Arc>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Arc drawing not implemented yet");
	} else if (e.Has<RoundedRect>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Rounded rectangle drawing not implemented yet");
	} else if (e.Has<Capsule>()) {
		// TODO: Implement.
		// vertex_count = ?;
		// index_count =  ?;
		PTGN_ERROR("Capsule drawing not implemented yet");
	} else if (e.Has<PointLight>()) {
		return { 0, 0 };
	}
	PTGN_ERROR("Shape not implemented.");
}
*/

Batch& RenderData::GetBatch(
	std::size_t vertex_count, std::size_t index_count, const Texture& texture, const Shader& shader,
	BlendMode blend_mode, const Depth& depth
) {
	PTGN_ASSERT(texture.IsValid());

	auto& batches{ batch_map[depth].vector };

	Batch* b{ nullptr };

	if (batches.empty()) {
		b = &batches.emplace_back(shader, blend_mode);
	} else {
		b = &batches.back();
		if (!b->Uses(shader, blend_mode) ||
			!b->HasRoomForTexture(texture, white_texture, max_texture_slots) ||
			!b->HasRoomForShape(vertex_count, index_count)) {
			b = &batches.emplace_back(shader, blend_mode);
		}
	}
	PTGN_ASSERT(b != nullptr, "Failed to create or find a valid batch to add to");
	return *b;
}

float RenderData::GetTextureIndex(Batch& batch, const Texture& texture) {
	PTGN_ASSERT(texture.IsValid());
	auto texture_index{
		batch.GetTextureIndex(texture.GetId(), white_texture.GetId(), max_texture_slots)
	};
	PTGN_ASSERT(texture_index != -1.0f);
	return texture_index;
}

void RenderData::AddLine(
	const V2_float& line_start, const V2_float& line_end, float line_width, const Depth& depth,
	BlendMode blend_mode, const V4_float& color
) {
	PTGN_ASSERT(line_width >= min_line_width, "-1.0f is an invalid line width for lines");
	auto vertices{ Line::GetQuadVertices(line_start, line_end, line_width) };
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), blend_mode, depth
	) };
	batch.AddFilledQuad(vertices, color, depth);
}

void RenderData::AddLines(
	const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
	BlendMode blend_mode, const V4_float& color, bool connect_last_to_first
) {
	std::size_t vertex_modulo{ vertices.size() };

	if (!connect_last_to_first) {
		PTGN_ASSERT(
			vertices.size() >= 2, "Lines which do not connect the last vertex to the first vertex "
								  "must have at least 2 vertices"
		);
		vertex_modulo -= 1;
	} else {
		PTGN_ASSERT(
			vertices.size() >= 3, "Lines which connect the last vertex to the first vertex "
								  "must have at least 3 vertices"
		);
	}

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		AddLine(
			vertices[i], vertices[(i + 1) % vertex_modulo], line_width, depth, blend_mode, color
		);
	}
}

void RenderData::AddFilledTriangle(
	const std::array<V2_float, Batch::triangle_vertex_count>& vertices, const Depth& depth,
	BlendMode blend_mode, const V4_float& color
) {
	auto& batch{ GetBatch(
		Batch::triangle_vertex_count, Batch::triangle_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), blend_mode, depth
	) };
	batch.AddFilledTriangle(vertices, color, depth);
}

void RenderData::AddFilledQuad(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
	BlendMode blend_mode, const V4_float& color
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), blend_mode, depth
	) };
	batch.AddFilledQuad(vertices, color, depth);
}

void RenderData::AddFilledEllipse(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
	BlendMode blend_mode, const V4_float& color
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Circle>(), blend_mode, depth
	) };
	batch.AddFilledEllipse(vertices, color, depth);
}

void RenderData::AddHollowEllipse(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, float line_width,
	const V2_float& radius, const Depth& depth, BlendMode blend_mode, const V4_float& color
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Circle>(), blend_mode, depth
	) };
	batch.AddHollowEllipse(vertices, line_width, radius, color, depth);
}

void RenderData::AddTexturedQuad(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices,
	const std::array<V2_float, Batch::quad_vertex_count>& tex_coords, const Texture& texture,
	const Depth& depth, BlendMode blend_mode, const V4_float& color
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, texture,
		game.shader.Get<ShapeShader::Quad>(), blend_mode, depth
	) };
	float texture_index{ GetTextureIndex(batch, texture) };
	PTGN_ASSERT(texture_index > 0.0f, "Failed to find a valid texture index");
	batch.AddTexturedQuad(vertices, tex_coords, texture_index, color, depth);
}

void RenderData::AddPointLight(const ecs::Entity& o, const Depth& depth) {
	PTGN_ASSERT(o.Has<PointLight>());

	auto [it, inserted] = batch_map.try_emplace(depth);

	auto& batches{ it->second };

	auto& batch_vector{ batches.vector };

	Batch* b{ nullptr };

	const auto& shader{ game.shader.Get<OtherShader::Light>() };

	if (batch_vector.empty()) {
		b = &batch_vector.emplace_back(shader, light_blend_mode);
	} else {
		b = &batch_vector.back();
		if (!b->Uses(shader, light_blend_mode)) {
			b = &batch_vector.emplace_back(shader, light_blend_mode);
		}
	}
	PTGN_ASSERT(b != nullptr, "Failed to find batch for light");
	b->lights.emplace_back(o);
}

V2_float RenderData::GetTextureSize(const ecs::Entity& o, const Texture& texture) {
	V2_float size;
	if (o.Has<TextureCrop>()) {
		size = o.Get<TextureCrop>().GetSize();
	}
	if (o.Has<DisplaySize>()) {
		size = o.Get<DisplaySize>();
	}
	if (size.IsZero()) {
		size = texture.GetSize();
	}
	size *= GetScale(o);
	return size;
}

void RenderData::AddToBatch(
	const ecs::Entity& o, const std::array<V2_float, Batch::quad_vertex_count>& camera_vertices,
	bool check_visibility
) {
	PTGN_ASSERT(
		(o.Has<Transform, Visible>()), "Cannot render entity without transform or visible component"
	);

	if (check_visibility && !o.Get<Visible>()) {
		return;
	}

	const auto& depth{ GetDepth(o) };
	BlendMode blend_mode{ GetBlendMode(o) };
	V2_float pos{ GetPosition(o) };
	float angle{ GetRotation(o) };
	V2_float rot_center{ GetRotationCenter(o) };
	V4_float tint{ GetTint(o).Normalized() };

	auto add_texture = [&](const Texture& texture, const V2_float& size, Origin origin,
						   bool flip_vertically = false) {
		AddTexturedQuad(
			impl::GetQuadVertices(pos - GetOffsetFromCenter(size, origin), angle, size, rot_center),
			ptgn::GetTextureCoordinates(o, flip_vertically), texture, depth, blend_mode, tint
		);
	};

	if (o.Has<TextureKey>()) {
		const auto& texture_key{ o.Get<TextureKey>() };
		PTGN_ASSERT(game.texture.Has(texture_key), "Texture key is not loaded");
		const auto& texture{ game.texture.Get(texture_key) };
		std::invoke(add_texture, texture, GetTextureSize(o, texture), GetOrigin(o));
		return;
	} else if (o.Has<Text>()) {
		const auto& text{ o.Get<Text>() };
		const auto& texture{ text.GetTexture() };
		if (texture.IsValid() && text.GetColor().a != 0 && !text.GetContent().empty()) {
			std::invoke(add_texture, texture, GetTextureSize(o, texture), GetOrigin(o));
		}
		return;
	} else if (o.Has<RenderTarget>()) {
		const auto& rt{ o.Get<RenderTarget>() };
		const auto& texture{ rt.GetTexture() };
		// TODO: Add custom size.
		AddTexturedQuad(
			camera_vertices, ptgn::GetTextureCoordinates(o, true), texture, depth, blend_mode, tint
		);
		return;
	} else if (o.Has<PointLight>()) {
		AddPointLight(o, depth);
		return;
	}

	auto line_width{ o.Has<LineWidth>() ? o.Get<LineWidth>() : LineWidth{ -1.0f } };

	auto add_ellipse = [&](auto radius) {
		V2_float diameter{ radius * 2.0f };
		auto vertices{ impl::GetQuadVertices(pos, angle, diameter, rot_center) };
		if (line_width == -1.0f) {
			AddFilledEllipse(vertices, depth, blend_mode, tint);
		} else {
			AddHollowEllipse(vertices, line_width, V2_float{ radius }, depth, blend_mode, tint);
		}
	};

	if (o.Has<Rect>()) {
		const auto& rect{ o.Get<Rect>() };
		std::array<V2_float, Batch::quad_vertex_count> vertices;
		auto size{ rect.GetSize() };
		if (size.IsZero()) {
			vertices = camera_vertices;
		} else {
			vertices = impl::GetQuadVertices(
				pos - GetOffsetFromCenter(size, rect.GetOrigin()), angle, size, rot_center
			);
		}
		if (line_width == -1.0f) {
			AddFilledQuad(vertices, depth, blend_mode, tint);
		} else {
			AddLines(ToVector(vertices), line_width, depth, blend_mode, tint, true);
		}
		return;
	} else if (o.Has<Polygon>()) {
		const auto& polygon{ o.Get<Polygon>() };
		if (line_width == -1.0f) {
			auto triangles{ polygon.Triangulate() };
			for (const auto& triangle : triangles) {
				AddFilledTriangle(triangle, depth, blend_mode, tint);
			}
		} else {
			AddLines(polygon.GetVertices(), line_width, depth, blend_mode, tint, true);
		}
		return;
	} else if (o.Has<Line>()) {
		const auto& line{ o.Get<Line>() };
		AddLine(line.GetStart(), line.GetEnd(), line_width, depth, blend_mode, tint);
		return;
	} else if (o.Has<Circle>()) {
		const auto& circle{ o.Get<Circle>() };
		auto radius{ circle.GetRadius() };
		PTGN_ASSERT(radius > 0.0f, "Invalid circle radius");
		std::invoke(add_ellipse, radius);
		return;
	} else if (o.Has<Ellipse>()) {
		const auto& ellipse{ o.Get<Ellipse>() };
		auto radius{ ellipse.GetRadius() };
		PTGN_ASSERT(radius.x > 0.0f && radius.y > 0.0f, "Invalid ellipse radius");
		std::invoke(add_ellipse, radius);
		return;
	} else if (o.Has<Triangle>()) {
		const auto& triangle{ o.Get<Triangle>() };
		auto vertices{ triangle.GetVertices() };
		if (line_width == -1.0f) {
			AddFilledTriangle(vertices, depth, blend_mode, tint);
		} else {
			AddLines(ToVector(vertices), line_width, depth, blend_mode, tint, true);
		}
		return;
	} else if (o.Has<Point>()) {
		PTGN_ASSERT(!o.Has<LineWidth>(), "Points cannot have a line width");
		PTGN_ASSERT(line_width == -1.0f);
		// TODO: Check that this works.
		AddFilledQuad({ pos, pos, pos, pos }, depth, blend_mode, tint);
		return;
	} else if (o.Has<Arc>()) {
		// TODO: Implement.
		PTGN_ERROR("Arc drawing not implemented yet");
	} /*else if (o.Has<RoundedRect>()) {
		// TODO: Implement.
		PTGN_ERROR("Rounded rectangle drawing not implemented yet");
	}*/
	else if (o.Has<Capsule>()) {
		// TODO: Implement.
		PTGN_ERROR("Capsule drawing not implemented yet");
	} else {
		PTGN_ERROR("Unknown drawable");
	}
}

void RenderData::SetupRender(const FrameBuffer& frame_buffer, const Camera& camera) const {
	frame_buffer.Bind();
	auto position{ camera.GetViewportPosition() };
	auto size{ camera.GetViewportSize() };
	GLRenderer::SetViewport(position, position + size);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& camera, ecs::Manager& manager
) {
	SetupRender(frame_buffer, camera);
	auto camera_vertices{ camera.GetQuadVertices() };
	for (auto [e, t, v] : manager.EntitiesWith<Transform, Visible>()) {
		AddToBatch(e, camera_vertices, true);
	}
	FlushBatches(frame_buffer, camera);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& camera, const ecs::Entity& o,
	bool check_visibility
) {
	SetupRender(frame_buffer, camera);
	auto camera_vertices{ camera.GetQuadVertices() };
	AddToBatch(o, camera_vertices, check_visibility);
	FlushBatches(frame_buffer, camera);
}

void RenderData::RenderToScreen(const RenderTarget& target, const Camera& camera) {
	FrameBuffer::Unbind();
	GLRenderer::SetBlendMode(BlendMode::Blend);
	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };
	quad_shader.Bind();
	quad_shader.SetUniform("u_ViewProjection", camera);
	quad_shader.SetUniform("u_Texture", 1);
	quad_shader.SetUniform("u_Resolution", V2_float{ game.window.GetSize() });
	target.GetFrameBuffer().GetTexture().Bind(1);
	SetVertexArrayToWindow(camera, target.GetTint(), Depth{ 0 }, 1.0f);
	GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
}

void RenderData::SetVertexArrayToWindow(
	const Camera& camera, const Color& color, const Depth& depth, float texture_index
) {
	std::array<Batch::IndexType, Batch::quad_index_count> indices{ 0, 1, 2, 2, 3, 0 };
	auto positions{ camera.GetQuadVertices() };
	auto tex_coords{ GetDefaultTextureCoordinates() };
	FlipTextureCoordinates(tex_coords, Flip::Vertical);
	auto c{ color.Normalized() };
	std::array<Vertex, Batch::quad_vertex_count> vertices{};
	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		vertices[i].position  = { positions[i].x, positions[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices[i].tex_index = { texture_index };
	}
	triangle_vao.Bind();
	triangle_vao.GetVertexBuffer().SetSubData(
		vertices.data(), 0, static_cast<std::uint32_t>(vertices.size()), sizeof(Vertex), false
	);
	triangle_vao.GetIndexBuffer().SetSubData(
		indices.data(), 0, static_cast<std::uint32_t>(indices.size()), sizeof(Batch::IndexType),
		false
	);
}

void RenderData::FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera) {
	white_texture.Bind();
	V2_float window_size{ game.window.GetSize() };
	// Assume depth map sorted.
	for (auto& [depth, batches] : batch_map) {
		for (auto& batch : batches.vector) {
			// TODO: Move this somewhere else and simplify.
			if (!batch.lights.empty()) {
				PTGN_ASSERT(batch.texture_ids.size() == 0);
				bool lights_found{ false };
				for (const auto& e : batch.lights) {
					if (!e.Has<Visible>() || !e.Get<Visible>()) {
						continue;
					}
					PTGN_ASSERT((e.Has<PointLight, Transform>()));
					if (!lights_found) {
						lights.Bind();
						lights.Clear();
						GLRenderer::SetBlendMode(BlendMode::Blend);
						batch.shader.Bind();
						batch.shader.SetUniform("u_Texture", 1);
						batch.shader.SetUniform("u_ViewProjection", camera);
						batch.shader.SetUniform("u_Resolution", window_size);
						lights.GetTexture().Bind(1);
						SetVertexArrayToWindow(camera, color::White, depth, 1.0f);
						lights_found = true;
					}
					const auto& light{ e.Get<PointLight>() };
					const auto& transform{ e.Get<Transform>() };
					batch.shader.SetUniform("u_LightPosition", transform.position);
					batch.shader.SetUniform("u_LightIntensity", light.GetIntensity());
					batch.shader.SetUniform("u_LightRadius", light.GetRadius());
					batch.shader.SetUniform("u_Falloff", light.GetFalloff());
					batch.shader.SetUniform("u_Color", light.GetColor().Normalized());
					auto ac{ PointLight::GetShaderColor(light.GetAmbientColor()) };
					batch.shader.SetUniform("u_AmbientColor", ac);
					batch.shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());
					GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
				}
				if (!lights_found) {
					continue;
				}
				frame_buffer.Bind();
				GLRenderer::SetBlendMode(batch.blend_mode);
				const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };
				quad_shader.Bind();
				quad_shader.SetUniform("u_ViewProjection", camera);
				lights.GetTexture().Bind(1);
				GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
				continue;
			}
			// TODO: Change to an assert once all shapes have been implemented.
			if (batch.vertices.empty() || batch.indices.empty()) {
				PTGN_WARN("Attempting to draw a shape with no vertices or indices in the batch");
				continue;
			}
			frame_buffer.Bind();
			GLRenderer::SetBlendMode(batch.blend_mode);
			batch.shader.Bind();
			batch.shader.SetUniform("u_ViewProjection", camera);
			batch.BindTextures();
			triangle_vao.Bind();
			triangle_vao.GetVertexBuffer().SetSubData(
				batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()),
				sizeof(Vertex), false
			);
			triangle_vao.GetIndexBuffer().SetSubData(
				batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()),
				sizeof(Batch::IndexType), false
			);
			GLRenderer::DrawElements(triangle_vao, batch.indices.size(), false);
		}
	}

	batch_map.clear();
}

} // namespace ptgn::impl