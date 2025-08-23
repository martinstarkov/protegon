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
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/drawable.h"
#include "components/effects.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/time.h"
#include "core/timer.h"
#include "core/window.h"
#include "debug/log.h"
#include "input/input_handler.h"
#include "math/geometry.h"
#include "math/geometry/line.h"
#include "math/geometry/rect.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/api/origin.h"
#include "renderer/api/vertex.h"
#include "renderer/buffers/buffer.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/buffers/vertex_array.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/gl/gl_types.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn {

Viewport::Viewport(const V2_int& position, const V2_int& size) :
	position{ position }, size{ size } {}

namespace impl {

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

RenderState::RenderState(
	const ShaderPass& shader_pass, BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) :
	shader_pass{ shader_pass }, blend_mode{ blend_mode }, camera{ camera }, post_fx{ post_fx } {}

ShapeDrawInfo::ShapeDrawInfo(const Entity& entity) :
	transform{ GetDrawTransform(entity) },
	tint{ GetTint(entity) },
	depth{ GetDepth(entity) },
	line_width{ entity.GetOrDefault<LineWidth>() },
	state{ game.shader.Get<ShapeShader::Quad>(), GetBlendMode(entity),
		   entity.GetOrDefault<Camera>(), entity.GetOrDefault<PostFX>() } {}

void ViewportResizeScript::OnWindowResized() {
	auto& render_data{ game.renderer.GetRenderData() };
	auto window_size{ game.window.GetSize() };
	if (!render_data.logical_resolution_set_) {
		render_data.UpdateResolutions(window_size, render_data.resolution_mode_);
	}
	render_data.RecomputeViewport(window_size);
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
		shader_->Bind();
		uniform_callback_(entity, *shader_);
	}
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
	const Transform& transform, const V2_float& size, Origin draw_origin, const Color& tint,
	const Depth& depth, float line_width, const RenderState& state
) {
	PTGN_ASSERT(size.BothAboveZero(), "Cannot draw quad with invalid size");
	auto quad_points{ Rect{ size }.GetWorldVertices(transform, draw_origin) };
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

TextureId RenderData::PingPong(
	const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
	const Texture& texture, DrawTarget target, bool flip_vertices
) {
	PTGN_ASSERT(!container.empty(), "Cannot ping pong on an empty container");

	auto read{ read_context };
	auto write{ draw_context_pool.Get(target.viewport.size) };

	PTGN_ASSERT(read != nullptr && write != nullptr);
	PTGN_ASSERT(read->frame_buffer.GetTexture().GetSize() == target.viewport.size);
	PTGN_ASSERT(write->frame_buffer.GetTexture().GetSize() == target.viewport.size);

	bool use_previous_texture{ true };

	for (const auto& fx : container) {
		bool first_effect{ fx == container.front() };

		if (!first_effect && use_previous_texture) {
			std::swap(read, write);
		}

		TextureId texture_id{ 0 };

		if ((first_effect || !use_previous_texture) && texture.IsValid()) {
			texture_id = texture.GetId();
		} else {
			texture_id = read->frame_buffer.GetTexture().GetId();
		}

		const auto& shader_pass{ fx.Get<ShaderPass>() };
		const auto& shader{ shader_pass.GetShader() };

		shader.Bind();
		shader.SetUniform("u_Texture", 1);
		shader.SetUniform("u_Resolution", GetResolutionScale(target.viewport.size));
		shader_pass.Invoke(fx);

		target.texture_id	= texture_id;
		target.frame_buffer = &write->frame_buffer;
		target.tint			= GetTint(fx);
		target.blend_mode	= GetBlendMode(fx);

		DrawFullscreenQuad(shader, target, flip_vertices, use_previous_texture, color::Transparent);

		use_previous_texture = fx.GetOrDefault<UsePreviousTexture>();
	}
	read->in_use = false;

	return write->frame_buffer.GetTexture().GetId();
}

void RenderData::AddTexturedQuad(
	const Texture& texture, Transform transform, const V2_float& size, Origin origin,
	const Color& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates,
	const RenderState& state, const PreFX& pre_fx
) {
	PTGN_ASSERT(texture.IsValid(), "Cannot draw textured quad with invalid texture");
	PTGN_ASSERT(size.BothAboveZero(), "Cannot draw textured quad with zero or negative size");

	SetState(state);

	auto texture_points{ Rect{ size }.GetWorldVertices(transform, origin) };

	auto texture_vertices{
		impl::GetQuadVertices(texture_points, tint, depth, 0.0f, texture_coordinates, false)
	};

	auto texture_id{ texture.GetId() };

	if (bool has_pre_fx{ !pre_fx.pre_fx_.empty() }) {
		auto texture_size{ texture.GetSize() };

		PTGN_ASSERT(
			texture_size.BothAboveZero(), "Texture must have a valid size for it to have post fx"
		);

		Viewport viewport{ {}, texture_size };

		DrawTarget target;
		target.viewport = viewport;
		SetPointsAndProjection(target);

		texture_id =
			PingPong(pre_fx.pre_fx_, draw_context_pool.Get(viewport.size), texture, target, true);

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
		textures_.emplace_back(texture_id);
	}

	PTGN_ASSERT(textures_.size() < max_texture_slots);
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

	screen_target_ = CreateRenderTarget(
		render_manager, ResizeToResolution::Physical, color::Transparent,
		HDR_ENABLED ? TextureFormat::HDR_RGBA : TextureFormat::RGBA8888
	);
	SetBlendMode(screen_target_, BlendMode::None);

#ifdef PTGN_PLATFORM_MACOS
	// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	// texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		Texture::Bind(white_texture.GetId(), slot);
	}
#endif

	SetState(RenderState{ {}, BlendMode::None, {} });

	viewport_tracker = render_manager.CreateEntity();
	AddScript<impl::ViewportResizeScript>(viewport_tracker);
	auto window_size{ game.window.GetSize() };
	RecomputeViewport(window_size);

	render_manager.Refresh();
}

