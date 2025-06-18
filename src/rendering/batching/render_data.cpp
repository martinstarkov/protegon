#include "rendering/batching/render_data.h"

#include <array>
#include <cstdint>
#include <functional>
#include <list>
#include <numeric>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/vertex.h"
#include "rendering/buffers/buffer.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/gl/gl_types.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/shader.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn::impl {

std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	float texture_index
) {
	std::array<Vertex, 4> vertices{};

	V4_float c{ color.Normalized() };

	PTGN_ASSERT(vertices.size() == default_texture_coordinates.size());

	for (std::size_t i{ 0 }; i < vertices.size(); ++i) {
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { default_texture_coordinates[i].x,
								  default_texture_coordinates[i].y };
		vertices[i].tex_index = { texture_index };
	}

	return vertices;
}

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

	IndexBuffer quad_ib{ nullptr, index_capacity, static_cast<std::uint32_t>(sizeof(Index)),
						 BufferUsage::DynamicDraw };
	VertexBuffer quad_vb{ nullptr, vertex_capacity, static_cast<std::uint32_t>(sizeof(Vertex)),
						  BufferUsage::DynamicDraw };

	triangle_vao = VertexArray(
		PrimitiveMode::Triangles, std::move(quad_vb), quad_vertex_layout, std::move(quad_ib)
	);

	white_texture = Texture(static_cast<const void*>(&color::White), { 1, 1 });

	screen_fbo = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Black
	);
	fbo1 = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Black
	);
	fbo2 = impl::CreateRenderTarget(
		render_manager.CreateEntity(), impl::CreateCamera(render_manager.CreateEntity()), { 1, 1 },
		color::Black
	);
	current_fbo = {};

	// TODO: Once render target window resizing is implemented, get rid of this.
	game.event.window.Subscribe(
		WindowEvent::Resized, this, std::function([&](const WindowResizedEvent& e) {
			screen_fbo.GetTexture().Resize(e.size);
			fbo1.GetTexture().Resize(e.size);
			fbo2.GetTexture().Resize(e.size);
		})
	);

#ifdef PTGN_PLATFORM_MACOS
	// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	// texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		Texture::Bind(white_texture.GetId(), slot);
	}
#endif

	SetState(RenderState{ { &game.shader.Get<ShapeShader::Quad>() }, BlendMode::Blend, {} });
}

void RenderData::AddTriangle(const std::array<Vertex, 3>& vertices, const RenderState& state) {
	SetState(state);
	// TODO: Get rid of magic numbers.
	if (batch.vertices.size() + 3 > vertex_capacity || batch.indices.size() + 3 > index_capacity) {
		Flush();
	}

	batch.vertices.reserve(batch.vertices.size() + 3);

	batch.vertices.insert(batch.vertices.end(), vertices.begin(), vertices.end());

	batch.indices.reserve(batch.indices.size() + 3);

	batch.indices.push_back(batch.index_offset + 0);
	batch.indices.push_back(batch.index_offset + 1);
	batch.indices.push_back(batch.index_offset + 2);

	batch.index_offset += 3;
}

float RenderData::GetTextureIndex(std::uint32_t texture_id) {
	PTGN_ASSERT(texture_id != white_texture.GetId());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < batch.textures.size(); i++) {
		if (batch.textures[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			return static_cast<float>(i + 1);
		}
	}
	// Batch is at texture capacity.
	if (static_cast<std::uint32_t>(batch.textures.size()) == max_texture_slots - 1) {
		Flush();
	}
	// Texture does not exist in batch but can be added.
	batch.textures.emplace_back(texture_id);
	// i + 1 is implicit here because size is taken after emplacing.
	return static_cast<float>(batch.textures.size());
}

void RenderData::AddTexturedQuad(
	std::array<Vertex, 4>& vertices, const RenderState& state, TextureId texture_id
) {
	float texture_index{ GetTextureIndex(texture_id) };
	for (auto& v : vertices) {
		v.tex_index = { texture_index };
	}
	AddQuad(vertices, state);
}

