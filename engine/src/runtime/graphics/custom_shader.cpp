#include "runtime/graphics/custom_shader.h"

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

CustomShader::CustomShader(Entity entity) : Entity{ entity } {}

void CustomShader::Draw(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT((entity.Has<Rect, MaterialState>()));

	const auto& material{ entity.Get<MaterialState>() };

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& rect{ entity.Get<Rect>() };
	auto size{ rect.GetSize() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	auto entity_id{ entity.GetUUID() };
	auto tex_coords{ GetTextureCoordinates(entity, false) };

	auto effects{ impl::GetEffectParams(entity) };

	ctx.WithBlendMode(blend_mode, [&]() {
		if (entity.Has<Texture>()) {
			auto texture{ entity.Get<Texture>() };
			ctx.DrawTexture(
				material, texture, draw_transform, depth, size, draw_origin, tint, tex_coords,
				effects, entity_id
			);
		} else {
			ctx.DrawShader(
				material, draw_transform, depth, size, draw_origin, tint, tex_coords, effects,
				entity_id
			);
		}
	});
}

void SetMaterialUpdate(Entity entity, const std::function<void(Entity)>& update) {
	entity.Add<impl::MaterialUpdate>(update);
}

void SetMaterialUniforms(Entity entity, const std::vector<UniformWrite>& material_uniforms) {
	PTGN_ASSERT(entity.Has<MaterialState>(), "Shader entity must have a material component");
	entity.Get<MaterialState>().uniforms = material_uniforms;
}

void SetMaterial(Entity entity, const MaterialState& material) {
	PTGN_ASSERT(entity.Has<MaterialState>(), "Shader entity must have a material component");
	entity.Get<MaterialState>() = material;
}

CustomShader CreateCustomShader(
	Scene& scene, std::string_view shader_key, std::optional<std::string_view> texture_key,
	V2_float position, V2_float size, const std::vector<UniformWrite>& uniforms, Origin draw_origin
) {
	CustomShader custom_shader{ scene.CreateEntity() };

	const auto& assets{ scene.ctx().asset };

	auto shader{ assets.Get<Shader>(shader_key) };

	if (texture_key.has_value()) {
		auto texture{ assets.Get<Texture>(*texture_key) };
		custom_shader.Add<Texture>(texture);
	}

	custom_shader.Add<MaterialState>(shader, uniforms);

	SetDraw<CustomShader>(custom_shader);

	Show(custom_shader, false);

	custom_shader.Add<Rect>(size);

	SetPosition(custom_shader, position);

	SetDrawOrigin(custom_shader, draw_origin);

	return custom_shader;
}

} // namespace ptgn