bool RenderData::GetTextureIndex(std::uint32_t texture_id, float& out_texture_index) {
	PTGN_ASSERT(texture_id != white_texture.GetId());
	// Texture exists in batch, therefore do not add it again.
	for (std::size_t i{ 0 }; i < textures_.size(); i++) {
		if (textures_[i] == texture_id) {
			// i + 1 because first texture index is white texture.
			out_texture_index = static_cast<float>(i + 1);
			return true;
		}
	}
	// Batch is at texture capacity.
	if (static_cast<std::uint32_t>(textures_.size()) == max_texture_slots - 1) {
		Flush();
	}
	out_texture_index = static_cast<float>(textures_.size() + 1);
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

V2_float RenderData::GetResolutionScale(const V2_float& viewport_size) {
	auto logical_resolution{ game.renderer.GetLogicalResolution() };
	PTGN_ASSERT(logical_resolution.BothAboveZero());
	V2_float resolution_scale{ V2_float{ viewport_size } / logical_resolution };
	PTGN_ASSERT(resolution_scale.BothAboveZero());
	return resolution_scale;
}

void RenderData::AddShader(
	Entity entity, const RenderState& state, const Color& target_clear_color,
	const TextureOrSize& texture_or_size, bool clear_between_consecutive_calls
) {
	bool state_changed{ SetState(state) };

	bool uses_size{ std::holds_alternative<V2_int>(texture_or_size) };

	// Clear the intermediate frame buffer if the shader is new (changes renderer state), or if the
	// shader uses size (no texture) and the user desires it (most often true). In the case of
	// back-to-back light rendering this is not desired.
	bool clear{ state_changed || (uses_size && clear_between_consecutive_calls) };

	if (clear_between_consecutive_calls) {
		force_flush = true;
	}

	auto target{ drawing_to_ };

	if (render_state.camera) {
		target.view_projection = render_state.camera;
		target.points		   = render_state.camera.GetWorldVertices();
	}

	target.depth	  = GetDepth(entity);
	target.tint		  = target.tint.Normalized() * GetTint(entity).Normalized();
	target.blend_mode = GetBlendMode(entity);

	if (uses_size) {
		if (!std::get<V2_int>(texture_or_size).IsZero()) {
			target.viewport.size = std::get<V2_int>(texture_or_size);
		}
	} else if (std::holds_alternative<std::reference_wrapper<const Texture>>(texture_or_size)) {
		const Texture& texture =
			std::get<std::reference_wrapper<const Texture>>(texture_or_size).get();

		PTGN_ASSERT(texture.IsValid(), "Cannot draw shader to an invalid texture");

		target.viewport.size = texture.GetSize();
		target.texture_id	 = texture.GetId();
	} else {
		PTGN_ERROR("Unknown variant value");
	}

	if (clear) {
		intermediate_target = draw_context_pool.Get(target.viewport.size);
	}

	const auto& shader{ render_state.shader_pass.GetShader() };

	PTGN_ASSERT(shader != game.shader.Get<ShapeShader::Quad>());

	shader.Bind();
	shader.SetUniform("u_Texture", 1);
	shader.SetUniform("u_Resolution", GetResolutionScale(target.viewport.size));

	render_state.shader_pass.Invoke(entity);

	target.frame_buffer = &intermediate_target->frame_buffer;

	DrawFullscreenQuad(shader, target, false, clear, target_clear_color);
}

void RenderData::AddTemporaryTexture(Texture&& texture) {
	temporary_textures.emplace_back(std::move(texture));
}

V2_float RenderData::RelativeToViewport(const V2_float& window_relative_point) const {
	V2_float scale{
		V2_float{ logical_resolution_ } / physical_viewport_.size,
	};

	V2_float local{ (window_relative_point - physical_viewport_.position) * scale };

	return local;
}

void RenderData::DrawFullscreenQuad(
	const Shader& shader, const RenderData::DrawTarget& target, bool flip_texture,
	bool clear_frame_buffer, const Color& target_clear_color
) {
	DrawCall(
		shader,
		GetQuadVertices(
			target.points, target.tint, target.depth, 1.0f, default_texture_coordinates,
			flip_texture
		),
		quad_indices, { target.texture_id }, target.frame_buffer, clear_frame_buffer,
		target_clear_color, target.blend_mode, target.viewport, target.view_projection
	);
}

void RenderData::DrawCall(
	const Shader& shader, std::span<const Vertex> vertices, std::span<const Index> indices,
	const std::vector<TextureId>& textures, const impl::FrameBuffer* frame_buffer,
	bool clear_frame_buffer, const Color& clear_color, BlendMode blend_mode,
	const Viewport& viewport, const Matrix4& view_projection
) {
	if (vertices.empty() || indices.empty()) {
		return;
	}

	if (frame_buffer) {
		frame_buffer->Bind();
	} else {
		FrameBuffer::Unbind();
	}

	if (clear_frame_buffer) {
		GLRenderer::ClearToColor(clear_color);
	}

	PTGN_ASSERT(viewport.size.BothAboveZero(), "Viewport size must be above zero");

	GLRenderer::SetViewport(viewport.position, viewport.size);
	GLRenderer::SetBlendMode(blend_mode);

	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		vertices.data(), 0, static_cast<std::uint32_t>(vertices.size()), sizeof(Vertex), false, true
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		indices.data(), 0, static_cast<std::uint32_t>(indices.size()), sizeof(Index), false, true
	);

	shader.Bind();
	shader.SetUniform("u_ViewProjection", view_projection);

	PTGN_ASSERT(textures.size() < max_texture_slots);

	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures.size()); i++) {
		PTGN_ASSERT(textures[i], "Cannot bind invalid texture");
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		Texture::Bind(textures[i], slot);
	}

	GLRenderer::DrawElements(triangle_vao, indices.size(), false);
}

