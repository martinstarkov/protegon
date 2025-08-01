#include "renderer/render_data.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "api/origin.h"
#include "common/assert.h"
#include "components/common.h"
#include "components/drawable.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"
#include "core/window.h"
#include "math/geometry.h"
#include "math/geometry/rect.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/api/vertex.h"
#include "renderer/buffers/buffer.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/gl/gl_types.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn::impl {

std::array<Vertex, 3> GetTriangleVertices(
	const std::array<V2_float, 3>& triangle_points, const Color& color, const Depth& depth
) {
	constexpr std::array<V2_float, 3> texture_coordinates{
		V2_float{ 0.0f, 0.0f }, // lower-left corner
		V2_float{ 1.0f, 0.0f }, // lower-right corner
		V2_float{ 0.5f, 1.0f }, // top-center corner
	};

	std::array<Vertex, 3> vertices{};

	auto c{ color.Normalized() };

	PTGN_ASSERT(vertices.size() == triangle_points.size());
	PTGN_ASSERT(vertices.size() == texture_coordinates.size());

	for (std::size_t i{ 0 }; i < triangle_points.size(); i++) {
		vertices[i].position  = { triangle_points[i].x, triangle_points[i].y,
								  static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
		vertices[i].tex_index = { 0.0f };
	}

	return vertices;
}

std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	float texture_index, std::array<V2_float, 4> texture_coordinates, bool flip_vertices
) {
	std::array<Vertex, 4> vertices{};

	auto c{ color.Normalized() };

	if (flip_vertices) {
		FlipTextureCoordinates(texture_coordinates, Flip::Vertical);
	}

	PTGN_ASSERT(vertices.size() == quad_points.size());
	PTGN_ASSERT(vertices.size() == texture_coordinates.size());

	for (std::size_t i{ 0 }; i < vertices.size(); ++i) {
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
		vertices[i].tex_index = { texture_index };
	}

	return vertices;
}

void SortEntities(std::vector<Entity>& entities) {
	// PTGN_PROFILE_FUNCTION();
	std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		auto depth_a{ a.GetDepth() };
		auto depth_b{ b.GetDepth() };
		if (depth_a == depth_b) {
			// Depth order of equal depth element is maintained via creation order.
			return a.WasCreatedBefore(b);
		}
		return depth_a < depth_b;
	});
}

void GetRenderArea(
	const V2_float& screen_size, const V2_float& target_size, ResolutionMode mode,
	V2_float& out_position, V2_float& out_size
) {
	switch (mode) {
		case ResolutionMode::Disabled:
		case ResolutionMode::Stretch:
			out_position = {};
			out_size	 = screen_size;
			break;

		case ResolutionMode::Letterbox: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			float scale	 = std::min(ratio.x, ratio.y);
			out_size	 = V2_float{ target_size } * scale;
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}

		case ResolutionMode::Overscan: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			float scale	 = std::max(ratio.x, ratio.y);
			out_size	 = V2_float{ target_size } * scale;
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}

		case ResolutionMode::IntegerScale: {
			PTGN_ASSERT(!target_size.IsZero());
			auto ratio{ screen_size / target_size };
			int scale	 = static_cast<int>(std::min(ratio.x, ratio.y));
			scale		 = std::max(1, scale); // avoid zero
			out_size	 = V2_float{ target_size } * static_cast<float>(scale);
			out_position = (V2_float{ screen_size } - out_size) / 2.0f;
			break;
		}
		default: PTGN_ERROR("Unsupported resolution mode");
	}
}

ShaderPass::ShaderPass(const Shader& shader, UniformCallback uniform_callback) :
	shader_{ &shader }, uniform_callback_{ uniform_callback } {}

const Shader& ShaderPass::GetShader() const {
	PTGN_ASSERT(shader_ != nullptr);
	return *shader_;
}

void ShaderPass::Invoke(Entity entity) const {
	PTGN_ASSERT(shader_ != nullptr);
	if (uniform_callback_) {
		std::invoke(uniform_callback_, entity, *shader_);
	}
}

bool ShaderPass::operator==(const ShaderPass& other) const {
	return shader_ == other.shader_ && uniform_callback_ == other.uniform_callback_;
}

