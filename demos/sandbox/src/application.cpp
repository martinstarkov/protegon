#include <array>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "core/entity.h"
#include "core/game.h"
#include "debug/log.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/batching/vertex.h"
#include "rendering/buffers/buffer.h"
#include "rendering/buffers/buffer_layout.h"
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

using namespace ptgn;

constexpr V2_int window_size{ 1000, 1000 }; //{ 1280, 720 };

namespace ptgn {

namespace impl {

using Index = std::uint32_t;

template <bool have_render_targets = false>
void SortEntities(std::vector<Entity>& entities) {
	std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
		auto depthA{ a.GetDepth() };
		auto depthB{ b.GetDepth() };

		if constexpr (!have_render_targets) {
			return depthA < depthB;
		}

		if (depthA != depthB) {
			return depthA < depthB; // Smaller depth first
		}

		PTGN_ASSERT(a.Has<RenderTarget>());
		PTGN_ASSERT(b.Has<RenderTarget>());

		// If depths are equal, compare framebuffer IDs
		auto idA{ a.Get<RenderTarget>().GetFrameBuffer().GetId() };
		auto idB{ b.Get<RenderTarget>().GetFrameBuffer().GetId() };
		return idA < idB;
	});
}

/*

Things that trigger Batch flush:

Frame Buffer / Render Target Bind
Shader Bind
Blend Mode Change
Uniform Change
Viewport Change
Camera Change -> Uniform Change
view_projection_dirty
shader_dirty (uniform has changed so flush previous batch)

TODO: Think of what a typical Quad batch looks like, then consider what happens when a Circle batch
is added. Then consider what happens if a custom render target shader is used such as with lighting.

white_texture.Bind();
frame_buffer.Bind();

auto new_vertex_count{ vertices.size() + X };
auto new_index_count{ indices.size() + Y };
auto new_texture_count{ textures.size() + Z };
auto chosen_camera{ camera ? camera : fallback_camera -> scene camera or render target camera };

if (bound_shader != quad_shader || bound_camera != chosen_camera || new_texture_count >
texture_capacity || bound_blend_mode != blend_mode || new_vertex_count > vertex_capacity ||
new_index_count > index_capacity) { Flush(); bound_shader = quad_shader; bound_blend_mode =
blend_mode; bound_camera = chosen_camera;
}
vertices.Add(X);
indices.Add(Y);
textures.Add(Z);

ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawQuad();
ctx.DrawCircle();
ctx.DrawCircle();
ctx.DrawCircle();
ctx.DrawLight();
ctx.DrawLight();
ctx.DrawLight();


// If entity has a blend mode:
SetBlendMode(GetBlendMode());

// If entity is a specific type.
quad_shader.Bind();

// If entity has a new camera.
if (camera_dirty) {
	quad_shader.Bind();
	quad_shader.SetUniform("u_ViewProjection", camera);
	SetViewport(camera.GetViewport());
}



DrawCustomShader(first = [](){
	shader.SetUniform(u_Texture, 1);
	shader.SetUniform(u_Resolution);
}, every = [](){
	shader.SetUniform(light.GetRadius());
	shader.SetUniform(light.GetOtherThing());
});

Flush(flush_to_target) {
if (render_target != {}) {
	flush_to_target.Bind();
	SetBlendMode();
	quad_shader.Bind();
	quad_shader.SetUniform(render_target.GetCamera());
	SetViewport(render_target.GetCamera());
	render_target.GetTexture().Bind(1);
	draw();
	return;
}
flush_to_target.Bind();
set blend mode
set shader
set uniforms (camera);
set viewport
set textures
set vbos
draw();
}

DrawCustomShader(first, every) {

if (not shader) {
	Flush();
	Bind And Set(render_target); // Clear all render targets at the start of render cycle.
	shader.Bind();
	first();
	render_target.Bind(1);
	VAO.BindAndSetData(window);
}

every();
draw();

}









// Flush
BindTextures();
VAO.Bind();
VAO.SetSubData(vertices, indices);
GLDraw(VAO);

Hmm: ?

Frame Buffer / Render Target Clear.
Shader Uniform Set.

const Shader* shader_{ nullptr };
BlendMode blend_mode_{ BlendMode::None };
Camera camera_;
std::function<void(const Shader& shader)> uniform_callback_;
bool view_projection_dirty_{ true };

*/

struct Batch {
	std::vector<Vertex> vertices;
	std::vector<Index> indices;
	std::vector<TextureId> textures;
	Index index_offset{ 0 };
};

constexpr std::array<V2_float, 4> default_texture_coordinates{
	V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 0.0f, 1.0f }
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

