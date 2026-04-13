#pragma once

#include <functional>
#include <optional>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;
class Scene;

namespace impl {

struct ShaderData {
	Shader shader;
	std::function<void()> shader_setup;
};

}; // namespace impl

class CustomShader : public Entity {
public:
	CustomShader() = default;
	explicit CustomShader(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

void SetShaderSetup(CustomShader entity, const std::function<void(Entity, Shader)>& shader_setup);

CustomShader CreateCustomShader(
	Scene& scene, ShaderOrKey shader, std::optional<TextureOrKey> texture, V2_float position,
	V2_float size, const std::function<void(Entity, Shader)>& shader_setup = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(CustomShader);

} // namespace ptgn