bool ShaderPass::operator!=(const ShaderPass& other) const {
	return !(*this == other);
}

DrawContext::DrawContext(const V2_int& size) :
	frame_buffer{ impl::Texture{
		nullptr, size, HDR_ENABLED ? TextureFormat::HDR_RGBA : TextureFormat::RGBA8888 } },
	timer{ true } {}

DrawContextPool::DrawContextPool(milliseconds max_age) : max_age_{ max_age } {}

void DrawContextPool::TrimExpired() {
	for (auto it{ contexts_.begin() }; it != contexts_.end();) {
		const auto& context{ *it };
		if (!context->in_use && !context->keep_alive && context->timer.Elapsed() > max_age_ &&
			context.use_count() <= 1) {
			it = contexts_.erase(it);
		} else {
			if (!context->keep_alive) {
				context->in_use = false;
			}
			++it;
		}
	}
}

std::shared_ptr<DrawContext> DrawContextPool::Get(V2_int size) {
	PTGN_ASSERT(size.x > 0 && size.y > 0);

	constexpr V2_int max_resolution{ 4096, 2160 };

	size.x = std::min(size.x, max_resolution.x);
	size.y = std::min(size.y, max_resolution.y);

	std::shared_ptr<DrawContext> spare_context;

	for (auto& context : contexts_) {
		if (!context->in_use) {
			spare_context = context;
			break;
		}
	}

	if (!spare_context) {
		return contexts_.emplace_back(std::make_shared<DrawContext>(size));
	}

	auto& texture{ spare_context->frame_buffer.GetTexture() };

	if (texture.GetSize() != size) {
		texture.Resize(size);
	}

	spare_context->in_use = true;
	spare_context->timer.Start(true);

	return spare_context;
}

void RenderData::AddPoint(
	const V2_float& position, const Color& tint, const Depth& depth, const RenderState& state
) {
	AddQuad(Transform{ position }, V2_float{ 1.0f }, Origin::Center, tint, depth, -1.0f, state);
}

void RenderData::AddLines(
	const std::vector<V2_float>& line_points, const Color& tint, const Depth& depth,
	float line_width, bool connect_last_to_first, const RenderState& state
) {
	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for line");

	SetState(state);

	std::size_t vertex_modulo{ line_points.size() };

	if (!connect_last_to_first) {
		PTGN_ASSERT(
			line_points.size() >= 2,
			"Lines which do not connect the last vertex to the first vertex "
			"must have at least 2 vertices"
		);
		vertex_modulo -= 1;
	} else {
		PTGN_ASSERT(
			line_points.size() >= 3, "Lines which connect the last vertex to the first vertex "
									 "must have at least 3 vertices"
		);
	}

	for (std::size_t i{ 0 }; i < line_points.size(); i++) {
		Line l{ line_points[i], line_points[(i + 1) % vertex_modulo] };
		auto quad_points{ l.GetWorldQuadVertices(Transform{}, line_width) };
		auto quad_vertices{
			impl::GetQuadVertices(quad_points, tint, depth, 0.0f, impl::default_texture_coordinates)
		};

		AddVertices(quad_vertices, quad_indices);
	}
}

void RenderData::AddLine(
	const V2_float& start, const V2_float& end, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for line");

	Line l{ start, end };
	auto quad_points{ l.GetWorldQuadVertices(Transform{}, line_width) };

	auto quad_vertices{
		impl::GetQuadVertices(quad_points, tint, depth, 0.0f, impl::default_texture_coordinates)
	};

	SetState(state);
	AddVertices(quad_vertices, quad_indices);
}

void RenderData::AddTriangle(
	const std::array<V2_float, 3>& triangle_points, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	auto triangle_vertices{ impl::GetTriangleVertices(triangle_points, tint, depth) };

	AddShape(triangle_vertices, triangle_indices, triangle_points, line_width, state);
}

void RenderData::AddQuad(
	const Transform& transform, const V2_float& size, Origin origin, const Color& tint,
	const Depth& depth, float line_width, const RenderState& state
) {
	auto quad_points{ Rect{ size }.GetWorldVertices(transform, origin) };
	auto quad_vertices{
		impl::GetQuadVertices(quad_points, tint, depth, 0.0f, impl::default_texture_coordinates)
	};

	AddShape(quad_vertices, quad_indices, quad_points, line_width, state);
}

