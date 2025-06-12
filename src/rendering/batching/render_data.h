#pragma once

#include <array>
#include <map>
#include <vector>

#include "components/common.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/batch.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/buffers/vertex_array.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"

// TODO: Move to RenderTarget's owning their own batches.

namespace ptgn {

class Shader;
class Camera;
class RenderTarget;

namespace impl {

class Batch;

class RenderData {
public:
	void Init();

	void Render(const FrameBuffer& frame_buffer, const Manager& manager);

	// void RenderToScreen(const RenderTarget& target, const Camera& camera);

	void AddLine(
		const V2_float& line_start, const V2_float& line_end, float line_width, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddLines(
		const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color,
		bool connect_last_to_first, bool debug
	);

	void AddTriangle(
		const std::array<V2_float, 3>& vertices, float line_width, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddQuad(
		const V2_float& position, const V2_float& size, Origin origin, float line_width,
		const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color,
		float rotation, bool debug
	);

	void AddEllipse(
		const V2_float& center, const V2_float& radius, float line_width, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, float rotation,
		bool debug
	);

	void AddPolygon(
		const std::vector<V2_float>& vertices, float line_width, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddPoint(
		const V2_float& position, const Depth& depth, const Camera& camera, BlendMode blend_mode,
		const V4_float& color, bool debug
	);

	void AddTexturedQuad(
		const Transform& transform, const V2_float& size, Origin origin,
		const std::array<V2_float, Batch::quad_vertex_count>& tex_coords, const Texture& texture,
		const Depth& depth, const Camera& camera, BlendMode blend_mode, const V4_float& color,
		bool debug
	);

	// Set once before adding to batch.
	std::array<V2_float, Batch::quad_vertex_count> camera_vertices;

	bool pixel_rounding{ false };

	constexpr static float min_line_width{ 1.0f };

	std::size_t max_texture_slots{ 0 };

	Texture white_texture;
	Camera fallback_camera;
	Camera active_camera;

	Manager light_manager;
	RenderTarget light_target;

	BlendMode light_blend_mode{ BlendMode::Add };

	VertexArray triangle_vao;

	std::map<Depth, Batches> batch_map;

	Batches debug_batches;

	bool sort_entity_drawing_by_y{ true };

private:
	friend class Batch;
	friend class ptgn::RenderTarget;

	void AddFilledTriangle(
		const std::array<V2_float, Batch::triangle_vertex_count>& vertices, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddFilledQuad(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddFilledEllipse(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, const Depth& depth,
		const Camera& camera, BlendMode blend_mode, const V4_float& color, bool debug
	);

	void AddHollowEllipse(
		const std::array<V2_float, Batch::quad_vertex_count>& vertices, float line_width,
		const V2_float& radius, const Depth& depth, const Camera& camera, BlendMode blend_mode,
		const V4_float& color, bool debug
	);

	[[nodiscard]] Batch& GetBatch(
		std::size_t vertex_count, std::size_t index_count, const Texture& texture,
		const Shader& shader, const Camera& camera, BlendMode blend_mode, const Depth& depth,
		bool debug
	);

	[[nodiscard]] float GetTextureIndex(Batch& batch, const Texture& texture);

	void AddToBatch(const Entity& object, bool check_visibility);

	void SetVertexArrayToWindow(const Color& color, const Depth& depth, float texture_index);

	void SortEntitiesByY(std::vector<Entity>& entities);

	void UseCamera(const Camera& camera);

	// @return True if the batch contains lights, false otherwise. Note: Light batches do not
	// contain other drawables.
	bool FlushLights(
		Batch& batch, const FrameBuffer& frame_buffer, const V2_float& window_size,
		const Depth& depth
	);

	void Flush(const FrameBuffer& frame_buffer);

	void FlushBatches(
		Batches& batches, const FrameBuffer& frame_buffer, const V2_float& window_size,
		const Depth& depth
	);

	void FlushBatch(Batch& batch, const FrameBuffer& frame_buffer);

	void FlushDebugBatches(const FrameBuffer& frame_buffer);
};

} // namespace impl

} // namespace ptgn