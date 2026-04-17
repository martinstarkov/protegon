#include "runtime/graphics/custom_shader.h"

#include <functional>
#include <optional>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

CustomShader::CustomShader(Entity entity) : Entity{ entity } {}

void CustomShader::Draw(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT((entity.Has<Rect, impl::ShaderData>()));

	const auto& [shader, setup] = entity.Get<impl::ShaderData>();

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& rect{ entity.Get<Rect>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto positions{ rect.GetWorldVertices(draw_transform, draw_origin) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.SetBlendMode(blend_mode);

	constexpr bool floor_positions{ true };

	if (entity.Has<Texture>()) {
		auto texture{ entity.Get<Texture>() };
		auto tex_coords{ GetTextureCoordinates(entity, false) };
		renderer.DrawTexture(
			shader, texture, positions, tint, depth, tex_coords, setup, floor_positions
		);
	} else {
		renderer.DrawQuad(shader, positions, {}, tint, depth, setup, floor_positions);
	}
}

void SetShaderSetup(CustomShader entity, const std::function<void(Entity, Shader)>& shader_setup) {
	PTGN_ASSERT(entity.Has<impl::ShaderData>(), "Shader entity must have shader data component");
	auto& shader_data{ entity.Get<impl::ShaderData>() };
	if (shader_setup) {
		shader_data.shader_setup = [shader_setup, s = shader_data.shader, entity]() mutable {
			shader_setup(entity, s);
		};
	} else {
		shader_data.shader_setup = {};
	}
}

CustomShader CreateCustomShader(
	Scene& scene, ShaderOrKey shader, std::optional<TextureOrKey> texture, V2_float position,
	V2_float size, const std::function<void(Entity, Shader)>& shader_setup, Origin draw_origin
) {
	CustomShader custom_shader{ scene.CreateEntity() };

	const auto& assets{ scene.ctx().asset };

	auto resolved_shader{ shader.Get(assets) };

	if (texture.has_value()) {
		auto resolved_texture{ texture->Get(assets) };
		custom_shader.Add<Texture>(resolved_texture);
	}

	auto& shader_data{ custom_shader.Add<impl::ShaderData>() };
	shader_data.shader = resolved_shader;

	SetShaderSetup(custom_shader, shader_setup);

	SetDraw<CustomShader>(custom_shader);

	Show(custom_shader, false);

	custom_shader.Add<Rect>(size);

	SetPosition(custom_shader, position);

	SetDrawOrigin(custom_shader, draw_origin);

	return custom_shader;
}

} // namespace ptgn