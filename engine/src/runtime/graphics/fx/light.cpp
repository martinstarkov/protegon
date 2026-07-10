#include "runtime/graphics/fx/light.h"

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/geometry_utils.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/broadphase.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace {

constexpr int kLightVisibleStencilRef{ 1 };
constexpr int kCircleShadowSegments{ 24 };

struct ShadowCasterEntry {
	Entity entity;
	float depth{ 0.0f };
	std::size_t order{ 0 };
	BoundingAABB aabb;
	bool masks_light_inside{ true };
};

std::optional<std::vector<V2_float>> GetShadowCasterWorldVertices(Entity entity) {
	auto transform{ GetDrawTransform(entity) };
	auto origin{ entity.GetOrDefault<Origin>(kDefaultOrigin) };

	if (entity.Has<Rect>()) {
		auto vertices{ entity.Get<Rect>().GetWorldVertices(transform, origin) };
		return std::vector<V2_float>{ vertices.begin(), vertices.end() };
	}

	if (entity.Has<Circle>()) {
		return entity.Get<Circle>().GetVertices(transform, kCircleShadowSegments);
	}

	if (auto texture_size{ GetTextureSize(entity) }) {
		Rect rect{ texture_size.value() };
		auto vertices{ rect.GetWorldVertices(transform, origin) };
		return std::vector<V2_float>{ vertices.begin(), vertices.end() };
	}

	return std::nullopt;
}

std::optional<BoundingAABB> GetShadowCasterAABB(Entity entity) {
	auto transform{ GetDrawTransform(entity) };

	if (entity.Has<Rect>()) {
		return GetBoundingAABB(entity.Get<Rect>(), transform);
	}

	if (entity.Has<Circle>()) {
		return GetBoundingAABB(entity.Get<Circle>(), transform);
	}

	if (auto texture_size{ GetTextureSize(entity) }) {
		return GetBoundingAABB(Rect{ texture_size.value() }, transform);
	}

	return std::nullopt;
}

void AddPolygonSegments(std::vector<Line>& segments, std::span<const V2_float> vertices) {
	if (vertices.size() < 2) {
		return;
	}

	for (auto i{ 0uz }; i < vertices.size(); ++i) {
		auto next{ (i + 1) % vertices.size() };

		segments.emplace_back(vertices[i], vertices[next]);
	}
}

bool CasterIsBeforeLight(
	const ShadowCasterEntry& caster, Entity light, float light_depth, std::size_t light_order
) {
	if (caster.depth < light_depth) {
		return true;
	}

	if (NearlyEqual(caster.depth, light_depth)) {
		return caster.order < light_order || caster.entity.WasCreatedBefore(light);
	}

	return false;
}

impl::VisibilityPolygon ComputeVisibilityPolygonForLight(
	Entity light, float light_depth, std::size_t light_order,
	std::span<const V2_float> camera_vertices, std::span<const ShadowCasterEntry> casters,
	std::span<const Entity> candidate_entities
) {
	auto light_position{ GetPosition(light) };

	std::unordered_set<Entity> candidate_set;
	candidate_set.reserve(candidate_entities.size());

	for (Entity entity : candidate_entities) {
		candidate_set.emplace(entity);
	}

	std::vector<Line> segments;
	std::vector<impl::ShadowMaskInterior> interiors;

	for (const auto& caster : casters) {
		if (!candidate_set.contains(caster.entity)) {
			continue;
		}

		if (!CasterIsBeforeLight(caster, light, light_depth, light_order)) {
			continue;
		}

		auto world_vertices{ GetShadowCasterWorldVertices(caster.entity) };

		if (!world_vertices.has_value() || world_vertices.value().size() < 3) {
			continue;
		}

		AddPolygonSegments(segments, world_vertices.value());

		interiors.emplace_back(
			impl::ShadowMaskInterior{
				.vertices			= world_vertices.value(),
				.masks_light_inside = caster.masks_light_inside,
			}
		);
	}

	impl::VisibilityPolygon result;

	if (segments.empty()) {
		result.vertices = std::ranges::to<std::vector>(camera_vertices);
		return result;
	}

	AddPolygonSegments(segments, camera_vertices);

	std::vector<V2_float> world_visibility;

	if (segments.empty()) {
		world_visibility = std::ranges::to<std::vector>(camera_vertices);
	} else {
		world_visibility = GetVisibilityPolygon(light_position, segments);
	}

	result.vertices			  = std::move(world_visibility);
	result.occluder_interiors = std::move(interiors);

	return result;
}

void DrawStencilPolygon(
	DrawContext& ctx, Transform draw_transform, std::span<const V2_float> vertices
) {
	if (vertices.size() < 3) {
		return;
	}

	Polygon polygon{ vertices };

	ctx.DrawShape(
		draw_transform.Inverse(), polygon, color::White,
		ShapeDrawParams{
			.depth		= 0.0f,
			.fill_style = Solid{},
			.origin		= Origin::Center,
		}
	);
}

RenderStateDelta ReplaceStencilState(int value) {
	return RenderStateDelta{
		.blending	   = false,
		.depth_testing = false,
		.color_mask =
			ColorMaskState{
				.red   = false,
				.green = false,
				.blue  = false,
				.alpha = false,
			},
		.stencil =
			StencilState{
				.enabled	= true,
				.func		= CompareFunc::Always,
				.ref		= value,
				.mask		= 0xFF,
				.fail_op	= StencilOp::Keep,
				.zfail_op	= StencilOp::Keep,
				.zpass_op	= StencilOp::Replace,
				.write_mask = 0xFF,
			},
	};
}

RenderStateDelta ReadStencilState(int value) {
	return RenderStateDelta{
		.blending	   = false,
		.depth_testing = false,
		.color_mask =
			ColorMaskState{
				.red   = true,
				.green = true,
				.blue  = true,
				.alpha = true,
			},
		.stencil =
			StencilState{
				.enabled	= true,
				.func		= CompareFunc::Equal,
				.ref		= value,
				.mask		= 0xFF,
				.fail_op	= StencilOp::Keep,
				.zfail_op	= StencilOp::Keep,
				.zpass_op	= StencilOp::Keep,
				.write_mask = 0x00,
			},
	};
}

void WriteLightVisibilityStencil(
	DrawContext& ctx, Transform draw_transform, const impl::VisibilityPolygon& visibility_polygon
) {
	ctx.WithRenderState(
		ReplaceStencilState(kLightVisibleStencilRef),
		[&ctx, &visibility_polygon, draw_transform]() {
			auto origin{ draw_transform.position };

			if (auto count{ visibility_polygon.vertices.size() }; count >= 3) {
				for (auto i{ 0uz }; i < count; ++i) {
					V2_float a{ visibility_polygon.vertices[i] };
					V2_float b{ visibility_polygon.vertices[(i + 1) % count] };

					ctx.DrawShape(
						draw_transform.Inverse(), Triangle{ origin, a, b }, color::White,
						ShapeDrawParams{
							.depth		= 0.0f,
							.fill_style = Solid{},
							.origin		= Origin::Center,
						}
					);
				}
			}

			for (const auto& interior : visibility_polygon.occluder_interiors) {
				if (interior.masks_light_inside) {
					continue;
				}

				DrawStencilPolygon(ctx, draw_transform, interior.vertices);
			}
		}
	);

	// Optional, but useful if the visibility polygon includes blocker interiors.
	// These interiors are forced back to stencil 0, so the light will not draw there.
	ctx.WithRenderState(ReplaceStencilState(0), [&ctx, &visibility_polygon, draw_transform]() {
		for (const auto& interior : visibility_polygon.occluder_interiors) {
			if (!interior.masks_light_inside) {
				continue;
			}

			DrawStencilPolygon(ctx, draw_transform, interior.vertices);
		}
	});
}

void DrawLightThroughStencil(
	DrawContext& ctx, Entity entity, V2_float size, std::vector<UniformWrite> uniforms
) {
	Material material{
		.shader	  = "light",
		.uniforms = std::move(uniforms),
	};

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	// This draw happens into the local light target, not into the world scene.
	params.depth   = 0.0f;
	params.origin  = Origin::Center;
	params.effects = {};

	ctx.WithRenderState(ReadStencilState(kLightVisibleStencilRef), [&ctx, &material, &params]() {
		ctx.DrawShader({}, material, std::move(params));
	});
}

void DrawShadowedLight(
	DrawContext& ctx, Entity entity, Transform draw_transform, V2_float size, BlendMode blend_mode,
	const impl::VisibilityPolygon& visibility_polygon, std::vector<UniformWrite> uniforms
) {
	V2_int target_size{ size };

	if (!target_size.IsPositive()) {
		PTGN_WARN("Light shadow target size should be positive");
		return;
	}

	TextureDesc light_desc{
		.size	= target_size,
		.format = TextureFormat::RGBA8,
	};

	TextureDesc shadow_desc{ .size = target_size, .format = TextureFormat::Stencil8 };

	auto composite_params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	Viewport viewport{
		.position{},
		.size{ target_size },
	};

	ctx.WithTemporaryFramebuffer(
		light_desc, shadow_desc,
		[&ctx, viewport, &visibility_polygon, entity, size, &uniforms, blend_mode,
		 draw_transform](impl::FramebufferObject& framebuffer) {
			framebuffer.Clear(color::Transparent, true);
			framebuffer.Clear(Stencil{ 0 }, true);

			ctx.WithRenderTarget(
				&framebuffer, viewport,
				[&ctx, &visibility_polygon, entity, size, &uniforms, draw_transform]() {
					WriteLightVisibilityStencil(ctx, draw_transform, visibility_polygon);
					DrawLightThroughStencil(ctx, entity, size, std::move(uniforms));
				}
			);

			auto texture{ framebuffer.GetTexture() };
			auto composite_params{ impl::GetTextureDrawParams(entity, size, true, color::White) };

			ctx.SetBlendMode(blend_mode);
			ctx.DrawTexture(draw_transform, texture, std::move(composite_params));
		}
	);
}

void DrawUnmaskedLight(
	DrawContext& ctx, Entity entity, Transform draw_transform, V2_float size, BlendMode blend_mode,
	std::vector<UniformWrite> uniforms
) {
	Material material{
		.shader	  = "light",
		.uniforms = std::move(uniforms),
	};

	auto params{ impl::GetTextureDrawParams(entity, size, false, color::White) };

	ctx.SetBlendMode(blend_mode);
	ctx.DrawShader(draw_transform, material, std::move(params));
}

std::array<UniformWrite, 9> GetUniforms(const LightConfig& light, Color tint) {
	auto color{ Color::Multiply(light.color, tint) };
	V4_float color_n{ color.Normalized() };

	auto ambient_light_n{ light.ambient_color.Normalized() };
	V3_float ambient_color{ ambient_light_n.xyz() };
	constexpr V3_float light_attenuation{ 1.0f, 0.0f, 0.1f };

	return { UniformWrite{ "u_LightIntensity", light.intensity },
			 UniformWrite{ "u_LightRadius", 0.5f },
			 UniformWrite{ "u_Falloff", light.falloff },
			 UniformWrite{ "u_UseCone", light.cone_angle.has_value() ? 1.0f : 0.0f },
			 UniformWrite{ "u_ConeAngle", light.cone_angle.has_value()
									? (light.cone_angle.value() / 2.0f).ToRad().value
									: kTwoPi },
			 UniformWrite{ "u_Color", color_n },
			 UniformWrite{ "u_AmbientColor", ambient_color },
			 UniformWrite{ "u_AmbientIntensity", light.ambient_intensity },
			 UniformWrite{ "u_LightAttenuation", light_attenuation } };
}

} // namespace