void RenderData::DrawVertices(
	const Shader& shader, const RenderData::DrawTarget& target, bool clear_frame_buffer
) {
	DrawCall(
		shader, vertices_, indices_, textures_, target.frame_buffer, clear_frame_buffer,
		color::Transparent, target.blend_mode, target.viewport, target.view_projection
	);
}

void RenderData::Flush() {
	std::vector<TextureId> texture_id;

	bool has_post_fx{ !render_state.post_fx.post_fx_.empty() };

	auto target{ drawing_to_ };

	if (render_state.camera) {
		target.view_projection = render_state.camera;
		target.points		   = render_state.camera.GetWorldVertices();
	}
	target.blend_mode = render_state.blend_mode;

	if (has_post_fx) {
		PTGN_ASSERT(!intermediate_target);

		intermediate_target = draw_context_pool.Get(target.viewport.size);

		target.frame_buffer = &intermediate_target->frame_buffer;

		const auto& shader{ render_state.shader_pass.GetShader() };

		// Draw unflushed vertices to intermediate target before adding post fx to it.
		DrawVertices(shader, target, true);

		// Add post fx to the intermediate target.

		// Flip only every odd ping pong to keep the flushed target upright.
		bool flip{ render_state.post_fx.post_fx_.size() % 2 == 1 };
		auto id{ PingPong(render_state.post_fx.post_fx_, intermediate_target, {}, target, flip) };
		target.texture_id = id;
	}

	target.frame_buffer = drawing_to_.frame_buffer;

	if (intermediate_target) {
		// This branch is for when an intermediate target needs to be flushed onto the drawing_to
		// frame buffer. It is used in cases where postfx are applied, or when a shader that uses
		// the intermediate target is being flushed (for instance a set of lights rendered onto an
		// intermediate target and then flushed onto the drawing_to frame buffer).

		if (!has_post_fx) {
			// The light case discussed above.
			target.texture_id = intermediate_target->frame_buffer.GetTexture().GetId();
		}

		const auto& shader{ game.shader.Get<ScreenShader::Default>() };

		// Flush intermediate target onto drawing_to frame buffer.

		DrawFullscreenQuad(
			shader, target, has_post_fx /* Only flip if postfx have been applied. */, false,
			color::Transparent
		);

	} else if (render_state.shader_pass != ShaderPass{}) {
		// No post fx, and no intermediate target.

		const auto& shader{ render_state.shader_pass.GetShader() };

		// Draw unflushed vertices directly to drawing_to frame buffer.
		DrawVertices(shader, target, false);
	}

	Reset();
}

