#include "rendering/batching/render_data.h"

#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"
#include "rendering/batching/batch.h"
#include "rendering/buffers/buffer.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/gl/gl_types.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
#include "utility/span.h"

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

#ifdef PTGN_PLATFORM_MACOS
	// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X GLD_TEXTURE_INDEX_2D
	// is unloadable and bound to sampler type (Float) - using zero texture because texture
	// unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		Texture::Bind(white_texture.GetId(), slot);
	}
#endif

	light_target = CreateRenderTarget(light_manager, { 1, 1 }, color::Transparent);

	// TODO: Once window resizing is implemented, get rid of this.
	game.event.window.Subscribe(
		WindowEvent::Resized, this, std::function([&](const WindowResizedEvent& e) {
			light_target.GetTexture().Resize(e.size);
		})
	);
}

/*
std::pair<std::size_t, std::size_t> RenderData::GetVertexIndexCount(const Entity& e) {
	if (e.HasAny<TextureHandle, Text, RenderTarget, Rect, Line, Circle, Ellipse, Point>()) {
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
	const Camera& camera, BlendMode blend_mode, const Depth& depth, bool debug
) {
	PTGN_ASSERT(texture.IsValid());

	Batches* batches{ nullptr };

	if (debug) {
		batches = &debug_batches;
	} else {
		batches = &batch_map[depth];
	}

	PTGN_ASSERT(batches != nullptr);

	auto& batch_vector{ batches->vector };

	Batch* b{ nullptr };

	if (batch_vector.empty()) {
		// Add new batch.
		b = &batch_vector.emplace_back(shader, camera, blend_mode);
	} else {
		b = &batch_vector.back();
		// Check if the back batch is suitable for use.
		// It is unsuitable if any of the following prerequisites fail:
		if (!b->Uses(shader, camera, blend_mode) ||
			!b->HasRoomForTexture(texture, white_texture, max_texture_slots) ||
			!b->HasRoomForShape(vertex_count, index_count)) {
			// Add new batch.
			b = &batch_vector.emplace_back(shader, camera, blend_mode);
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
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	PTGN_ASSERT(line_width >= min_line_width, "-1.0f is an invalid line width for lines");
	auto vertices{ GetQuadVertices(line_start, line_end, line_width) };
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	batch.AddFilledQuad(vertices, color, depth, pixel_rounding);
}

void RenderData::AddLines(
	const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool connect_last_to_first, bool debug
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
			vertices[i], vertices[(i + 1) % vertex_modulo], line_width, depth, camera, blend_mode, color,
			debug
		);
	}
}

void RenderData::AddFilledTriangle(
	const std::array<V2_float, Batch::triangle_vertex_count>& vertices, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	auto& batch{ GetBatch(
		Batch::triangle_vertex_count, Batch::triangle_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	batch.AddFilledTriangle(vertices, color, depth, pixel_rounding);
}

void RenderData::AddFilledQuad(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	batch.AddFilledQuad(vertices, color, depth, pixel_rounding);
}

void RenderData::AddFilledEllipse(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Circle>(), camera, blend_mode, depth, debug
	) };
	batch.AddFilledEllipse(vertices, color, depth, pixel_rounding);
}

void RenderData::AddHollowEllipse(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices, float line_width,
	const V2_float& radius, const Depth& depth, const Camera& camera, BlendMode blend_mode,
	const V4_float& color,
	bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Circle>(), camera, blend_mode, depth, debug
	) };
	batch.AddHollowEllipse(vertices, line_width, radius, color, depth, pixel_rounding);
}

void RenderData::AddTexturedQuad(
	const std::array<V2_float, Batch::quad_vertex_count>& vertices,
	const std::array<V2_float, Batch::quad_vertex_count>& tex_coords, const Texture& texture,
	const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	float texture_index{ GetTextureIndex(batch, texture) };
	PTGN_ASSERT(texture_index > 0.0f, "Failed to find a valid texture index");
	batch.AddTexturedQuad(vertices, tex_coords, texture_index, color, depth, pixel_rounding);
}

void RenderData::AddEllipse(
	const V2_float& center, const V2_float& radius, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, float rotation, bool debug
) {
	PTGN_ASSERT(radius.x > 0.0f && radius.y > 0.0f, "Invalid ellipse radius");
	V2_float diameter{ radius * 2.0f };
	auto vertices{ impl::GetVertices({ center, rotation }, diameter, Origin::Center) };
	if (line_width == -1.0f) {
		AddFilledEllipse(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddHollowEllipse(vertices, line_width, V2_float{ radius }, depth, camera, blend_mode, color, debug);
	}
}

void RenderData::AddPolygon(
	const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	PTGN_ASSERT(vertices.size() >= 3, "Polygon must have at least 3 vertices");
	if (line_width == -1.0f) {
		auto triangles{ Triangulate(vertices.data(), vertices.size()) };
		for (const auto& triangle : triangles) {
			AddFilledTriangle(triangle, depth, camera, blend_mode, color, debug);
		}
	} else {
		AddLines(vertices, line_width, depth, camera, blend_mode, color, true, debug);
	}
}

void RenderData::AddPoint(
	const V2_float& position, const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color,
	bool debug
) {
	constexpr V2_float half{ 0.5f };
	AddFilledQuad(
		{ position - half, position + V2_float{ half.x, -half.y }, position + half,
		  position + V2_float{ -half.x, half.y } },
		depth, camera, blend_mode, color, debug
	);
}

void RenderData::AddTriangle(
	const std::array<V2_float, 3>& vertices, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	if (line_width == -1.0f) {
		AddFilledTriangle(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddLines(ToVector(vertices), line_width, depth, camera, blend_mode, color, true, debug);
	}
}

void RenderData::AddQuad(
	const V2_float& position, const V2_float& size, Origin origin, float line_width,
	const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color, float rotation, bool debug
) {
	std::array<V2_float, Batch::quad_vertex_count> vertices;
	if (size.IsZero()) {
		vertices = camera_vertices;
	} else {
		vertices = impl::GetVertices({ position, rotation }, size, origin);
	}
	if (line_width == -1.0f) {
		AddFilledQuad(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddLines(ToVector(vertices), line_width, depth, camera, blend_mode, color, true, debug);
	}
}

void RenderData::AddToBatch(const Entity& entity, bool check_visibility) {
	if (check_visibility) {
		PTGN_ASSERT(entity.Has<Visible>(), "Cannot render entity without visible component");
		if (!entity.Get<Visible>()) {
			return;
		}
	}

	PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.Get<IDrawable>() };

	const auto& drawable_functions{ IDrawable::data() };

	const auto it{ drawable_functions.find(drawable.hash) };

	PTGN_ASSERT(it != drawable_functions.end(), "Failed to identify drawable hash");

	const auto& draw_function{ it->second };

	std::invoke(draw_function, *this, entity);
}

void RenderData::UseCamera(const Camera& camera) {
	if (camera != Camera{} && camera == active_camera) {
		return;
	}
	
	active_camera = camera ? camera : fallback_camera; 

	PTGN_ASSERT(active_camera, "Cannot render to invalid camera");
	PTGN_ASSERT(active_camera.Has<impl::CameraInfo>(), "Active render camera must have camera info component");

	pixel_rounding	= active_camera.IsPixelRoundingEnabled();
	camera_vertices = active_camera.GetVertices();

	auto position{ active_camera.GetViewportPosition() };
	auto size{ active_camera.GetViewportSize() };

	GLRenderer::SetViewport(position, position + size);
}

bool RenderData::FlushLights(
	Batch& batch, const FrameBuffer& frame_buffer,
	const V2_float& window_size, const Depth& depth
) {
	if (batch.lights.empty()) {
		return false;
	}

	PTGN_ASSERT(batch.texture_ids.empty());

	bool lights_found{ false };

	for (const auto& e : batch.lights) {
		if (!e.IsVisible()) {
			continue;
		}

		auto light_camera{ e.GetOrDefault<Camera>() };

		UseCamera(light_camera);
		
		if (!lights_found) {
			light_target.Bind();
			light_target.Clear();

			GLRenderer::SetBlendMode(BlendMode::Blend);

			batch.shader.Bind();
			batch.shader.SetUniform("u_Texture", 1);
			batch.shader.SetUniform("u_ViewProjection", active_camera);
			batch.shader.SetUniform("u_Resolution", window_size);

			light_target.GetTexture().Bind(1);

			SetVertexArrayToWindow(color::White, depth, 1.0f);

			lights_found = true;
		}

		PointLight light{ e };

		auto offset_transform{ GetOffset(e) };
		auto transform{ e.GetAbsoluteTransform() };
		transform = transform.RelativeTo(offset_transform);
		float radius{ light.GetRadius() * Abs(transform.scale.x) };

		batch.shader.SetUniform("u_LightPosition", transform.position);
		batch.shader.SetUniform("u_LightIntensity", light.GetIntensity());
		batch.shader.SetUniform("u_LightRadius", radius);
		batch.shader.SetUniform("u_Falloff", light.GetFalloff());
		batch.shader.SetUniform("u_Color", light.GetColor().Normalized());
		auto ambient_color{ PointLight::GetShaderColor(light.GetAmbientColor()) };
		batch.shader.SetUniform("u_AmbientColor", ambient_color);
		batch.shader.SetUniform("u_AmbientIntensity", light.GetAmbientIntensity());

		GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
	}

	if (!lights_found) {
		return false;
	}

	frame_buffer.Bind();

	GLRenderer::SetBlendMode(batch.blend_mode);

	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };

	quad_shader.Bind();

	UseCamera(light_target.GetCamera());

	quad_shader.SetUniform("u_ViewProjection", active_camera);

	light_target.GetTexture().Bind(1);

	GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);

	return true;
}

void RenderData::SortEntitiesByY(std::vector<Entity>&) {
	// TODO: Investigate making this faster for large numbers of static entities.
	/*std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		return a.GetLowestY() < b.GetLowestY();
	});*/
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Manager& manager
) {
	frame_buffer.Bind();

	fallback_camera = game.scene.GetCurrent().camera.primary;
	UseCamera(Camera{});

	// auto entities{ .GetVector() };

	/*if (sort_entity_drawing_by_y) {
		SortEntitiesByY(entities);
	}*/

	for (auto [e, v, d] : manager.EntitiesWith<Visible, IDrawable>()) {
		AddToBatch(e, true);
	}

	Flush(frame_buffer);

	FlushDebugBatches(frame_buffer);
}