void RenderData::AddQuad(const std::array<Vertex, 4>& vertices, const RenderState& state) {
	SetState(state);
	// TODO: Get rid of magic numbers.
	if (batch.vertices.size() + 4 > vertex_capacity || batch.indices.size() + 6 > index_capacity) {
		Flush();
	}

	batch.vertices.reserve(batch.vertices.size() + 4);

	batch.vertices.insert(batch.vertices.end(), vertices.begin(), vertices.end());

	batch.indices.reserve(batch.indices.size() + 6);

	batch.indices.push_back(batch.index_offset + 0);
	batch.indices.push_back(batch.index_offset + 1);
	batch.indices.push_back(batch.index_offset + 2);
	batch.indices.push_back(batch.index_offset + 2);
	batch.indices.push_back(batch.index_offset + 3);
	batch.indices.push_back(batch.index_offset + 0);

	batch.index_offset += 4;
}

void RenderData::SetState(const RenderState& new_render_state) {
	if (new_render_state == render_state) {
		return;
	}
	Flush();
	render_state = new_render_state;
}

void RenderData::SetCameraVertices(const Camera& camera) {
	std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	const auto& positions{ camera.GetVertices() };
	auto tex_coords{ GetDefaultTextureCoordinates() };
	FlipTextureCoordinates(tex_coords, Flip::Vertical);

	auto c{ color::White.Normalized() };

	std::array<Vertex, 4> vertices{};

	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		vertices[i].position  = { positions[i].x, positions[i].y,
								  static_cast<float>(camera.GetDepth()) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices[i].tex_index = { 1.0f };
	}
	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		vertices.data(), 0, static_cast<std::uint32_t>(vertices.size()), sizeof(Vertex), false
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		indices.data(), 0, static_cast<std::uint32_t>(indices.size()), sizeof(Index), false
	);

	/*batch.vertices.clear();
	auto camera_vertices{ GetQuadVertices(camera.GetVertices(), color::White, {}, 1.0f) };
	batch.vertices.insert(batch.vertices.begin(), camera_vertices.begin(), camera_vertices.end());
	PTGN_ASSERT(batch.vertices.size() == 4);
	batch.indices	   = { 0, 1, 2, 2, 3, 0 };
	batch.index_offset = 4;

	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()), sizeof(Vertex),
		false
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()), sizeof(Index),
		false
	);*/
}

void RenderData::AddShader(
	const RenderState& state, const std::vector<std::function<void(const Shader&)>>& uniforms,
	BlendMode fbo_blendmode, bool ping_pong
) {
	PTGN_ASSERT(uniforms.size() == state.shader_.size());

	auto draw_shaders = [&](const Camera& camera) {
		GLRenderer::SetBlendMode(render_state.blend_mode_);
		for (auto i = 0; i < state.shader_.size(); ++i) {
			auto shader = state.shader_[i];
			PTGN_ASSERT(shader != nullptr);
			shader->Bind();
			if (uniforms[i]) {
				uniforms[i](*shader);
			}
			// TODO: Only update these if shader bind is dirty.
			shader->SetUniform("u_ViewProjection", camera);
			shader->SetUniform("u_Resolution", V2_float{ game.window.GetSize() });
			PTGN_ASSERT(shader != &game.shader.Get<ShapeShader::Quad>());
			shader->SetUniform("u_Texture", 1);
			// SetCameraVertices(camera);

			GLRenderer::DrawElements(triangle_vao, 6, false);
		}
	};

	auto draw_to_rt = [&](const RenderTarget& rt) {
		rt.Clear();
		current_fbo	   = rt;
		fbo_blend_mode = fbo_blendmode;
		auto camera{ render_state.camera_ ? render_state.camera_ : rt.GetCamera() };
		PTGN_ASSERT(camera);
		SetCameraVertices(camera);
		PTGN_ASSERT(screen_fbo);
		screen_fbo.GetTexture().Bind(1);
		draw_shaders(camera);
		Flush();
	};

	auto get_fbo = [&]() {
		PTGN_ASSERT(fbo1 && fbo2);
		if (current_fbo == fbo1) {
			return fbo2;
		}
		return fbo1;
	};

	if (state == render_state) {
		if (ping_pong) {
			RenderTarget fbo{ get_fbo() };
			draw_to_rt(fbo);
		} else {
			PTGN_ASSERT(current_fbo);
			draw_shaders(render_state.camera_ ? render_state.camera_ : current_fbo.GetCamera());
		}
	} else {
		// Flush will reset current_fbo so fbo needs to be retrieved before flushing.
		RenderTarget fbo{ get_fbo() };
		Flush();
		render_state = state;
		draw_to_rt(fbo);
	}
}