void RenderData::Reset() {
	intermediate_target = {};
	vertices_.clear();
	indices_.clear();
	textures_.clear();
	index_offset_ = 0;
	force_flush	  = false;
	draw_context_pool.TrimExpired();
}

void RenderData::InvokeDrawable(const Entity& entity) {
	PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.GetImpl<IDrawable>() };

	const auto& drawable_functions{ IDrawable::data() };

	const auto it{ drawable_functions.find(drawable.hash) };

	PTGN_ASSERT(it != drawable_functions.end(), "Failed to identify drawable hash");

	const auto& draw_function{ it->second };

	draw_function(*this, entity);
}

RenderData::DrawTarget RenderData::GetDrawTarget(const Scene& scene) {
	return GetDrawTarget(
		scene.render_target_, scene.camera, scene.camera.GetWorldVertices(), false
	);
}

void RenderData::SetPoints(RenderData::DrawTarget& target) {
	PTGN_ASSERT(target.viewport.size.BothAboveZero());

	target.points = { target.viewport.position,
					  target.viewport.position + V2_float{ target.viewport.size.x, 0.0f },
					  target.viewport.position + target.viewport.size,
					  target.viewport.position + V2_float{ 0.0f, target.viewport.size.y } };
}

void RenderData::SetProjection(RenderData::DrawTarget& target) {
	SetProjection(target, target.points[0], target.points[2]);
}

void RenderData::SetProjection(
	RenderData::DrawTarget& target, const V2_float& min, const V2_float& max
) {
	target.view_projection = GetProjection(min, max);
}

Matrix4 RenderData::GetProjection(const V2_float& min, const V2_float& max) {
	return Matrix4::Orthographic(
		min.x, max.x, max.y, min.y, -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	);
}

