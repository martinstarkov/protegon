#pragma once

#include <array>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"

namespace ptgn {

class Application;

namespace impl {

class Renderer;

struct TriangleCommand {
	std::vector<RenderTriangle<ColorVertex>> triangles;
};

struct QuadCommand {
	std::vector<RenderQuad<ColorVertex>> quads;
};

struct ShapeCommand {
	ShaderId shader;
	std::vector<RenderQuad<ShapeVertex>> shapes;
};

struct TextureCommand {
	MaterialState material;
	std::vector<RenderQuad<TextureVertex>> quads;
	std::vector<TextureId> textures;
};

using ManualCommand = std::variant<TriangleCommand, QuadCommand, ShapeCommand, TextureCommand>;

} // namespace impl

class DrawContext {
public:
	void Flush();

	[[nodiscard]] V2_int BoundTargetSize() const;
	[[nodiscard]] impl::TextureId BoundTarget() const;

	[[nodiscard]] RenderPassBuilder Pass();

	void SetShader(std::string_view shader);
	void SetShader(impl::ShaderId shader);
	void SetBlendMode(BlendMode mode);

	void DrawTexture(
		impl::TextureId texture, const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const impl::EffectParams& effects = {},
		std::span<const impl::TextureBinding> extra_textures = {}, int entity_id = -1
	);

	void Draw(const impl::ManualCommand& cmd);

	impl::ShaderId GetShader(std::string_view name) const;

private:
	friend class impl::Renderer;
	friend class Application;

	explicit DrawContext(impl::Renderer& renderer);

	impl::Renderer& renderer_;
};

} // namespace ptgn