void RenderData::Flush() {
	if (!game.scene.HasCurrent()) {
		return;
	}

	if (current_fbo) {
		// PTGN_LOG(current_fbo.GetFrameBuffer().GetPixel({ 500, 500 }));

		screen_fbo.Bind();

		auto camera{
			game.scene.GetCurrent().camera.window
		}; // Scene camera or render target camera.

		PTGN_ASSERT(camera);

		const auto& camera_vp{ camera.GetViewProjection() };

		GLRenderer::SetViewport(camera.GetViewportPosition(), camera.GetViewportSize());
		/*
		// TODO: Add postfx to current_fbo
		if (postFX) {
			ApplyPostFX();
		}
		*/
		const auto& shader{ game.shader.Get<ShapeShader::Quad>() };
		shader.Bind();
		shader.SetUniform("u_ViewProjection", camera_vp);
		/*PTGN_ASSERT(batch.vertices.size() == 0);
		PTGN_ASSERT(batch.indices.size() == 0);*/
		// assert that vertices is screen vertices.
		GLRenderer::SetBlendMode(fbo_blend_mode);
		current_fbo.GetTexture().Bind(1);
		SetCameraVertices(camera);
		GLRenderer::DrawElements(triangle_vao, 6, false);

	} else if (!batch.vertices.empty() && !batch.indices.empty()) {
		screen_fbo.Bind();

		Camera fallback_camera{ game.scene.GetCurrent().camera.primary };

		PTGN_ASSERT(fallback_camera);

		PTGN_ASSERT(!render_state.shader_.empty());

		auto chosen_camera{ render_state.camera_
								? render_state.camera_
								: fallback_camera }; // Scene camera or render target camera.

		PTGN_ASSERT(chosen_camera);

		const auto& camera_vp{ chosen_camera.GetViewProjection() };

		GLRenderer::SetViewport(
			chosen_camera.GetViewportPosition(), chosen_camera.GetViewportSize()
		);

		triangle_vao.Bind();

		triangle_vao.GetVertexBuffer().SetSubData(
			batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()),
			sizeof(Vertex), false
		);

		triangle_vao.GetIndexBuffer().SetSubData(
			batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()),
			sizeof(Index), false
		);

		GLRenderer::SetBlendMode(render_state.blend_mode_);

		PTGN_ASSERT(batch.textures.size() <= max_texture_slots);

		for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(batch.textures.size()); i++) {
			// Save first texture slot for empty white texture.
			std::uint32_t slot{ i + 1 };
			Texture::Bind(batch.textures[i], slot);
		}

		for (auto shader : render_state.shader_) {
			shader->Bind();
			// TODO: Only set uniform if camera changed.
			shader->SetUniform("u_ViewProjection", camera_vp);
			// render_state.shader_->SetUniform("u_Resolution", V2_float{ game.window.GetSize() });

			GLRenderer::DrawElements(triangle_vao, batch.indices.size(), false);
		}

		current_fbo = {};
	}

	batch.vertices.clear();
	batch.indices.clear();
	batch.textures.clear();
	batch.index_offset = 0;
}