namespace impl {

void UpdateLightVisibilityPolygons(
	std::vector<impl::EntityRenderCommand>& commands, std::span<const V2_float> camera_vertices
) {
	impl::KDTree tree{ 20 };
	std::vector<impl::KDObject> objects;
	std::vector<ShadowCasterEntry> casters;
	std::unordered_map<Entity, ShadowCasterEntry*> caster_lookup;

	objects.reserve(commands.size());
	casters.reserve(commands.size());

	auto camera_bounds{ GetBoundingAABB(camera_vertices) };

	for (auto order{ 0uz }; order < commands.size(); ++order) {
		auto entity{ commands[order].entity };

		if (entity.Has<LightConfig>()) {
			entity.Remove<impl::VisibilityPolygon>();
		}

		if (!entity.Has<impl::ShadowCaster>()) {
			continue;
		}

		const auto& caster{ entity.Get<impl::ShadowCaster>() };

		if (!caster.casts_shadows) {
			continue;
		}

		auto aabb{ GetShadowCasterAABB(entity) };

		if (!aabb.has_value()) {
			continue;
		}

		auto& entry{ casters.emplace_back(
			ShadowCasterEntry{
				.entity				= entity,
				.depth				= commands[order].depth,
				.order				= order,
				.aabb				= aabb.value(),
				.masks_light_inside = caster.masks_light_inside,
			}
		) };

		caster_lookup.emplace(entity, &entry);
		objects.emplace_back(entity, aabb.value());
	}

	if (objects.empty()) {
		return;
	}

	tree.Build(objects);

	for (auto order{ 0uz }; order < commands.size(); ++order) {
		auto entity{ commands[order].entity };

		if (!entity.Has<LightConfig>()) {
			continue;
		}

		auto candidates{ tree.Query(camera_bounds) };

		if (candidates.empty()) {
			continue;
		}

		auto polygon{ ComputeVisibilityPolygonForLight(
			entity, commands[order].depth, order, camera_vertices, casters, candidates
		) };

		if (polygon.vertices.empty()) {
			continue;
		}

		entity.Add<impl::VisibilityPolygon>(std::move(polygon));
	}
}

} // namespace impl