void RenderData::Render(
	const FrameBuffer& frame_buffer, const Camera& target_camera, const Entity& entity, bool check_visibility
) {
	frame_buffer.Bind();

	fallback_camera = target_camera;
	
	UseCamera(target_camera);

	AddToBatch(entity, check_visibility);

	Flush(frame_buffer);
}

/*
void RenderData::RenderToScreen(const RenderTarget& target, const Camera& camera) {
	FrameBuffer::Unbind();

	GLRenderer::SetBlendMode(BlendMode::Blend);

	const auto& quad_shader{ game.shader.Get<ShapeShader::Quad>() };

	quad_shader.Bind();
	
	UseCamera(camera);

	quad_shader.SetUniform("u_ViewProjection", active_camera);
	quad_shader.SetUniform("u_Texture", 1);
	quad_shader.SetUniform("u_Resolution", V2_float{ game.window.GetSize() });

	target.GetFrameBuffer().GetTexture().Bind(1);

	SetVertexArrayToWindow(target.GetTint(), Depth{ 0 }, 1.0f);

	GLRenderer::DrawElements(triangle_vao, Batch::quad_index_count, false);
}
*/

void RenderData::SetVertexArrayToWindow(const Color& color, const Depth& depth, float texture_index
) {
	std::array<Batch::IndexType, Batch::quad_index_count> indices{ 0, 1, 2, 2, 3, 0 };

	const auto& positions{ camera_vertices };
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

void RenderData::FlushBatches(
	Batches& batches, const FrameBuffer& frame_buffer,
	const V2_float& window_size, const Depth& depth
) {
	for (auto& batch : batches.vector) {
		bool light_only_batch{ FlushLights(batch, frame_buffer, window_size, depth) };
		if (light_only_batch) {
			continue;
		}
		FlushBatch(batch, frame_buffer);
	}
}

void RenderData::FlushBatch(Batch& batch, const FrameBuffer& frame_buffer) {
	PTGN_ASSERT(
		!batch.vertices.empty() && !batch.indices.empty(),
		"Attempting to draw a shape with no vertices or indices in the batch"
	);

	frame_buffer.Bind();

	GLRenderer::SetBlendMode(batch.blend_mode);

	batch.shader.Bind();

	UseCamera(batch.camera);

	batch.shader.SetUniform("u_ViewProjection", active_camera);
	batch.BindTextures();

	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()), sizeof(Vertex),
		false
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()),
		sizeof(Batch::IndexType), false
	);

	GLRenderer::DrawElements(triangle_vao, batch.indices.size(), false);
}

void RenderData::FlushDebugBatches(const FrameBuffer& frame_buffer) {
	FlushBatches(
		debug_batches, frame_buffer, game.window.GetSize(),
		std::numeric_limits<std::int32_t>::max()
	);
	debug_batches = {};
}

void RenderData::Flush(const FrameBuffer& frame_buffer) {
	white_texture.Bind();
	V2_float window_size{ game.window.GetSize() };
	// Assume depth map sorted.
	for (auto& [depth, batches] : batch_map) {
		FlushBatches(batches, frame_buffer, window_size, depth);
	}

	/*if (batch_map.empty()) {
		PTGN_WARN("Renderer empty screen");
	}*/

	batch_map.clear();
}

} // namespace ptgn::impl