void RenderData::AddPolygon(
	const std::vector<V2_float>& polygon_points, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	PTGN_ASSERT(polygon_points.size() >= 3, "Polygon must have at least 3 points");

	if (line_width == -1.0f) {
		SetState(state);
		auto triangles{ Triangulate(polygon_points.data(), polygon_points.size()) };
		for (const auto& triangle : triangles) {
			auto triangle_vertices{ impl::GetTriangleVertices(triangle, tint, depth) };
			AddVertices(triangle_vertices, triangle_indices);
		}
	} else {
		AddLines(polygon_points, tint, depth, line_width, true, state);
	}
}

void RenderData::AddCircle(
	const Transform& transform, float radius, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	AddEllipse(transform, V2_float{ radius, radius }, tint, depth, line_width, state);
}

void RenderData::AddEllipse(
	const Transform& transform, const V2_float& radii, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	if (line_width == -1.0f) {
		// Internally line width for a filled ellipse is 1.0f.
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for circle");

		// Internally line width for a completely hollow ellipse is 0.0f.
		// TODO: Check that dividing by std::max(radii.x, radii.y) does not cause
		// any unexpected bugs.
		line_width = 0.005f + line_width / std::min(radii.x, radii.y);
	}

	auto quad_points{ Rect{ radii * 2.0f }.GetWorldVertices(transform) };
	auto points{ impl::GetQuadVertices(
		quad_points, tint, depth, line_width, impl::default_texture_coordinates
	) };

	SetState(state);
	AddVertices(points, quad_indices);
}

void RenderData::AddTexturedQuad(
	const Texture& texture, const Transform& transform, const V2_float& size, Origin origin,
	const Color& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates,
	const RenderState& state, const PreFX& pre_fx
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw textured quad with invalid texture");
	PTGN_ASSERT(!size.IsZero(), "Cannot draw textured quad with zero size");

	SetState(state);

	auto texture_points{ Rect{ size }.GetWorldVertices(transform, origin) };

	auto texture_vertices{
		impl::GetQuadVertices(texture_points, tint, depth, 0.0f, texture_coordinates)
	};

	auto texture_id{ texture.GetId() };
	auto texture_size{ texture.GetSize() };

	PTGN_ASSERT(!texture_size.IsZero(), "Texture must have a non-zero size");

	bool pre_fx_exist{ !pre_fx.pre_fx_.empty() };

	std::shared_ptr<DrawContext> context;

	if (pre_fx_exist) {
		V2_float extents{ texture_size * 0.5f };
		Matrix4 camera{ Matrix4::Orthographic(
			-extents.x, extents.x, extents.y, -extents.y, -std::numeric_limits<float>::infinity(),
			std::numeric_limits<float>::infinity()
		) };

		std::array<V2_float, 4> verts{};
		for (std::size_t i{ 0 }; i < verts.size(); i++) {
			verts[i] = impl::default_texture_coordinates[i] * texture_size - extents;
		}

		auto ping{ draw_context_pool.Get(texture_size) };
		auto pong{ draw_context_pool.Get(texture_size) };

		PTGN_ASSERT(ping != nullptr && pong != nullptr);
		PTGN_ASSERT(ping->frame_buffer.GetTexture().GetSize() == texture_size);
		PTGN_ASSERT(pong->frame_buffer.GetTexture().GetSize() == texture_size);

		for (auto it = pre_fx.pre_fx_.begin(); it != pre_fx.pre_fx_.end(); ++it) {
			const auto& fx{ *it };
			DrawTo(pong->frame_buffer);
			PTGN_ASSERT(pong->frame_buffer.IsBound());
			impl::GLRenderer::ClearToColor(color::Transparent);

			const auto& shader_pass{ fx.Get<ShaderPass>() };
			const auto& shader{ shader_pass.GetShader() };

			BindCamera(shader, camera);

			// assert that vertices is screen vertices.
			GLRenderer::SetViewport({ 0, 0 }, texture_size);
			GLRenderer::SetBlendMode(fx.GetBlendMode());

			if (it == pre_fx.pre_fx_.begin()) {
				ReadFrom(texture);
			} else {
				ReadFrom(ping->frame_buffer);
			}

			// TODO: Cache this somehow?
			SetCameraVertices(verts, depth);

			shader.SetUniform("u_Texture", 1);
			shader.SetUniform("u_Resolution", V2_float{ texture_size });

			shader_pass.Invoke(fx);

			DrawVertexArray(quad_indices.size());

			std::swap(ping, pong);
		}

		pong->in_use = false;

		texture_id = ping->frame_buffer.GetTexture().GetId();

		white_texture.Bind(0);

		force_flush = true;
	}

	float texture_index{ 0.0f };

	bool existing_texture{ GetTextureIndex(texture_id, texture_index) };

	for (auto& v : texture_vertices) {
		v.tex_index = { texture_index };
	}

	AddVertices(texture_vertices, quad_indices);

	if (!existing_texture) {
		// Must be done after AddVertices and SetState because both of them may Flush the current
		// batch, which will clear textures.
		textures.emplace_back(texture_id);
	}

	PTGN_ASSERT(textures.size() < max_texture_slots);
}

