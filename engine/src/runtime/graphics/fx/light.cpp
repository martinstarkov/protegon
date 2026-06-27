#include "runtime/graphics/fx/light.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
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
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/broadphase.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

constexpr int kLightVisibleStencilRef{ 1 };
constexpr float kLightShadowQueryFactor{ 1.25f };
constexpr int kCircleShadowSegments{ 24 };

struct ShadowCasterEntry {
	Entity entity;
	float depth{ 0.0f };
	std::size_t order{ 0 };
	BoundingAABB aabb;
	bool masks_light_inside{ true };
};

bool IsLightEntity(Entity entity) {
	return entity.Has<Circle, impl::LightData>();
}

bool IsUsableShadowCaster(Entity entity) {
	auto caster{ entity.TryGet<impl::ShadowCaster>() };
	return caster && caster->casts_shadows;
}

BoundingAABB MakeAABB(V2_float center, float radius) {
	auto r{ V2_float{ radius, radius } };

	return BoundingAABB{
		.min = center - r,
		.max = center + r,
	};
}

float GetScaledLightRadius(Entity entity) {
	auto radius{ entity.Get<Circle>().radius };
	auto scale{ GetWorldScale(entity) };

	auto scale_x{ std::abs(scale.x) };
	auto scale_y{ std::abs(scale.y) };

	return radius * std::max(scale_x, scale_y);
}

BoundingAABB GetLightInfluenceAABB(Entity entity) {
	auto position{ GetPosition(entity) };
	auto radius{ GetScaledLightRadius(entity) * kLightShadowQueryFactor };

	return MakeAABB(position, radius);
}

std::vector<V2_float> BuildCircleVertices(const Circle& circle, Transform transform) {
	std::vector<V2_float> vertices;
	vertices.reserve(kCircleShadowSegments);

	for (auto i{ 0 }; i < kCircleShadowSegments; ++i) {
		auto t{ kTwoPi * static_cast<float>(i) / static_cast<float>(kCircleShadowSegments) };
		auto local{ V2_float{ std::cos(t), std::sin(t) } * circle.radius };

		vertices.emplace_back(transform.Apply(local));
	}

	return vertices;
}

std::optional<V2_float> GetTextureCasterSize(Entity entity) {
	auto texture_size{ GetTextureSize(entity) };

	if (!texture_size.has_value() || !texture_size.value().IsPositive()) {
		return std::nullopt;
	}

	return V2_float{ texture_size.value() };
}

