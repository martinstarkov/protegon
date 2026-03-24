#include "runtime/graphics/shader_component.h"

#include <functional>
#include <optional>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

ShaderEntity::ShaderEntity(Entity entity) : Entity{ entity } {}

void ShaderEntity::Draw(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT((entity.Has<Rect, impl::ShaderData>()));

	const auto& [shader, setup] = entity.Get<impl::ShaderData>();

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& rect{ entity.Get<Rect>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto tint{ GetTint(entity) };
	auto depth{ GetDepth(entity) };
	auto positions{ rect.GetWorldVertices(draw_transform, draw_origin) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.SetBlend(blend_mode);

	if (entity.Has<Texture>()) {
		auto texture{ entity.Get<Texture>() };
		auto tex_coords{ GetTextureCoordinates(entity, false) };
		renderer.DrawTexture(shader, texture, positions, tint, depth, tex_coords, setup);
	} else {
		renderer.DrawQuad(shader, positions, {}, tint, depth, setup);
	}
}

void SetShaderSetup(ShaderEntity entity, const std::function<void(Entity, Shader)>& shader_setup) {
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

ShaderEntity CreateShaderEntity(
	Scene& scene, ShaderOrKey shader, std::optional<TextureOrKey> texture, V2_float position,
	V2_float size, const std::function<void(Entity, Shader)>& shader_setup, Origin draw_origin
) {
	ShaderEntity shader_entity{ scene.CreateEntity() };

	const auto& assets{ scene.ctx().asset };

	auto resolved_shader{ shader.Get(assets) };

	if (texture.has_value()) {
		auto resolved_texture{ texture->Get(assets) };
		shader_entity.Add<Texture>(resolved_texture);
	}

	auto& shader_data{ shader_entity.Add<impl::ShaderData>() };
	shader_data.shader = resolved_shader;

	SetShaderSetup(shader_entity, shader_setup);

	SetDraw<ShaderEntity>(shader_entity);

	Show(shader_entity, false);

	shader_entity.Add<Rect>(size);

	SetPosition(shader_entity, position);

	SetDrawOrigin(shader_entity, draw_origin);

	return shader_entity;
}

} // namespace ptgn