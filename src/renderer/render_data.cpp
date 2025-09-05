#include "renderer/render_data.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
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
#include "core/resolution.h"
#include "core/script.h"
#include "core/time.h"
#include "core/timer.h"
#include "core/window.h"
#include "debug/log.h"
#include "math/geometry.h"
#include "math/geometry/line.h"
#include "math/geometry/rect.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
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

namespace ptgn {

Viewport::Viewport(const V2_int& position, const V2_int& size) :
	position{ position }, size{ size } {}

namespace impl {

RenderState::RenderState(
	const ShaderPass& shader_pass, BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) :
	shader_pass{ shader_pass }, blend_mode{ blend_mode }, camera{ camera }, post_fx{ post_fx } {}

ShapeDrawInfo::ShapeDrawInfo(const Entity& entity) :
	transform{ GetDrawTransform(entity) },
	tint{ GetTint(entity) },
	depth{ GetDepth(entity) },
	line_width{ entity.GetOrDefault<LineWidth>() },
	state{ game.shader.Get("quad"), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
		   entity.GetOrDefault<PostFX>() } {}

void ViewportResizeScript::OnWindowResized() {
	auto& render_data{ game.renderer.GetRenderData() };
	auto window_size{ game.window.GetSize() };
	if (!render_data.game_size_set_) {
		render_data.UpdateResolutions(window_size, render_data.resolution_mode_);
	}
	render_data.RecomputeDisplaySize(window_size);
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

DrawContext::DrawContext(const V2_int& size, TextureFormat texture_format) :
	frame_buffer{ Texture{ nullptr, size, texture_format } }, timer{ true } {}

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

std::shared_ptr<DrawContext> DrawContextPool::Get(V2_int size, TextureFormat texture_format) {
	PTGN_ASSERT(size.x > 0 && size.y > 0);

	constexpr V2_int max_resolution{ 4096, 2160 };

	size.x = std::min(size.x, max_resolution.x);
	size.y = std::min(size.y, max_resolution.y);

	std::shared_ptr<DrawContext> spare_context;

	for (auto& context : contexts_) {
		if (!context->in_use && context->frame_buffer.GetTexture().GetFormat() == texture_format) {
			spare_context = context;
			break;
		}
	}

	if (!spare_context) {
		return contexts_.emplace_back(std::make_shared<DrawContext>(size, texture_format));
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
			Vertex::GetQuad(quad_points, tint, depth, { 0.0f }, GetDefaultTextureCoordinates())
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
		Vertex::GetQuad(quad_points, tint, depth, { 0.0f }, GetDefaultTextureCoordinates())
	};

	SetState(state);
	AddVertices(quad_vertices, quad_indices);
}

void RenderData::AddTriangle(
	const std::array<V2_float, 3>& triangle_points, const Color& tint, const Depth& depth,
	float line_width, const RenderState& state
) {
	auto triangle_vertices{ Vertex::GetTriangle(triangle_points, tint, depth) };

	AddShape(triangle_vertices, triangle_indices, triangle_points, line_width, state);
}

void RenderData::AddQuad(
	const Transform& transform, const V2_float& size, Origin draw_origin, const Color& tint,
	const Depth& depth, float line_width, const RenderState& state
) {
	PTGN_ASSERT(size.BothAboveZero(), "Cannot draw quad with invalid size");
	auto quad_points{ Rect{ size }.GetWorldVertices(transform, draw_origin) };
	auto quad_vertices{
		Vertex::GetQuad(quad_points, tint, depth, { 0.0f }, GetDefaultTextureCoordinates())
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
			auto triangle_vertices{ Vertex::GetTriangle(triangle, tint, depth) };
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
	auto points{
		Vertex::GetQuad(quad_points, tint, depth, { line_width }, GetDefaultTextureCoordinates())
	};

	SetState(state);
	AddVertices(points, quad_indices);
}

TextureId RenderData::PingPong(
	const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
	const Texture& texture, DrawTarget target, bool flip_vertices
) {
	PTGN_ASSERT(!container.empty(), "Cannot ping pong on an empty container");

	auto read{ read_context };
	auto write{ draw_context_pool.Get(target.viewport.size, target.texture_format) };

	PTGN_ASSERT(read != nullptr && write != nullptr);
	PTGN_ASSERT(read->frame_buffer.GetTexture().GetSize() == target.viewport.size);
	PTGN_ASSERT(write->frame_buffer.GetTexture().GetSize() == target.viewport.size);

	bool use_previous_texture{ true };

	for (const auto& fx : container) {
		PTGN_ASSERT(fx.Has<ShaderPass>());

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
		shader.SetUniform("u_ViewportSize", V2_float{ target.viewport.size });
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
		Vertex::GetQuad(texture_points, tint, depth, { 0.0f }, texture_coordinates, false)
	};

	auto texture_id{ texture.GetId() };

	if (!pre_fx.pre_fx_.empty()) {
		auto texture_size{ texture.GetSize() };

		PTGN_ASSERT(
			texture_size.BothAboveZero(), "Texture must have a valid size for it to have post fx"
		);

		Viewport viewport{ {}, texture_size };

		DrawTarget target;
		target.viewport		  = viewport;
		target.texture_format = texture.GetFormat();
		SetPointsAndProjection(target);

		texture_id = PingPong(
			pre_fx.pre_fx_, draw_context_pool.Get(viewport.size, target.texture_format), texture,
			target, true
		);

		white_texture.Bind(0);

		force_flush = true;
	}

	float texture_index{ 0.0f };

	bool existing_texture{ GetTextureIndex(texture_id, texture_index) };

	Vertex::SetTextureIndex(texture_vertices, texture_index);

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

	const auto& screen_shader{ game.shader.Get("screen_default") };
	PTGN_ASSERT(screen_shader.IsValid());
	screen_shader.Bind();
	screen_shader.SetUniform("u_Texture", 1);

	const auto& quad_shader{ game.shader.Get("quad") };

	PTGN_ASSERT(quad_shader.IsValid());
	PTGN_ASSERT(game.shader.Get("circle").IsValid());
	PTGN_ASSERT(game.shader.Get("screen_default").IsValid());
	PTGN_ASSERT(game.shader.Get("light").IsValid());

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
		PrimitiveMode::Triangles, std::move(quad_vb), Vertex::GetLayout(), std::move(quad_ib)
	);

	white_texture = Texture(static_cast<const void*>(&color::White), { 1, 1 });
	white_texture.Bind(0);
	Texture::SetActiveSlot(1);

	intermediate_target = {};

	screen_target_ = CreateRenderTarget(
		render_manager, ResizeMode::DisplaySize, color::Transparent, TextureFormat::RGBA8888
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
	AddScript<ViewportResizeScript>(viewport_tracker);
	auto window_size{ game.window.GetSize() };
	RecomputeDisplaySize(window_size);

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

void RenderData::AddShader(
	Entity entity, const RenderState& state, const Color& target_clear_color,
	const TextureOrSize& texture_or_size, bool clear_between_consecutive_calls,
	BlendMode blend_mode, TextureFormat texture_format
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
	target.blend_mode = blend_mode;

	if (uses_size) {
		if (!std::get<V2_int>(texture_or_size).IsZero()) {
			target.viewport.size = std::get<V2_int>(texture_or_size);
		}
		target.texture_format = texture_format;
	} else if (std::holds_alternative<std::reference_wrapper<const Texture>>(texture_or_size)) {
		const Texture& texture =
			std::get<std::reference_wrapper<const Texture>>(texture_or_size).get();

		PTGN_ASSERT(texture.IsValid(), "Cannot draw shader to an invalid texture");

		target.viewport.size  = texture.GetSize();
		target.texture_id	  = texture.GetId();
		target.texture_format = texture.GetFormat();
	} else {
		PTGN_ERROR("Unknown variant value");
	}

	if (clear) {
		intermediate_target = draw_context_pool.Get(target.viewport.size, target.texture_format);
	}

	const auto& shader{ render_state.shader_pass.GetShader() };

	PTGN_ASSERT(shader != game.shader.Get("quad"));

	shader.Bind();
	shader.SetUniform("u_Texture", 1);
	shader.SetUniform("u_ViewportSize", V2_float{ target.viewport.size });

	render_state.shader_pass.Invoke(entity);

	target.frame_buffer = &intermediate_target->frame_buffer;

	DrawFullscreenQuad(shader, target, false, clear, target_clear_color);
}

void RenderData::AddTemporaryTexture(Texture&& texture) {
	temporary_textures.emplace_back(std::move(texture));
}

std::size_t RenderData::GetMaxTextureSlots() const {
	if (!max_texture_slots) {
		max_texture_slots = GLRenderer::GetMaxTextureSlots();
	}
	return max_texture_slots;
}

void RenderData::AddShape(
	std::span<const Vertex> shape_vertices, std::span<const Index> shape_indices,
	std::span<const V2_float> shape_points, float line_width, const RenderState& state
) {
	SetState(state);

	if (line_width == -1.0f) {
		AddVertices(shape_vertices, shape_indices);
	} else {
		// We need a mutable copy of the vertices when drawing lines
		std::vector<Vertex> mutable_vertices{ shape_vertices.begin(), shape_vertices.end() };
		AddLinesImpl(mutable_vertices, shape_indices, shape_points, line_width, state);
	}
}

void RenderData::AddLinesImpl(
	std::span<Vertex> line_vertices, std::span<const Index> line_indices,
	std::span<const V2_float> points, float line_width, [[maybe_unused]] const RenderState& state
) {
	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for lines");
	PTGN_ASSERT(points.size() == line_vertices.size());

	for (std::size_t i = 0; i < points.size(); ++i) {
		auto start = points[i];
		auto end   = points[(i + 1) % points.size()];

		Line l{ start, end };
		auto line_points = l.GetWorldQuadVertices(Transform{}, line_width);

		for (std::size_t j = 0; j < line_vertices.size(); ++j) {
			line_vertices[j].position[0] = line_points[j].x;
			line_vertices[j].position[1] = line_points[j].y;
		}

		AddVertices(line_vertices, line_indices);
	}
}

void RenderData::AddVertices(
	std::span<const Vertex> point_vertices, std::span<const Index> point_indices
) {
	if (vertices_.size() + point_vertices.size() > vertex_capacity ||
		indices_.size() + point_indices.size() > index_capacity) {
		Flush();
	}

	vertices_.insert(vertices_.end(), point_vertices.begin(), point_vertices.end());

	indices_.reserve(indices_.size() + point_indices.size());

	for (auto index : point_indices) {
		indices_.emplace_back(index + index_offset_);
	}

	index_offset_ += static_cast<Index>(point_vertices.size());
}

void RenderData::DrawFullscreenQuad(
	const Shader& shader, const RenderData::DrawTarget& target, bool flip_texture,
	bool clear_frame_buffer, const Color& target_clear_color
) {
	float texture_index{ 1.0f };
	DrawCall(
		shader,
		Vertex::GetQuad(
			target.points, target.tint, target.depth, { texture_index },
			GetDefaultTextureCoordinates(), flip_texture
		),
		quad_indices, { target.texture_id }, target.frame_buffer, clear_frame_buffer,
		target_clear_color, target.blend_mode, target.viewport, target.view_projection
	);
}

void RenderData::DrawCall(
	const Shader& shader, std::span<const Vertex> vertices, std::span<const Index> indices,
	const std::vector<TextureId>& textures, const FrameBuffer* frame_buffer,
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

		intermediate_target = draw_context_pool.Get(target.viewport.size, target.texture_format);

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
			const auto& texture{ intermediate_target->frame_buffer.GetTexture() };
			target.texture_id	  = texture.GetId();
			target.texture_format = texture.GetFormat();
			target.texture_size	  = texture.GetSize();
		}

		// Flush intermediate target onto drawing_to frame buffer.
		DrawFullscreenQuad(
			GetFullscreenShader(target.texture_format), target,
			has_post_fx /* Only flip if postfx have been applied. */, false, color::Transparent
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

RenderData::DrawTarget RenderData::GetDrawTarget(
	const RenderTarget& render_target, const Matrix4& view_projection,
	const std::array<V2_float, 4>& points, bool use_viewport
) {
	DrawTarget target;

	const auto& texture{ render_target.GetTexture() };
	auto texture_size{ render_target.GetTextureSize() };

	target.texture_size		 = texture_size;
	target.texture_id		 = texture.GetId();
	target.texture_format	 = texture.GetFormat();
	target.viewport.position = {};
	target.viewport.size	 = texture_size;

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
		 scene.InternalEntitiesWith<Visible, IDrawable, FrameBuffer, DisplayList>()) {
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

void RenderData::RecomputeDisplaySize(const V2_int& window_size) {
	if (!game_size_.BothAboveZero()) {
		UpdateResolutions(window_size, resolution_mode_);
	}

	Viewport vp;
	vp.position = { 0, 0 };
	vp.size		= window_size;

	auto compute_aspect_fit = [&](bool letterbox_mode) {
		float window_aspect{ static_cast<float>(window_size.x) /
							 static_cast<float>(window_size.y) };
		float game_aspect{ static_cast<float>(game_size_.x) / static_cast<float>(game_size_.y) };

		// In letterbox mode we need require window_aspect > game_aspect to fit height, and in
		// overscan we require window_aspect > game_aspect to fit height.
		bool fit_height{ (window_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			vp.size.y = window_size.y;
			vp.size.x = static_cast<int>(static_cast<float>(window_size.y) * game_aspect + 0.5f);
			vp.position.x = (window_size.x - vp.size.x) / 2; // left edge.
			vp.position.y = 0;
		} else {
			// Fit width.
			vp.size.x = window_size.x;
			vp.size.y = static_cast<int>(static_cast<float>(window_size.x) / game_aspect + 0.5f);
			vp.position.x = 0;
			vp.position.y = (window_size.y - vp.size.y) / 2; // top edge.
		}
	};

	switch (resolution_mode_) {
		case ScalingMode::Letterbox:	compute_aspect_fit(true); break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ window_size / game_size_ };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			vp.size		= game_size_ * scale;		   // scale up.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;
		}

		case ScalingMode::Stretch:
			// Viewport is full window (default).
			break;

		case ScalingMode::Disabled:
			vp.size		= game_size_;				   // no change.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;

		case ScalingMode::Overscan: compute_aspect_fit(false); break;

		default:					PTGN_ERROR("Unsupported resolution mode")
	}

	if (vp != display_viewport_) {
		// Only update viewport if it changed. This reduces DisplaySizeChanged event
		// dispatch.
		display_viewport_	  = vp;
		display_size_changed_ = true;
	}
}

void RenderData::UpdateResolutions(const V2_int& game_size, ScalingMode scaling_mode) {
	bool new_game_size{ game_size_ != game_size };
	if (!new_game_size && resolution_mode_ == scaling_mode) {
		return;
	}
	auto window_size{ game.window.GetSize() };
	game_size_		   = game_size;
	resolution_mode_   = scaling_mode;
	game_size_changed_ = new_game_size;
	RecomputeDisplaySize(window_size);
}

void RenderData::ClearScreenTarget() const {
	screen_target_.Clear();
}

void RenderData::ClearRenderTargets(Scene& scene) const {
	scene.render_target_.Clear();

	for (auto [entity, frame_buffer] : scene.EntitiesWith<FrameBuffer>()) {
		RenderTarget rt{ entity };
		rt.Clear();
		// rt.ClearDisplayList();
	}
}

const Shader& RenderData::GetFullscreenShader(TextureFormat texture_format) {
	const Shader* shader{ nullptr };

	if (texture_format == TextureFormat::HDR_RGBA || texture_format == TextureFormat::HDR_RGB) {
		shader = &game.shader.Get("tone_mapping");
		PTGN_ASSERT(shader != nullptr);
		shader->Bind();
		shader->SetUniform("u_Texture", 1);
		// TODO: Add a way to adjust these.
		shader->SetUniform("u_Exposure", 1.0f);
		shader->SetUniform("u_Gamma", 2.2f);
	} else {
		shader = &game.shader.Get("screen_default");
	}

	return *shader;
}

void RenderData::DrawFromTo(
	const RenderTarget& source_target, const std::array<V2_float, 4>& points,
	const Matrix4& projection, const Viewport& viewport, const FrameBuffer* destination_buffer,
	bool flip_texture
) {
	auto target{ GetDrawTarget(source_target, {}, {}, false) };
	target.view_projection = projection;
	target.points		   = points;
	target.viewport		   = viewport;
	target.frame_buffer	   = destination_buffer;

	DrawFullscreenQuad(
		GetFullscreenShader(target.texture_format), target, flip_texture, false, color::Transparent
	);
}

void RenderData::SetPointsAndProjection(RenderData::DrawTarget& target) {
	PTGN_ASSERT(target.viewport.size.BothAboveZero());

	auto half_viewport{ target.viewport.size * 0.5f };

	target.points = { target.viewport.position - half_viewport,
					  target.viewport.position + V2_float{ half_viewport.x, -half_viewport.y },
					  target.viewport.position + half_viewport,
					  target.viewport.position + V2_float{ -half_viewport.x, half_viewport.y } };

	target.view_projection = Matrix4::Orthographic(target.points[0], target.points[2]);
}

void RenderData::DrawScreenTarget() {
	auto half_viewport{ display_viewport_.size * 0.5f };

	auto projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	std::array<V2_float, 4> points{ -half_viewport, V2_float{ half_viewport.x, -half_viewport.y },
									half_viewport, V2_float{ -half_viewport.x, half_viewport.y } };

	DrawFromTo(screen_target_, points, projection, display_viewport_, nullptr, true);
}

void RenderData::Draw(Scene& scene) {
	// PTGN_LOG(draw_context_pool.contexts_.size());
	//  PTGN_PROFILE_FUNCTION();

	white_texture.Bind(0);

	DrawScene(scene);

	auto half_game_size{ game_size_ * 0.5f };

	auto projection{ Matrix4::Orthographic(-half_game_size, half_game_size) };

	Transform scene_transform{ GetTransform(scene.render_target_) };

	auto points{ Rect{ scene.camera.GetViewportSize() }.GetWorldVertices(scene_transform) };

	Viewport viewport;
	viewport.position = {};
	viewport.size	  = display_viewport_.size;

	DrawFromTo(
		scene.render_target_, points, projection, viewport, &screen_target_.GetFrameBuffer(), true
	);

	Reset();

	temporary_textures = std::vector<Texture>{};
	render_state	   = {};
}

} // namespace impl

} // namespace ptgn