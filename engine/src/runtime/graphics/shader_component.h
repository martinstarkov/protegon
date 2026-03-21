#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <variant>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
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
	Scene& scene, std::variant<Shader, std::string_view> shader,
	const std::optional<std::variant<Texture, std::string_view>>& texture, V2_float position,
	V2_float size, const std::function<void(Entity, Shader)>& shader_setup = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(ShaderEntity);

} // namespace ptgn