std::optional<std::vector<V2_float>> GetShadowCasterWorldVertices(Entity entity) {
	auto transform{ GetDrawTransform(entity) };
	auto origin{ GetDrawOrigin(entity) };

	if (entity.Has<Rect>()) {
		auto vertices{ entity.Get<Rect>().GetWorldVertices(transform, origin) };
		return std::vector<V2_float>{ vertices.begin(), vertices.end() };
	}

	if (entity.Has<Circle>()) {
		return BuildCircleVertices(entity.Get<Circle>(), transform);
	}

	if (auto texture_size{ GetTextureCasterSize(entity) }) {
		Rect rect{ *texture_size };
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

	if (auto texture_size{ GetTextureCasterSize(entity) }) {
		return GetBoundingAABB(Rect{ *texture_size }, transform);
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

std::vector<V2_float> BuildFallbackCameraPolygon(std::span<const V2_float> camera_vertices) {
	return std::vector<V2_float>{ camera_vertices.begin(), camera_vertices.end() };
}

std::vector<V2_float> RunVisibilitySolver(
	V2_float light_position, std::span<const V2_float> camera_vertices,
	std::span<const Line> segments
) {
	if (segments.empty()) {
		return BuildFallbackCameraPolygon(camera_vertices);
	}

	return GetVisibilityPolygon(light_position, segments);
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
		result.vertices = BuildFallbackCameraPolygon(camera_vertices);
		return result;
	}

	AddPolygonSegments(segments, camera_vertices);

	auto world_visibility{ RunVisibilitySolver(light_position, camera_vertices, segments) };

	result.vertices			  = std::move(world_visibility);
	result.occluder_interiors = std::move(interiors);

	return result;
}

void ClearStaleVisibilityPolygon(Entity entity) {
	if (entity.Has<impl::VisibilityPolygon>()) {
		entity.Remove<impl::VisibilityPolygon>();
	}
}

bool IsInvisibleCone(Entity entity) {
	const auto& light{ entity.Get<impl::LightData>() };
	return light.cone_angle.has_value() && light.cone_angle.value() == Radians{ 0.0f };
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

			// DrawStencilPolygon(ctx, draw_transform, visibility_polygon.vertices);

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

	PTGN_ASSERT(target_size.IsPositive(), "Light shadow target size must be positive");

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

struct LightVisibilityDebugSettings {
	bool draw_enabled{ true };
	bool draw_interiors{ true };

	Color polygon_color{ color::Yellow };
	Color masks_inside_color{ color::Red };
	Color does_not_mask_inside_color{ color::Green };

	FillStyle draw_fill_style{ 2.0f };
};

void DrawPolygonLines(
	Scene& scene, const SceneCamera& camera, std::span<const V2_float> vertices, Color color,
	const FillStyle& fill_style, Depth depth
) {
	if (vertices.size() < 2) {
		return;
	}

	scene.ctx().render_queue.DrawLines(
		vertices, color,
		ShapeRenderParams{
			.fill_style = fill_style,
			.origin		= Origin::Center,
			.depth		= depth,
			.camera		= camera,
			.debug		= true,
		},
		true, {}
	);
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

		if (IsLightEntity(entity)) {
			ClearStaleVisibilityPolygon(entity);
		}

		if (!IsUsableShadowCaster(entity)) {
			continue;
		}

		auto aabb{ GetShadowCasterAABB(entity) };

		if (!aabb.has_value()) {
			continue;
		}

		const auto& caster{ entity.Get<impl::ShadowCaster>() };

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

		if (!IsLightEntity(entity)) {
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

void DrawDebugLightVisibilityPolygons(
	Scene& scene, const SceneCamera& camera, const impl::EntityFilterFunc& filter
) {
	constexpr LightVisibilityDebugSettings debug_settings{};

	if (!debug_settings.draw_enabled) {
		return;
	}

	for (auto [entity, _light, visibility_polygon] :
		 scene.EntitiesWith<impl::LightData, impl::VisibilityPolygon>()) {
		// Mask test: entity layers vs camera include/exclude.
		if (filter(entity)) {
			continue;
		}

		if (visibility_polygon.vertices.empty()) {
			continue;
		}

		auto depth{ GetDepth(entity) };

		DrawPolygonLines(
			scene, camera, visibility_polygon.vertices, debug_settings.polygon_color,
			debug_settings.draw_fill_style, depth
		);

		if (!debug_settings.draw_interiors) {
			continue;
		}

		for (const auto& interior : visibility_polygon.occluder_interiors) {
			auto color{ interior.masks_light_inside ? debug_settings.masks_inside_color
													: debug_settings.does_not_mask_inside_color };

			DrawPolygonLines(
				scene, camera, interior.vertices, color, debug_settings.draw_fill_style, depth
			);
		}
	}
}

} // namespace impl

Light::Light(Entity entity) : Entity{ entity } {}

std::array<UniformWrite, 9> Light::GetUniforms() const {
	const auto& light{ Get<impl::LightData>() };

	auto color{ GetTint(*this) };
	V4_float color_n{ color.Normalized() };

	auto ambient_light_n{ light.ambient_color.Normalized() };
	V3_float ambient_color{ ambient_light_n.xyz() };
	constexpr V3_float light_attenuation{ 1.0f, 0.0f, 0.1f };

	return { { { "u_LightIntensity", light.intensity },
			   { "u_LightRadius", 0.5f },
			   { "u_Falloff", light.falloff },
			   { "u_UseCone", light.cone_angle.has_value() ? 1.0f : 0.0f },
			   { "u_ConeAngle",
				 light.cone_angle.has_value() ? (light.cone_angle.value() / 2.0f).value : kTwoPi },
			   { "u_Color", color_n },
			   { "u_AmbientColor", ambient_color },
			   { "u_AmbientIntensity", light.ambient_intensity },
			   { "u_LightAttenuation", light_attenuation } } };
}

void Light::Draw(DrawContext& ctx, Entity entity) {
	PTGN_ASSERT((entity.Has<Circle, impl::LightData>()));

	if (IsInvisibleCone(entity)) {
		return;
	}

	auto draw_transform{ GetDrawTransform(entity) };
	const auto& circle{ entity.Get<Circle>() };
	auto size{ circle.GetSize() };
	auto blend_mode{ GetBlendMode(entity) };

	auto uniforms{ std::ranges::to<std::vector<UniformWrite>>(Light{ entity }.GetUniforms()) };

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

Light& Light::SetIntensity(float intensity) {
	Get<impl::LightData>().intensity = intensity;
	return *this;
}

float Light::GetIntensity() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().intensity;
}

Light& Light::SetColor(Color color) {
	Add<impl::Tint>(color);
	return *this;
}

Color Light::GetColor() const {
	PTGN_ASSERT(Has<impl::Tint>(), "Light must have Tint component");
	return Get<impl::Tint>();
}

Light& Light::SetAmbientIntensity(float ambient_intensity) {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().ambient_intensity = ambient_intensity;
	return *this;
}

float Light::GetAmbientIntensity() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().ambient_intensity;
}

Light& Light::SetAmbientColor(Color ambient_color) {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().ambient_color = ambient_color;
	return *this;
}

Color Light::GetAmbientColor() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().ambient_color;
}

Light& Light::SetRadius(float radius) {
	PTGN_ASSERT(radius > 0.0f, "Light radius must be above 0");
	Add<Circle>().radius = radius;
	return *this;
}

float Light::GetRadius() const {
	PTGN_ASSERT(Has<Circle>(), "Light must have Circle component");
	return Get<Circle>().radius;
}

Light& Light::SetFalloff(float falloff) {
	PTGN_ASSERT(falloff >= 0.0f, "Light falloff must be above or equal to 0");
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	Get<impl::LightData>().falloff = falloff;
	return *this;
}

float Light::GetFalloff() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	return Get<impl::LightData>().falloff;
};

Light& Light::SetConeAngle(std::optional<Degrees> cone_angle) {
	PTGN_ASSERT(Has<impl::LightData>(), "Directional light must have LightData component");
	auto& light_data{ Get<impl::LightData>() };

	if (!cone_angle.has_value()) {
		light_data.cone_angle = std::nullopt;
		return *this;
	}

	light_data.cone_angle = Radians{ Clamp(cone_angle.value()) };

	return *this;
}

std::optional<Degrees> Light::GetConeAngle() const {
	PTGN_ASSERT(Has<impl::LightData>(), "Light must have LightData component");
	if (const auto& light_data{ Get<impl::LightData>() }; light_data.cone_angle.has_value()) {
		return light_data.cone_angle.value().ToDeg();
	} else {
		return std::nullopt;
	}
}

Light& Light::SetLightProperties(const LightProperties& properties) {
	SetRadius(properties.radius);
	SetColor(properties.color);
	SetIntensity(properties.intensity);
	SetFalloff(properties.falloff);
	SetConeAngle(properties.cone_angle);
	SetRotation(*this, properties.direction_angle);
	return *this;
}

LightProperties Light::GetLightProperties() const {
	LightProperties properties;
	properties.radius	  = GetRadius();
	properties.color	  = GetColor();
	properties.intensity  = GetIntensity();
	properties.falloff	  = GetFalloff();
	properties.cone_angle = GetConeAngle();
	auto rotation{ GetRotation(*this) };
	properties.direction_angle = rotation;
	return properties;
}

Light CreateLight(Scene& scene, Transform transform, const LightProperties& properties) {
	Light light{ scene.CreateEntity() };
	light.Add<impl::LightData>();
	light.SetLightProperties(properties);

	SetTransform(light, transform);
	SetDraw<Light>(light);
	light.Add<impl::Visible>(true);

	// Blend mode with which the lights are added to the scene.
	SetBlendMode(light, BlendMode::PremultipliedAddRGBA);

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