void RenderData::SetPointsAndProjection(RenderData::DrawTarget& target) {
	SetPoints(target);
	SetProjection(target);
}

RenderData::DrawTarget RenderData::GetDrawTarget(
	const RenderTarget& render_target, const Matrix4& view_projection,
	const std::array<V2_float, 4>& points, bool use_viewport
) {
	DrawTarget target;

	const auto& texture{ render_target.GetTexture() };
	auto texture_size{ render_target.GetTextureSize() };

	target.texture_size = texture_size;
	target.texture_id	= texture.GetId();

	if (const auto custom_viewport{ render_target.TryGet<Viewport>() }) {
		target.viewport = *custom_viewport;
	} else {
		target.viewport.size = texture_size;
	}

	if (use_viewport) {
		SetPointsAndProjection(target);
	} else {
		target.view_projection = view_projection;
		target.points		   = points;
	}

	target.blend_mode	= GetBlendMode(render_target);
	target.depth		= GetDepth(render_target);
	target.tint			= GetTint(render_target);
	target.frame_buffer = &render_target.GetFrameBuffer();

	return target;
}

void RenderData::DrawScene(Scene& scene) {
	// Loop through render targets and render their display lists onto their internal frame buffers.
	for (auto [entity, visible, drawable, frame_buffer, display_list] :
		 scene.InternalEntitiesWith<Visible, IDrawable, impl::FrameBuffer, impl::DisplayList>()) {
		if (!visible) {
			continue;
		}

		RenderTarget rt{ entity };

		SortByDepth(display_list.entities);

		drawing_to_ = GetDrawTarget(rt, {}, {}, true);

		for (const auto& display_entity : display_list.entities) {
			InvokeDrawable(display_entity);
		}

		Flush();
	}

	auto& display_list{ scene.render_target_.GetDisplayList() };
	SortByDepth(display_list);

	drawing_to_ = GetDrawTarget(scene);

	for (const auto& entity : display_list) {
		// Skip entities which are in the display list of a custom render target.
		// TODO: Perhaps rethink how this is done after HD render targets are introduced.
		if (entity.Has<RenderTarget>()) {
			continue;
		}
		InvokeDrawable(entity);
	}

	Flush();
}

void RenderData::RecomputeViewport(const V2_int& window_size) {
	if (!logical_resolution_.BothAboveZero()) {
		UpdateResolutions(window_size, resolution_mode_);
	}

	Viewport vp;
	vp.position = { 0, 0 };
	vp.size		= window_size;

	auto compute_aspect_fit = [&](bool letterbox_mode) {
		float window_aspect{ static_cast<float>(window_size.x) /
							 static_cast<float>(window_size.y) };
		float logical_aspect{ static_cast<float>(logical_resolution_.x) /
							  static_cast<float>(logical_resolution_.y) };

		// In letterbox mode we need require window_aspect > logical_aspect to fit height, and in
		// overscan we require window_aspect > logical_aspect to fit height.
		bool fit_height{ (window_aspect > logical_aspect) == letterbox_mode };

		if (fit_height) {
			vp.size.y = window_size.y;
			vp.size.x = static_cast<int>(static_cast<float>(window_size.y) * logical_aspect + 0.5f);
			vp.position.x = (window_size.x - vp.size.x) / 2; // left edge.
			vp.position.y = 0;
		} else {
			// Fit width.
			vp.size.x = window_size.x;
			vp.size.y = static_cast<int>(static_cast<float>(window_size.x) / logical_aspect + 0.5f);
			vp.position.x = 0;
			vp.position.y = (window_size.y - vp.size.y) / 2; // top edge.
		}
	};

	switch (resolution_mode_) {
		case LogicalResolutionMode::Letterbox:	  compute_aspect_fit(true); break;

		case LogicalResolutionMode::IntegerScale: {
			V2_int ratio{ window_size / logical_resolution_ };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			vp.size		= logical_resolution_ * scale; // scale up.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;
		}

		case LogicalResolutionMode::Stretch:
			// Viewport is full window (default).
			break;

		case LogicalResolutionMode::Disabled:
			vp.size		= logical_resolution_;		   // no change.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;

		case LogicalResolutionMode::Overscan: compute_aspect_fit(false); break;

		default:							  PTGN_ERROR("Unsupported resolution mode")
	}

	if (vp != physical_viewport_) {
		// Only update viewport if it changed. This reduces PhysicalResolutionChanged event
		// dispatch.
		physical_viewport_			 = vp;
		physical_resolution_changed_ = true;
	}
}

