#pragma once

#include <map>

#include "components/draw.h"
#include "components/transform.h"
#include "ecs/ecs.h"
#include "renderer/batch.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
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
		const FrameBuffer& frame_buffer, const Camera& camera, ecs::Entity e, bool check_visibility
	);
	void RenderToScreen(const RenderTarget& target, const Camera& camera);

private:
	void AddToBatch(
		Batch& batch, ecs::Entity e, Transform transform, const Depth& depth,
		const Texture& texture, const Camera& camera
	);

	void AddTexture(
		ecs::Entity e, const Transform& transform, const Depth& depth, const BlendMode& blend_mode,
		const Texture& texture, const Shader& shader, const Camera& camera
	);

	void SetVertexArrayToWindow(
		const Camera& camera, const Color& color, const Depth& depth, float texture_index
	);

	void SetupRender(const FrameBuffer& frame_buffer, const Camera& camera) const;
	void PopulateBatches(ecs::Entity entity, bool check_visibility, const Camera& camera);
	void FlushBatches(const FrameBuffer& frame_buffer, const Camera& camera);

	std::size_t max_texture_slots{ 0 };

	Texture white_texture;

	RenderTarget lights;

	BlendMode default_blend_mode{ BlendMode::Blend };
	BlendMode light_blend_mode{ BlendMode::Add };

	VertexArray triangle_vao;

	std::map<Depth, Batches> batch_map;
};

} // namespace impl

} // namespace ptgn