void RenderData::Draw(Scene& scene) {
	screen_fbo.Clear();
	fbo1.Clear();
	fbo2.Clear();
	// TODO: Clear all render targets.

	white_texture.Bind(0);

	std::vector<Entity> regular_entities;
	regular_entities.reserve(scene.Size());

	// TODO: Fix render target entities.

	// std::vector<Entity> rt_entities;

	/*for (auto [e, rt] : scene.EntitiesWith<Visible, IDrawable, RenderTarget>()) {
		rt_entities.emplace_back(e);
	}*/

	for (auto [entity, visible, drawable] : scene.EntitiesWith<Visible, IDrawable>()) {
		if (!visible || entity.Has<RenderTarget>()) {
			continue;
		}
		if (entity.Has<QuadVertices>()) {
			// TODO: Update dirty vertices here?
			regular_entities.emplace_back(entity);
		} else {
			// TODO: Add general vertices.
			regular_entities.emplace_back(entity);
		}
	}

	// SortEntities<true>(rt_entities);
	SortEntities<false>(regular_entities);

	// for (auto e : rt_entities) {
	//	auto& rt = e.Get<RenderTarget>();
	//	// rt.Draw(e);
	// }

	for (const auto& entity : regular_entities) {
		PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

		const auto& drawable{ entity.Get<IDrawable>() };

		const auto& drawable_functions{ IDrawable::data() };

		const auto it{ drawable_functions.find(drawable.hash) };

		PTGN_ASSERT(it != drawable_functions.end(), "Failed to identify drawable hash");

		const auto& draw_function{ it->second };

		std::invoke(draw_function, *this, entity);
	}

	Flush();

	FrameBuffer::Unbind();

	GLRenderer::SetBlendMode(BlendMode::Blend);

	Camera camera{ game.scene.GetCurrent().camera.window };

	const auto& camera_vp{ camera.GetViewProjection() };

	GLRenderer::SetViewport(camera.GetViewportPosition(), camera.GetViewportSize());

	const auto& shader{ game.shader.Get<ShapeShader::Quad>() };

	shader.Bind();
	shader.SetUniform("u_ViewProjection", camera_vp);

	screen_fbo.GetTexture().Bind(1);

	SetCameraVertices(camera);

	GLRenderer::DrawElements(triangle_vao, 6, false);

	batch.vertices.clear();
	batch.indices.clear();
	batch.textures.clear();
	batch.index_offset = 0;
	render_state	   = {};
	current_fbo		   = {};
}

