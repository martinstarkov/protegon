#pragma once

#include <array>
#include <map>
#include <vector>

#include "components/transform.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/batch.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/origin.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"

namespace ptgn {

// TODO: Figure out what to do with this.
struct Point {};

namespace impl {

class RenderData {
public:
	void Init();

	void Render(const FrameBuffer& frame_buffer, const Camera& camera, ecs::Manager& manager);
	void Render(
		const FrameBuffer& frame_buffer, const Camera& camera, const ecs::Entity& o,
		bool check_visibility
	);
	void RenderToScreen(const RenderTarget& target, const Camera& camera);

	void AddLine(
		const V2_float& line_start, const V2_float& line_end, float line_width, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddLines(
		const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
		BlendMode blend_mode, const V4_float& color, bool connect_last_to_first
	);

	void AddTexture(
		const ecs::Entity& e, const Texture& texture, const V2_float& position,
		const V2_float& size, Origin origin, const Depth& depth, BlendMode blend_mode,
		const V4_float& color, float rotation, bool flip_vertically = false
	);

	void AddTriangle(
		const std::array<V2_float, 3>& vertices, float line_width, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddQuad(
		const V2_float& position, const V2_float& size, Origin origin, float line_width,
		const Depth& depth, BlendMode blend_mode, const V4_float& color, float rotation
	);

	void AddEllipse(
		const V2_float& center, const V2_float& radius, float line_width, const Depth& depth,
		BlendMode blend_mode, const V4_float& color, float rotation
	);

	void AddPolygon(
		const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddPoint(
		const V2_float position, const Depth& depth, BlendMode blend_mode, const V4_float& color
	);

	void AddPointLight(const ecs::Entity& o, const Depth& depth);

	void AddText(
		const ecs::Entity& o, const Text& text, const V2_float& position, const V2_float& size,
		Origin origin, const Depth& depth, BlendMode blend_mode, const V4_float& color,
		float rotation
	);

	void AddRenderTarget(
		const ecs::Entity& o, const RenderTarget& rt, const Depth& depth, BlendMode blend_mode,
		const V4_float& tint
	);

	void AddButton(
		const Text* text, const Texture* texture, const V4_float& background_color,
		float background_line_width, const V4_float& border_color, float border_line_width,
		const V2_float& position, const V2_float& size, Origin origin, const Depth& depth,
		BlendMode blend_mode, const V4_float& tint, float rotation
	);

private:
	void AddFilledTriangle(
		const std::array<V2_float, Batch::triangle_vertex_count>& vertices, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddTexturedQuad(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices,
		const std::array<V2_float, Batch::quad_vertex_count>& tex_coords, const Texture& texture,
		const Depth& depth, BlendMode blend_mode, const V4_float& color
	);

	void AddFilledQuad(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddFilledEllipse(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
		BlendMode blend_mode, const V4_float& color
	);

	void AddHollowEllipse(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, float line_width,
		const V2_float& radius, const Depth& depth, BlendMode blend_mode, const V4_float& color
	);

	[[nodiscard]] V2_float GetTextureSize(const ecs::Entity& o, const Texture& texture);

	[[nodiscard]] Batch& GetBatch(
		std::size_t vertex_count, std::size_t index_count, const Texture& texture,
		const Shader& shader, BlendMode blend_mode, const Depth& depth
	);

	[[nodiscard]] float GetTextureIndex(Batch& batch, const Texture& texture);

	void AddToBatch(const ecs::Entity& object, bool check_visibility);

	void SetVertexArrayToWindow(
		const Camera& camera, const Color& color, const Depth& depth, float texture_index
	);

	void SetupRender(const FrameBuffer& frame_buffer, const Camera& camera);
	void FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera);

	// Set once before adding to batch.
	std::array<V2_float, Batch::quad_vertex_count> camera_vertices;

	constexpr static float min_line_width{ 1.0f };

	std::size_t max_texture_slots{ 0 };

	Texture white_texture;

	RenderTarget lights;

	BlendMode light_blend_mode{ BlendMode::Add };

	VertexArray triangle_vao;

	std::map<Depth, Batches> batch_map;
};

} // namespace impl

} // namespace ptgn