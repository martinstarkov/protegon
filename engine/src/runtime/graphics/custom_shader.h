#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/shader.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;
class Scene;

class CustomShader : public Entity {
public:
	CustomShader() = default;
	explicit CustomShader(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	CustomShader& SetMaterialUpdate(const std::function<void(CustomShader)>& update);

	CustomShader& SetMaterialUniforms(const std::vector<UniformWrite>& material_uniforms);

	CustomShader& SetMaterial(Material material);
};

namespace impl {

MaterialState GetMaterialState(Entity entity);

/// @brief Optional component that can be added to an effect entity to specify a custom update
/// function for the effect.
struct MaterialUpdate {
	/// @brief Called once per draw for the entity with this component.
	std::function<void(CustomShader)> update;
};

} // namespace impl

CustomShader CreateCustomShader(
	Scene& scene, Transform transform = {}, std::string_view shader_key = {},
	std::string_view texture_key = {}, V2_float size = {},
	const std::vector<UniformWrite>& uniforms = {}, Origin origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(CustomShader, { .name = "Custom Shader" });

} // namespace ptgn