Light::Light(Entity entity) : Entity{ entity } {}

void Light::Draw(DrawContext& ctx, Entity entity) {
	if (!entity.Has<LightConfig>()) {
		return;
	}

	const auto& light{ entity.Get<LightConfig>() };

	if (light.cone_angle.has_value() && light.cone_angle.value() == Degrees{ 0.0f }) {
		return;
	}

	if (light.radius <= 0.0f) {
		return;
	}

	SetRotation(entity, light.direction_angle);

	auto draw_transform{ GetDrawTransform(entity) };
	auto size{ Circle{ light.radius }.GetSize() };
	auto blend_mode{ GetBlendMode(entity) };
	auto tint{ GetTint(entity) };

	auto uniforms{ std::ranges::to<std::vector<UniformWrite>>(GetUniforms(light, tint)) };

	if (!entity.Has<impl::VisibilityPolygon>()) {
		DrawUnmaskedLight(ctx, entity, draw_transform, size, blend_mode, std::move(uniforms));
		return;
	}

	const auto& visibility_polygon{ entity.Get<impl::VisibilityPolygon>() };

	if (visibility_polygon.vertices.empty()) {
		return;
	}

	DrawShadowedLight(
		ctx, entity, draw_transform, size, blend_mode, visibility_polygon, std::move(uniforms)
	);
}