void RenderData::Init() {
	// GLRenderer::EnableLineSmoothing();

	GLRenderer::DisableGammaCorrection();

	max_texture_slots = GLRenderer::GetMaxTextureSlots();

	const auto& screen_shader{ game.shader.Get<ScreenShader::Default>() };
	PTGN_ASSERT(screen_shader.IsValid());
	screen_shader.Bind();
	screen_shader.SetUniform("u_Texture", 1);

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
	white_texture.Bind(0);
	Texture::SetActiveSlot(1);

	intermediate_target = {};

#ifdef PTGN_PLATFORM_MACOS
	// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	// texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		Texture::Bind(white_texture.GetId(), slot);
	}
#endif

	SetState(RenderState{ {}, BlendMode::None, {} });

	render_manager.Refresh();
}

bool RenderData::GetTextureIndex(std::uint32_t texture_id, float& out_texture_index) {
	PTGN_ASSERT(texture_id != white_texture.GetId());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < textures.size(); i++) {
		if (textures[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			out_texture_index = static_cast<float>(i + 1);
			return true;
		}
	}
	// Batch is at texture capacity.
	if (static_cast<std::uint32_t>(textures.size()) == max_texture_slots - 1) {
		Flush();
	}
	out_texture_index = static_cast<float>(textures.size() + 1);
	return false;
}

bool RenderData::SetState(const RenderState& new_render_state) {
	if (new_render_state != render_state || force_flush) {
		Flush();
		render_state = new_render_state;
		return true;
	}
	return false;
}

void RenderData::BindCamera(const Shader& shader, const Matrix4& view_projection) {
	shader.Bind();
	shader.SetUniform("u_ViewProjection", view_projection);
}

void RenderData::UpdateVertexArray(
	const Vertex* data_vertices, std::size_t vertex_count, const Index* data_indices,
	std::size_t index_count
) {
	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		data_vertices, 0, static_cast<std::uint32_t>(vertex_count), sizeof(Vertex), false, true
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		data_indices, 0, static_cast<std::uint32_t>(index_count), sizeof(Index), false, true
	);
}

void RenderData::SetRenderParameters(const Camera& camera, BlendMode blend_mode) {
	SetViewport(camera);
	GLRenderer::SetBlendMode(blend_mode);
}

void RenderData::SetViewport(const Camera& camera) {
	GLRenderer::SetViewport(camera.GetViewportPosition(), camera.GetViewportSize());
}

void RenderData::SetCameraVertices(const std::array<V2_float, 4>& positions, const Depth& depth) {
	camera_vertices =
		GetQuadVertices(positions, color::White, depth, 1.0f, default_texture_coordinates, true);

	UpdateVertexArray(camera_vertices, quad_indices);
}

void RenderData::SetCameraVertices(const Camera& camera) {
	SetCameraVertices(camera.GetVertices(), camera.GetDepth());
}

void RenderData::DrawTo(const FrameBuffer& frame_buffer) {
	PTGN_ASSERT(frame_buffer.IsValid());
	frame_buffer.Bind();
	impl::GLRenderer::SetViewport({}, frame_buffer.GetTexture().GetSize());
}