constexpr std::size_t batch_capacity{ 4000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

class RenderState {
public:
	RenderState() = default;

	RenderState(
		const RenderTarget& render_target, const Shader* shader, BlendMode blend_mode,
		const Camera& camera
	) :
		render_target_{ render_target },
		shader_{ shader },
		blend_mode_{ blend_mode },
		camera_{ camera } {}

	friend bool operator==(const RenderState& a, const RenderState& b) {
		return a.shader_ == b.shader_ && a.camera_ == b.camera_ &&
			   a.render_target_ == b.render_target_ && a.blend_mode_ == b.blend_mode_;
	}

	friend bool operator!=(const RenderState& a, const RenderState& b) {
		return !(a == b);
	}

	RenderTarget render_target_;
	const Shader* shader_{ nullptr };
	BlendMode blend_mode_{ BlendMode::None };
	Camera camera_;
};

class RenderDataThing {
public:
	void Init() {
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

#ifdef PTGN_PLATFORM_MACOS
		// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
		// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
		// texture because texture unloadable."
		for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
			Texture::Bind(white_texture.GetId(), slot);
		}
#endif

		SetState(RenderState{ {}, &game.shader.Get<ShapeShader::Quad>(), BlendMode::Blend, {} });
	}

	void AddTriangle(const std::array<Vertex, 3>& vertices, const RenderState& state) {
		SetState(state);
		// TODO: Get rid of magic numbers.
		if (batch.vertices.size() + 3 > vertex_capacity ||
			batch.indices.size() + 3 > index_capacity) {
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

	void AddQuad(const std::array<Vertex, 4>& vertices, const RenderState& state) {
		SetState(state);
		// TODO: Get rid of magic numbers.
		if (batch.vertices.size() + 4 > vertex_capacity ||
			batch.indices.size() + 6 > index_capacity) {
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

	template <typename T>
	void AddVertices(const T& vertices, const RenderState& state) {
		if constexpr (std::is_same_v<T, std::array<Vertex, 3>>) {
			AddTriangle(vertices, state);
		} else if constexpr (std::is_same_v<T, std::array<Vertex, 4>>) {
			AddQuad(vertices, state);
		} else if constexpr (std::is_same_v<T, std::vector<Vertex>>) {
			// TODO: Implement specific cases in terms of the other two.
		}
	}

	void SetState(const RenderState& new_render_state) {
		if (new_render_state == render_state) {
			return;
		}
		Flush();
		render_state = new_render_state;
	}

	void Flush() {
		if (batch.vertices.empty() || batch.indices.empty()) {
			return;
		}

		if (render_state.render_target_) {
			render_state.render_target_.Bind();
			// TODO: Clear all render targets before the render draw.
		} else {
			FrameBuffer::Unbind();
		}

		render_state.shader_->Bind();
		const auto& camera_vp{ render_state.camera_.GetViewProjection() };
		render_state.shader_->SetUniform("u_ViewProjection", camera_vp);

		game.renderer.SetBlendMode(render_state.blend_mode_);
		game.renderer.SetViewport(
			render_state.camera_.GetViewportPosition(), render_state.camera_.GetViewportSize()
		);

		for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(batch.textures.size()); i++) {
			// Save first texture slot for empty white texture.
			std::uint32_t slot{ i + 1 };
			Texture::Bind(batch.textures[i], slot);
		}

		triangle_vao.Bind();

		triangle_vao.GetVertexBuffer().SetSubData(
			batch.vertices.data(), 0, static_cast<std::uint32_t>(batch.vertices.size()),
			sizeof(Vertex), false
		);

		triangle_vao.GetIndexBuffer().SetSubData(
			batch.indices.data(), 0, static_cast<std::uint32_t>(batch.indices.size()),
			sizeof(Index), false
		);

		GLRenderer::DrawElements(triangle_vao, batch.indices.size(), false);

		batch.vertices.clear();
		batch.indices.clear();
		batch.textures.clear();
		batch.index_offset = 0;
	}

	void Draw(Scene& scene) {
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
				regular_entities.emplace_back(entity);
			} else {
				// TODO: Add general vertices.
				// regular_entities.emplace_back(entity);
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
	}

	RenderState render_state;
	Batch batch;
	std::size_t max_texture_slots{ 0 };
	Texture white_texture;
	VertexArray triangle_vao;
};

[[nodiscard]] static std::array<Vertex, 4> GetQuadVertices(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth
) {
	std::array<Vertex, 4> vertices{};

	V4_float c{ color.Normalized() };

	PTGN_ASSERT(vertices.size() == default_texture_coordinates.size());

	for (std::size_t i{ 0 }; i < vertices.size(); ++i) {
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { default_texture_coordinates[i].x,
								  default_texture_coordinates[i].y };
		vertices[i].tex_index = { 0.0f };
	}

	return vertices;
}

} // namespace impl

} // namespace ptgn

struct QuadVertices {
	std::array<impl::Vertex, 4> vertices;
};

struct SandboxScene : public Scene {
	std::array<V2_float, 4> points{ V2_float{ 50.0f, 50.0f }, V2_float{ 200.0f, 50.0f },
									V2_float{ 200.0f, 200.0f }, V2_float{ 50.0f, 200.0f } };

	QuadVertices v;

	impl::RenderDataThing renderer;

	Entity e;

	void Enter() override {
		e		   = CreateEntity();
		v.vertices = impl::GetQuadVertices(points, color::Red, Depth{ 1 });
		e.Add<QuadVertices>(v);
		e.Show();

		renderer.Init();
	}

	void Render() {
		renderer.Draw(*this);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("SandboxScene");
	game.scene.Enter<SandboxScene>("");
	return 0;
}