void RenderData::UpdateResolutions(
	const V2_int& logical_resolution, LogicalResolutionMode logical_resolution_mode
) {
	if (logical_resolution_ == logical_resolution && resolution_mode_ == logical_resolution_mode) {
		return;
	}
	auto window_size{ game.window.GetSize() };
	logical_resolution_			= logical_resolution;
	resolution_mode_			= logical_resolution_mode;
	logical_resolution_changed_ = true;
	RecomputeViewport(window_size);
}

void RenderData::ClearScreenTarget() const {
	screen_target_.Clear();
}

void RenderData::ClearRenderTargets(Scene& scene) const {
	scene.render_target_.Clear();

	for (auto [entity, frame_buffer] : scene.EntitiesWith<impl::FrameBuffer>()) {
		RenderTarget rt{ entity };
		rt.Clear();
		// rt.ClearDisplayList();
	}
}

void RenderData::DrawFromTo(
	const RenderTarget& source_target, const std::array<V2_float, 4>& points,
	const Matrix4& projection, const Viewport& viewport, const FrameBuffer* destination_buffer
) {
	const Shader* shader{ nullptr };

	if constexpr (HDR_ENABLED) {
		shader = &game.shader.Get<OtherShader::ToneMapping>();
		PTGN_ASSERT(shader != nullptr);
		shader->Bind();
		shader->SetUniform("u_Texture", 1);
		shader->SetUniform("u_Exposure", 1.0f);
		shader->SetUniform("u_Gamma", 2.2f);
	} else {
		shader = &game.shader.Get<ScreenShader::Default>();
		PTGN_ASSERT(shader != nullptr);
	}

	auto target{ GetDrawTarget(source_target, {}, {}, false) };
	target.view_projection = projection;
	target.points		   = points;
	target.viewport		   = viewport;
	target.frame_buffer	   = destination_buffer;

	DrawFullscreenQuad(*shader, target, true, false, color::Transparent);
}

void RenderData::DrawScreenTarget() {
	auto projection{
		GetProjection(-physical_viewport_.size / 2.0f, physical_viewport_.size / 2.0f)
	};
	std::array<V2_float, 4> points{
		-physical_viewport_.size / 2.0f,
		V2_float{ physical_viewport_.size.x / 2.0f, -physical_viewport_.size.y / 2.0f },
		physical_viewport_.size / 2.0f,
		V2_float{ -physical_viewport_.size.x / 2.0f, physical_viewport_.size.y / 2.0f }
	};
	//	PTGN_LOG("Mouse position in window: ", game.input.GetMouseWindowPosition());
	DrawFromTo(screen_target_, points, projection, physical_viewport_, nullptr);
}

void RenderData::Draw(Scene& scene) {
	// PTGN_LOG(draw_context_pool.contexts_.size());
	//  PTGN_PROFILE_FUNCTION();

	white_texture.Bind(0);

	DrawScene(scene);

	auto projection{ GetProjection(-logical_resolution_ / 2.0f, logical_resolution_ / 2.0f) };

	Transform scene_transform{ GetTransform(scene.render_target_) };

	auto points{ Rect{ scene.camera.GetViewportSize() }.GetWorldVertices(scene_transform) };

	Viewport viewport;
	viewport.position = {};
	viewport.size	  = physical_viewport_.size;

	DrawFromTo(
		scene.render_target_, points, projection, viewport, &screen_target_.GetFrameBuffer()
	);

	Reset();

	temporary_textures = std::vector<Texture>{};
	render_state	   = {};
}

} // namespace impl

} // namespace ptgn