void RenderData::DrawTo(const RenderTarget& render_target) {
	PTGN_ASSERT(render_target);
	DrawTo(render_target.GetFrameBuffer());
}

void RenderData::ReadFrom(const Texture& texture) {
	PTGN_ASSERT(texture.IsValid());
	texture.Bind(1);
}

void RenderData::ReadFrom(const FrameBuffer& frame_buffer) {
	PTGN_ASSERT(frame_buffer.IsValid());
	ReadFrom(frame_buffer.GetTexture());
}

void RenderData::ReadFrom(const RenderTarget& render_target) {
	PTGN_ASSERT(render_target);
	ReadFrom(render_target.GetFrameBuffer());
}

void RenderData::AddShader(
	Entity entity, const RenderState& state, BlendMode target_blend_mode,
	const Color& target_clear_color, bool uses_scene_texture
) {
	Scene& scene{ entity.GetScene() };
	PTGN_ASSERT(&scene == &game.scene.GetCurrent());
	Camera camera;
	if (bool state_changed{ SetState(state) }; state_changed || uses_scene_texture) {
		camera				= GetCamera(scene);
		intermediate_target = draw_context_pool.Get(camera.GetSize());
		DrawTo(intermediate_target->frame_buffer);
		intermediate_target->viewport_position = camera.GetViewportPosition();
		intermediate_target->viewport_size	   = camera.GetViewportSize();
		intermediate_target->clear_color	   = target_clear_color;
		intermediate_target->frame_buffer.ClearToColor(intermediate_target->clear_color);
		intermediate_target->blend_mode = target_blend_mode;
		PTGN_ASSERT(drawing_to);
		ReadFrom(drawing_to);
	} else {
		camera = GetCamera(scene);
		PTGN_ASSERT(intermediate_target->frame_buffer.IsBound());
		PTGN_ASSERT(intermediate_target);
	}

	SetCameraVertices(camera);

	GLRenderer::SetBlendMode(render_state.blend_mode);

	const auto& shader{ render_state.shader_pass.GetShader() };
	PTGN_ASSERT(shader != game.shader.Get<ShapeShader::Quad>());
	// TODO: Only update these if shader bind is dirty.
	BindCamera(shader, camera);
	shader.SetUniform("u_Texture", 1);
	// TODO: Replace.
	shader.SetUniform("u_Resolution", camera.GetViewportSize());
	render_state.shader_pass.Invoke(entity);

	// PTGN_LOG(
	// 	"Before drawing shader to intermediate target:",
	// 	intermediate_target.GetPixel({ 50, 50 })
	// );

	DrawVertexArray(quad_indices.size());

	// PTGN_LOG(
	// 	"After drawing shader to intermediate target:",
	// 	intermediate_target.GetPixel({ 50, 50 })
	// );
}

void RenderData::AddTemporaryTexture(Texture&& texture) {
	temporary_textures.emplace_back(std::move(texture));
}

void RenderData::BindTextures() const {
	PTGN_ASSERT(textures.size() < max_texture_slots);

	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures.size()); i++) {
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		Texture::Bind(textures[i], slot);
	}
}

Camera RenderData::GetCamera(Scene& scene) const {
	if (render_state.camera) {
		return render_state.camera;
	}
	auto scene_camera{ scene.camera.primary };
	PTGN_ASSERT(scene_camera);
	return scene_camera;
}

void RenderData::Flush() {
	PTGN_ASSERT(game.scene.HasCurrent());
	Flush(game.scene.GetCurrent());
}

