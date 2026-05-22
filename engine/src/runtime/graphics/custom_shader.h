#pragma once

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include "core/math/geometry/origin.h"
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
};

namespace impl {

/// @brief Optional component that can be added to an effect entity to specify a custom update
/// function for the effect.
struct MaterialUpdate {
	/// @brief Called once per draw for the entity with this component.
	std::function<void(Entity)> update;
};

} // namespace impl

void SetMaterialUpdate(Entity entity, const std::function<void(Entity)>& update);

void SetMaterialUniforms(Entity entity, const std::vector<UniformWrite>& material_uniforms);

void SetMaterial(Entity entity, const MaterialState& material);

CustomShader CreateCustomShader(
	Scene& scene, std::string_view shader_key, std::optional<std::string_view> texture_key,
	V2_float position, V2_float size, const std::vector<UniformWrite>& uniforms = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(CustomShader);

} // namespace ptgn