/*

void RenderData::AddLine(
	const V2_float& line_start, const V2_float& line_end, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
) {
	PTGN_ASSERT(line_width >= min_line_width, "-1.0f is an invalid line width for lines");
	auto vertices{
		GetLineQuadVertices(camera.ZoomIfNeeded(line_start), camera.ZoomIfNeeded(line_end),
line_width)
	};
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	batch.AddFilledQuad(vertices, color, depth, pixel_rounding);
}

void RenderData::AddLines(
	const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, bool
connect_last_to_first, bool debug ) { std::size_t vertex_modulo{ vertices.size() };

	if (!connect_last_to_first) {
		PTGN_ASSERT(
			vertices.size() >= 2, "Lines which do not connect the last vertex to the first
vertex " "must have at least 2 vertices"
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
			vertices[i], vertices[(i + 1) % vertex_modulo], line_width, depth, camera,
blend_mode, color, debug
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
	const V4_float& color, bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, white_texture,
		game.shader.Get<ShapeShader::Circle>(), camera, blend_mode, depth, debug
	) };
	batch.AddHollowEllipse(vertices, line_width, radius, color, depth, pixel_rounding);
}

void RenderData::AddTexturedQuad(
	const Transform& transform, const V2_float& size, Origin origin,
	const std::array<V2_float, Batch::quad_vertex_count>& tex_coords, const Texture& texture,
	const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color,
	bool debug
) {
	auto& batch{ GetBatch(
		Batch::quad_vertex_count, Batch::quad_index_count, texture,
		game.shader.Get<ShapeShader::Quad>(), camera, blend_mode, depth, debug
	) };
	std::array<V2_float, Batch::quad_vertex_count> vertices;
	if (size.IsZero()) {
		vertices = camera_vertices;
	} else {
		vertices = impl::GetVertices(
			{ camera.ZoomIfNeeded(transform.position), transform.rotation, transform.scale },
size, origin
		);
	}
	float texture_index{ GetTextureIndex(batch, texture) };
	PTGN_ASSERT(texture_index > 0.0f, "Failed to find a valid texture index");
	batch.AddTexturedQuad(vertices, tex_coords, texture_index, color, depth, pixel_rounding);
}

void RenderData::AddEllipse(
	const V2_float& center, const V2_float& radius, float line_width, const Depth& depth,
	const Camera& camera, BlendMode blend_mode, const V4_float& color, float rotation, bool
debug ) { PTGN_ASSERT(radius.x > 0.0f && radius.y > 0.0f, "Invalid ellipse radius"); V2_float
diameter{ radius * 2.0f }; auto vertices{ impl::GetVertices({ camera.ZoomIfNeeded(center),
rotation }, diameter, Origin::Center)
	};
	if (line_width == -1.0f) {
		AddFilledEllipse(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddHollowEllipse(
			vertices, line_width, V2_float{ radius }, depth, camera, blend_mode, color, debug
		);
	}
}

void RenderData::AddPolygon(
	std::vector<V2_float> vertices, float line_width, const Depth& depth, const Camera& camera,
	BlendMode blend_mode, const V4_float& color, bool debug
) {
	PTGN_ASSERT(vertices.size() >= 3, "Polygon must have at least 3 vertices");

	for (auto& v : vertices) {
		v = camera.ZoomIfNeeded(v);
	}

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
	V2_float position, const Depth& depth, const Camera& camera, BlendMode blend_mode,
	const V4_float& color, bool debug
) {
	constexpr V2_float half{ 0.5f };
	position = camera.ZoomIfNeeded(position);
	AddFilledQuad(
		{ position - half, position + V2_float{ half.x, -half.y }, position + half,
		  position + V2_float{ -half.x, half.y } },
		depth, camera, blend_mode, color, debug
	);
}

void RenderData::AddTriangle(
	std::array<V2_float, 3> vertices, float line_width, const Depth& depth, const Camera&
camera, BlendMode blend_mode, const V4_float& color, bool debug ) { for (auto& v : vertices) {
		v = camera.ZoomIfNeeded(v);
	}
	if (line_width == -1.0f) {
		AddFilledTriangle(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddLines(ToVector(vertices), line_width, depth, camera, blend_mode, color, true, debug);
	}
}

void RenderData::AddQuad(
	const V2_float& position, const V2_float& size, Origin origin, float line_width,
	const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color,
	float rotation, bool debug
) {
	std::array<V2_float, Batch::quad_vertex_count> vertices;
	if (size.IsZero()) {
		vertices = camera_vertices;
	} else {
		vertices = impl::GetVertices({ camera.ZoomIfNeeded(position), rotation }, size, origin);
	}
	if (line_width == -1.0f) {
		AddFilledQuad(vertices, depth, camera, blend_mode, color, debug);
	} else {
		AddLines(ToVector(vertices), line_width, depth, camera, blend_mode, color, true, debug);
	}
}

bool RenderData::FlushLights(
	Batch& batch, const FrameBuffer& frame_buffer, const V2_float& window_size, const Depth&
depth ) { if (batch.lights.empty()) { return false;
	}

	PTGN_ASSERT(batch.texture_ids.empty());

	bool lights_found{ false };

	for (const auto& e : batch.lights) {
		if (!e.IsVisible()) {
			continue;
		}

		auto light_camera{ e.GetOrParentOrDefault<Camera>() };

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

*/

/*
void RenderData::SortEntitiesByY(std::vector<Entity>&) {
	// TODO: Investigate making this faster for large numbers of static entities.
	std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		return a.GetLowestY() < b.GetLowestY();
	});
}

void RenderData::SetVertexArrayToWindow(
	const Color& color, const Depth& depth, float texture_index
) {
	std::array<Batch::IndexType, Batch::quad_index_count> indices{ 0, 1, 2, 2, 3, 0 };

	const auto& positions{ camera_vertices };
	auto tex_coords{ GetDefaultTextureCoordinates() };
	GetQuadVertices
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

*/

} // namespace ptgn::impl