void RenderData::Flush(Scene& scene) {
	const auto draw_vertices_to = [&](auto camera, const auto& target) {
		const auto& camera_vp{ camera.GetViewProjection() };
		DrawTo(target);
		UpdateVertexArray(vertices, indices);
		SetRenderParameters(camera, render_state.blend_mode);
		BindTextures();

		// TODO: Only set uniform if camera changed.
		BindCamera(render_state.shader_pass.GetShader(), camera_vp);

		// TODO: Call shader pass uniform.

		DrawVertexArray(indices.size());
	};
	auto camera{ GetCamera(scene) };

	if (!render_state.post_fx.post_fx_.empty()) {
		if (!vertices.empty() && !indices.empty()) {
			PTGN_ASSERT(!intermediate_target);
			intermediate_target					   = draw_context_pool.Get(camera.GetSize());
			intermediate_target->viewport_position = camera.GetViewportPosition();
			intermediate_target->viewport_size	   = camera.GetViewportSize();
			intermediate_target->clear_color	   = color::Transparent;
			intermediate_target->frame_buffer.ClearToColor(intermediate_target->clear_color);
			intermediate_target->blend_mode = render_state.blend_mode;
			// Draw vertices to intermediate target before adding post fx to it.
			/*PTGN_LOG(
				"intermediate_target center: ",
				intermediate_target->frame_buffer.GetPixel({ 400, 400 })
			);*/

			std::invoke(draw_vertices_to, camera, intermediate_target->frame_buffer);
			/*PTGN_LOG(
				"intermediate_target center: ",
				intermediate_target->frame_buffer.GetPixel({ 400, 400 })
			);*/
		}
		PTGN_ASSERT(
			intermediate_target, "Intermediate target must be used before rendering post fx"
		);

		auto ping{ intermediate_target };
		auto pong{ draw_context_pool.Get(camera.GetSize()) };
		pong->viewport_position = camera.GetViewportPosition();
		pong->viewport_size		= camera.GetViewportSize();
		pong->clear_color		= color::Transparent;
		pong->blend_mode		= render_state.blend_mode;

		for (const auto& fx : render_state.post_fx.post_fx_) {
			DrawTo(pong->frame_buffer);
			pong->frame_buffer.ClearToColor(pong->clear_color);

			const auto& shader_pass{ fx.Get<ShaderPass>() };
			const auto& shader{ shader_pass.GetShader() };

			BindCamera(shader, camera);

			/*PTGN_ASSERT(vertices.size() == 0);
			PTGN_ASSERT(indices.size() == 0);*/
			// assert that vertices is screen vertices.
			SetRenderParameters(camera, fx.GetBlendMode());

			ReadFrom(ping->frame_buffer);

			// TODO: Cache this somehow?
			SetCameraVertices(camera);

			V2_float viewport{ camera.GetViewportSize() };

			shader.SetUniform("u_Texture", 1);
			shader.SetUniform("u_Resolution", viewport);

			shader_pass.Invoke(fx);

			DrawVertexArray(quad_indices.size());

			intermediate_target = pong;
		}
	} else {
		//	PTGN_LOG("No post fx");
	}

	if (intermediate_target) {
		/*PTGN_LOG(
			"intermediate_target center: ", intermediate_target->frame_buffer.GetPixel({ 400, 400 })
		);*/
		PTGN_ASSERT(drawing_to);
		DrawTo(drawing_to);
		/*PTGN_LOG("PreDraw: ", drawing_to.GetFrameBuffer().GetPixel({ 400, 400 }));*/

		const auto& shader{ game.shader.Get<ScreenShader::Default>() };

		BindCamera(shader, camera);
		/*PTGN_ASSERT(vertices.size() == 0);
		PTGN_ASSERT(indices.size() == 0);*/
		// assert that vertices is screen vertices.
		auto blend_mode{ intermediate_target->blend_mode };
		SetRenderParameters(camera, blend_mode);

		ReadFrom(intermediate_target->frame_buffer);
		/*PTGN_LOG("Intermediate: ", intermediate_target->frame_buffer.GetPixel({ 400, 400 }));*/
		//	PTGN_LOG("Blend mode: ", intermediate_target.GetBlendMode());

		// TODO: Cache this somehow?
		SetCameraVertices(camera);

		DrawVertexArray(quad_indices.size());
		/*PTGN_LOG("PostDraw: ", drawing_to.GetPixel({ 400, 400 }));*/

	} else if (!vertices.empty() && !indices.empty()) {
		PTGN_ASSERT(drawing_to);
		std::invoke(draw_vertices_to, camera, drawing_to);
	}

	Reset();
}

void RenderData::Reset() {
	intermediate_target = {};
	vertices.clear();
	indices.clear();
	textures.clear();
	index_offset = 0;
	force_flush	 = false;
	draw_context_pool.TrimExpired();
}