Light& Light::Intensity(float intensity) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->intensity = std::max(intensity, 0.0f);
	}
	return *this;
}

Light& Light::Color(ptgn::Color color) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->color = color;
	}
	return *this;
}

Light& Light::AmbientIntensity(float ambient_intensity) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->ambient_intensity = std::max(ambient_intensity, 0.0f);
	}
	return *this;
}

Light& Light::AmbientColor(ptgn::Color ambient_color) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->ambient_color = ambient_color;
	}
	return *this;
}

Light& Light::Radius(float radius) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->radius = std::max(radius, 0.0f);
	}
	return *this;
}

Light& Light::Falloff(float falloff) {
	if (auto light{ TryGet<LightConfig>() }) {
		light->falloff = std::max(falloff, 0.0f);
	}
	return *this;
}

Light& Light::ConeAngle(std::optional<Degrees> cone_angle) {
	if (!Has<LightConfig>()) {
		return *this;
	}

	auto& light{ Get<LightConfig>() };

	if (!cone_angle.has_value()) {
		light.cone_angle = std::nullopt;
		return *this;
	}

	light.cone_angle = Clamp(cone_angle.value());

	return *this;
}

Light& Light::Config(const LightConfig& config) {
	Add<LightConfig>(config);
	SetRotation(*this, config.direction_angle);
	return *this;
}

LightConfig Light::GetConfig() const {
	PTGN_ASSERT(Has<LightConfig>());
	return Get<LightConfig>();
}

Light CreateLight(Scene& scene, Transform transform, const LightConfig& config) {
	Light light{ scene.CreateEntity() };

	PTGN_DEFAULT_NAME(light, "Light");

	light.Config(config);

	light.Add<Transform>(transform);
	light.Add<Visible>(true);

	// Blend mode with which the lights are added to the scene.
	light.Add<BlendMode>(BlendMode::PremultipliedAddRGBA);

	SetDraw<Light>(light);

	return light;
}

Entity SetOccluder(Entity entity, bool casts_shadows, bool masks_light_inside) {
	PTGN_ASSERT(entity, "Cannot set an invalid entity as an occluder");

	auto& caster{ entity.Add<impl::ShadowCaster>() };

	caster.casts_shadows	  = casts_shadows;
	caster.masks_light_inside = masks_light_inside;

	return entity;
}

} // namespace ptgn