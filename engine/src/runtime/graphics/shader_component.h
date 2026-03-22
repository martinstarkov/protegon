#pragma once

#include <functional>
#include <optional>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/shader.h"
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

class ShaderEntity : public Entity {
public:
	ShaderEntity() = default;
	explicit ShaderEntity(Entity entity);

	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

void SetShaderSetup(ShaderEntity entity, const std::function<void(Entity, Shader)>& shader_setup);

ShaderEntity CreateShaderEntity(
	Scene& scene, ShaderOrKey shader, std::optional<TextureOrKey> texture, V2_float position,
	V2_float size, const std::function<void(Entity, Shader)>& shader_setup = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(ShaderEntity);

} // namespace ptgn