void RenderData::DrawVertexArray(std::size_t index_count) const {
	GLRenderer::DrawElements(triangle_vao, index_count, false);
}

void RenderData::InvokeDrawable(const Entity& entity) {
	PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.GetImpl<IDrawable>() };

	const auto& drawable_functions{ IDrawable::data() };

	const auto it{ drawable_functions.find(drawable.hash) };

	PTGN_ASSERT(it != drawable_functions.end(), "Failed to identify drawable hash");

	const auto& draw_function{ it->second };

	std::invoke(draw_function, *this, entity);
}

void RenderData::DrawScene(Scene& scene) {
	// Loop through render targets and render their display lists onto their internal frame buffers.
	for (auto [entity, visible, drawable, frame_buffer, display_list] :
		 scene.EntitiesWith<Visible, IDrawable, impl::FrameBuffer, impl::DisplayList>()) {
		if (!visible) {
			continue;
		}
		RenderTarget rt{ entity };
		SortEntities(display_list.entities);
		drawing_to = rt;
		for (const auto& display_entity : display_list.entities) {
			InvokeDrawable(display_entity);
		}
		Flush(scene);
	}

	auto& display_list{ scene.render_target_.GetDisplayList() };
	SortEntities(display_list);
	drawing_to = scene.render_target_;
	for (const auto& entity : display_list) {
		// Skip entities which are in the display list of a custom render target.
		if (entity.Has<RenderTarget>()) {
			continue;
		}
		InvokeDrawable(entity);
	}
	Flush(scene);
}

void RenderData::DrawToScreen(Scene& scene) {
	FrameBuffer::Unbind();

	// Replace with resolution.
	auto camera{ scene.camera.window };

	auto screen_size{ game.window.GetSize() };

	PTGN_ASSERT(!screen_size.IsZero());

	V2_float renderer_position;
	V2_float renderer_size;

	impl::GetRenderArea(screen_size, resolution_, scaling_mode_, renderer_position, renderer_size);

	PTGN_ASSERT(!renderer_size.IsZero());

	// V2_float camera_scale{ 1.0f, 1.0f };
	// auto camera_points{ camera.GetVertices(camera_scale) };

	Rect r{ renderer_size };
	auto camera_points{ r.GetWorldVertices(Transform{ renderer_position }, Origin::TopLeft) };

	camera_vertices = GetQuadVertices(
		camera_points, color::White, camera.GetDepth(), 1.0f, default_texture_coordinates, true
	);
	UpdateVertexArray(camera_vertices, quad_indices);

	// TODO: Replace with viewport info.
	SetRenderParameters(camera, scene.render_target_.GetBlendMode());

	const Shader* shader{ nullptr };

	if constexpr (HDR_ENABLED) {
		shader = &game.shader.Get<OtherShader::ToneMapping>();
	} else {
		shader = &game.shader.Get<ScreenShader::Default>();
	}

	PTGN_ASSERT(shader != nullptr);

	// Replace with Matrix4::
	BindCamera(*shader, camera);

	if constexpr (HDR_ENABLED) {
		shader->SetUniform("u_Texture", 1);
		shader->SetUniform("u_Exposure", 1.0f);
		shader->SetUniform("u_Gamma", 2.2f);
	}

	ReadFrom(scene.render_target_);

	DrawVertexArray(quad_indices.size());
}

void RenderData::ClearRenderTargets(Scene& scene) const {
	scene.render_target_.Clear();

	for (auto [entity, frame_buffer] : scene.EntitiesWith<impl::FrameBuffer>()) {
		RenderTarget rt{ entity };
		rt.Clear();
		// rt.ClearDisplayList();
	}
}

void RenderData::Draw(Scene& scene) {
	// PTGN_LOG(draw_context_pool.contexts_.size());
	//  PTGN_PROFILE_FUNCTION();

	white_texture.Bind(0);

	DrawScene(scene);

	render_state		= {};
	intermediate_target = {};
	temporary_textures	= std::vector<Texture>{};

	DrawToScreen(scene);

	// TODO: Check if this is needed.
	Reset();
}

} // namespace ptgn::impl