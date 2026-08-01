#include "runtime/graphics/custom_shader.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace impl {

MaterialState GetMaterialState(Entity entity) {
	if (entity.Has<MaterialState>()) {
		return entity.Get<MaterialState>();
	}
	if (entity.Has<Material>()) {
		const auto& material{ entity.Get<Material>() };
		auto& scene{ entity.GetScene() };
		if (auto& assets{ scene.ctx().asset };
			impl::AssetAccessor{ assets }.Has<Shader>(material.shader)) {
			return MaterialState{
				.shader				   = impl::AssetAccessor{ assets }.Get<Shader>(material.shader),
				.uniforms			   = material.uniforms,
				.texture_slot_capacity = material.texture_slot_capacity,
			};
		}
	}
	if (entity.Has<ShaderId>()) {
		return MaterialState{ .shader = entity.Get<ShaderId>() };
	}
	if (entity.Has<Shader>()) {
		return MaterialState{ .shader = entity.Get<Shader>() };
	}
	return {};
}

} // namespace impl

CustomShader::CustomShader(Entity entity) : Entity{ entity } {}

void CustomShader::Draw(DrawContext& ctx, Entity entity) {
	auto material{ impl::GetMaterialState(entity) };

	if (!material) {
		PTGN_WARN("Custom shader cannot be drawn without material");
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };

	Rect rect{ entity.GetOrDefault<Rect>() };

	auto size{ rect.GetSize() };

	auto blend_mode{ GetBlendMode(entity) };

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	ctx.SetBlendMode(blend_mode);

	if (auto texture{ impl::GetTexture(entity) }) {
		ctx.DrawTexture(draw_transform, texture, material, params);
	} else {
		ctx.DrawShader(draw_transform, material, params);
	}
}

CustomShader& CustomShader::SetMaterialUpdate(const std::function<void(CustomShader)>& update) {
	Add<impl::MaterialUpdate>(update);
	return *this;
}

CustomShader& CustomShader::SetMaterialUniforms(
	const std::vector<UniformWrite>& material_uniforms
) {
	TryAdd<Material>().uniforms = material_uniforms;
	return *this;
}

CustomShader& CustomShader::SetMaterialUniform(
	std::string_view name, const UniformValue& value
) {
	auto& uniforms{ TryAdd<Material>().uniforms }; 

	auto it{ std::ranges::find_if(
		uniforms,
		[name](const UniformWrite& uniform) {
			return uniform.name == name;
		}
	) };

	if (it != uniforms.end()) {
		it->value = value;
	} else {
		uniforms.emplace_back(std::string{ name }, value);
	}

	return *this;
}

CustomShader& CustomShader::SetMaterial(Material material) {
	Add<Material>(std::move(material));
	return *this;
}

CustomShader CreateCustomShader(
	Scene& scene, Transform transform, ShaderKey shader_key, TextureKey texture_key, V2_float size,
	const std::vector<UniformWrite>& uniforms, Origin origin
) {
	CustomShader custom_shader{ scene.CreateEntity() };

	custom_shader.Add<Tag>("Custom Shader");
	custom_shader.Add<TextureKey>(std::move(texture_key));
	custom_shader.Add<Material>(Material{ .shader = std::move(shader_key), .uniforms = uniforms, });
	custom_shader.Add<Visible>(true);
	custom_shader.Add<Rect>(size);
	custom_shader.Add<Transform>(transform);
	custom_shader.Add<Origin>(origin);

	SetDraw<CustomShader>(custom_shader);

	return custom_shader;
}

} // namespace ptgn