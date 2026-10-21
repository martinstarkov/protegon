#include "runtime/scripting/scripts/rendering_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "core/util/time.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void SetTextureScript::OnStart() {
	Sprite{ Target() }.SetTexture(texture_key);
}

void SpriteSetDisplaySizeScript::OnStart() {
	if (!target) {
		return;
	}
	SetDisplaySize(target, option ? std::optional<V2_float>{ vector } : std::nullopt);
}

void SpriteSetCropScript::OnStart() {
	if (!target) {
		return;
	}
	target.Add<impl::TextureCrop>(impl::TextureCrop{
		vector, option ? std::optional<V2_float>{ vector2 } : std::nullopt });
}

void SpriteClearCropScript::OnStart() {
	if (!target) {
		return;
	}
	target.Remove<impl::TextureCrop>();
}

void SpriteSetTextureCoordinatesScript::OnStart() {
	if (!target) {
		return;
	}
	target.Add<impl::TextureCoordinateOverride>(impl::TextureCoordinateOverride{
		texture_coordinates });
}

void SpriteSetOriginScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto origin{ magic_enum::enum_cast<Origin>(text) }) {
		target.Add<Origin>(*origin);
	}
}

void SpriteFlipXScript::OnStart() {
	if (!target) {
		return;
	}
	SetScaleX(target, -GetScale(target).x);
}

void SpriteFlipYScript::OnStart() {
	if (!target) {
		return;
	}
	SetScaleY(target, -GetScale(target).y);
}

void SpriteStackSetTextureScript::OnStart() {
	if (!target) {
		return;
	}
	SpriteStack{ target }.SetTexture(TextureKey{ text });
}

void SpriteStackSetSliceCountScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* data{ target.TryGet<SpriteStackData>() }) {
		data->slice_count = static_cast<std::size_t>(std::max(0, index));
	}
}

void SpriteStackSetSliceOrderScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* data{ target.TryGet<SpriteStackData>() }) {
		if (auto order{ magic_enum::enum_cast<SpriteStackSliceOrder>(text) }) {
			data->slice_order = *order;
		}
	}
}

void SpriteStackSetLayerOffsetScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* data{ target.TryGet<SpriteStackData>() }) {
		data->layer_offset = vector;
	}
}

void SpriteStackSetPixelSnappingScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* data{ target.TryGet<SpriteStackData>() }) {
		data->pixel_snap = option;
	}
}

void TextSetTextScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Content(StyledText{ TextRun{ .text = this->text } });
}

void TextAppendTextScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Content(this->text);
}

void TextClearTextScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Clear();
}

void TextSetRichTextScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.SetRichText(this->text);
}

void TextSetTextColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Color(selected_color);
}

void TextSetFontScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Font(FontKey{ this->text });
}

void TextSetFontSizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Size(number);
}

void TextSetRevealCountScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Reveal(static_cast<std::size_t>(std::max(0, index)));
}

void TextSetRevealFractionScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.RevealFraction(number);
}

void TextRevealAllScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.RevealAll();
}

void TextTypewriterRevealScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}

	Text text_entity{ target };
	text_entity.Reveal(std::size_t{ 0 });

	PropertyTo<float>(
		target, SequenceChannelKey{ "text.reveal" }, 1.0f,
		milliseconds{ static_cast<std::int64_t>(std::max(1.0f, number)) },
		[](Entity entity) {
			return entity.Has<impl::TextData>() && Text{ entity }.IsFullyRevealed() ? 1.0f : 0.0f;
		},
		[](Entity entity, float fraction) { Text{ entity }.RevealFraction(fraction); }
	);
}

void TextSetAlignmentScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	if (auto value{ magic_enum::enum_cast<Origin>(this->text) }) {
		text_entity.Align(*value);
	}
}

void TextSetWrapModeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	if (auto value{ magic_enum::enum_cast<WrapMode>(this->text) }) {
		text_entity.Wrap(*value);
	}
}

void TextSetTextBoxScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Box(Rect{ vector, vector2 });
}

void TextSetClipScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	if (option) {
		text_entity.Clip(TextClip{ Rect{ vector, vector2 } });
	} else {
		text_entity.ClearClip();
	}
}

void TextSetOutlineScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Outline(selected_color, number);
}

void TextSetShadowScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.Shadow(selected_color, vector);
}

void TextSetGlowScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	text_entity.OuterGlow(selected_color, number);
}

void TextSetTextEffectScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::TextData>()) {
		return;
	}
	Text text_entity{ target };
	if (auto value{ magic_enum::enum_cast<GlyphEffectType>(this->text) }) {
		text_entity.Effect(*value, number, number2, vector.x, vector.y);
	}
}

void ParticlesStartScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Start();
}

void ParticlesStopScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Stop();
}

void ParticlesPauseScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Pause();
}

void ParticlesResumeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Resume();
}

void ParticlesToggleScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Toggle();
}

void ParticlesResetScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	emitter.Reset();
}

void ParticlesEmitBurstScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };

	auto& data{ target.Get<impl::ParticleEmitterData>() };

	for (int i{ 0 }; i < std::max(0, index); ++i) {
		if (!data.TrySpawnParticle()) {
			break;
		}
	}
}

void ParticlesSetEmissionRateScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };

	auto& data{ target.Get<impl::ParticleEmitterData>() };

	auto& config{ data.config };

	if (auto* rate{ std::get_if<ParticleRate>(&config.rate_or_burst) }) {
		rate->rate_over_time = std::max(0.0f, number);
	}
}

void ParticlesSetStartColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.start_color = use_range ? ConstantOrRange<Color>{ selected_color, selected_color2 }
								   : ConstantOrRange<Color>{ selected_color };
}

void ParticlesSetStartSpeedScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.start_speed =
		use_range ? ConstantOrRange<float>{ number, number2 } : ConstantOrRange<float>{ number };
}

void ParticlesSetStartSizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.start_size =
		use_range ? ConstantOrRange<float>{ number, number2 } : ConstantOrRange<float>{ number };
}

void ParticlesSetLifetimeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	milliseconds a{ static_cast<milliseconds::rep>(std::max(0.0f, number)) };
	milliseconds b{ static_cast<milliseconds::rep>(std::max(0.0f, number2)) };
	config.lifetime =
		use_range ? ConstantOrRange<milliseconds>{ a, b } : ConstantOrRange<milliseconds>{ a };
}

void ParticlesSetEmissionShapeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.emission_shape = emission_shape;
}

void ParticlesSetGravityScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.start_gravity = option ? std::optional<V2_float>{ vector } : std::nullopt;
}

void ParticlesSetSimulationSpeedScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.simulation_speed = std::max(0.0f, number);
}

void ParticlesSetMaximumParticlesScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.max_particles = static_cast<std::size_t>(std::max(0, index));
}

void ParticlesSetEndColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.color_over_lifetime =
		option ? std::optional<
					 ConstantOrRange<Color>>{ use_range ? ConstantOrRange<Color>{ selected_color,
																				  selected_color2 }
														: ConstantOrRange<Color>{ selected_color } }
			   : std::nullopt;
}

void ParticlesSetEndSizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.size_over_lifetime =
		option
			? std::optional<ConstantOrRange<float>>{ use_range
														 ? ConstantOrRange<float>{ number, number2 }
														 : ConstantOrRange<float>{ number } }
			: std::nullopt;
}

void ParticlesSetEndVelocityScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.velocity_over_lifetime =
		option
			? std::optional<ConstantOrRange<V2_float>>{ use_range
															? ConstantOrRange<V2_float>{ vector,
																						 vector2 }
															: ConstantOrRange<V2_float>{ vector } }
			: std::nullopt;
}

void ParticlesSetParticleTypeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::ParticleEmitterData>()) {
		return;
	}
	ParticleEmitter emitter{ target };
	auto& data{ target.Get<impl::ParticleEmitterData>() };
	auto& config{ data.config };
	config.particle_type = option ? ParticleType{ selected_shape } : ParticleType{ this->text };
}

void LightSetColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.Color(selected_color);
}

void LightSetIntensityScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.Intensity(number);
}

void LightSetRadiusScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.Radius(number);
}

void LightSetFalloffScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.Falloff(number);
}

void LightSetConeAngleScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.ConeAngle(option ? std::optional<Degrees>{ Degrees{ number } } : std::nullopt);
}

void LightSetDirectionAngleScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	target.Get<LightData>().direction_angle = Degrees{ number };
}

void LightSetAmbientColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.AmbientColor(selected_color);
}

void LightSetAmbientIntensityScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.AmbientIntensity(number);
}

void LightSetLightConfigScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<LightData>()) {
		return;
	}
	Light light{ target };
	light.Config(light_data);
}

void ShaderSetUniformScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<Material>()) {
		return;
	}
	CustomShader shader{ target };
	if (property.empty()) {
		return;
	}
	if (text == "Int") {
		shader.SetMaterialUniform(property, UniformValue{ index });
	} else if (text == "Vector") {
		shader.SetMaterialUniform(property, UniformValue{ vector });
	} else if (text == "Bool") {
		shader.SetMaterialUniform(property, UniformValue{ option });
	} else {
		shader.SetMaterialUniform(property, UniformValue{ number });
	}
}

void ShaderSetShaderScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<Material>()) {
		return;
	}
	CustomShader shader{ target };
	Material material{ target.Get<Material>() };
	material.shader = ShaderKey{ text };
	shader.SetMaterial(material);
}

void ShaderSetMaterialScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<Material>()) {
		return;
	}
	CustomShader shader{ target };
	shader.SetMaterial(material);
}

void ShaderSetMultipleUniformsScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<Material>()) {
		return;
	}
	CustomShader shader{ target };
	shader.SetMaterialUniforms(uniforms);
}

void RenderTargetSetClearColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.SetClearColor(selected_color);
}

void RenderTargetClearColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.ClearColor(option ? std::optional<Color>{ selected_color } : std::nullopt);
}

void RenderTargetClearDepthScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.ClearDepth(number);
}

void RenderTargetClearStencilScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.ClearStencil(static_cast<Stencil>(index));
}

void RenderTargetSetSizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.SetSize(V2_int{ static_cast<int>(vector.x), static_cast<int>(vector.y) });
}

void RenderTargetFollowDisplaySizeScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.SetFollowDisplaySize(option);
}

void RenderTargetSetClearDepthValueScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.SetClearDepth(number);
}

void RenderTargetSetClearStencilValueScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::RenderTargetDesc>()) {
		return;
	}
	RenderTarget rt{ target };
	rt.SetClearStencil(static_cast<Stencil>(index));
}

void CameraSetZoomScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.SetZoom(vector);
}

void CameraZoomByScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.Zoom(vector);
}

void CameraZoomToScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };

	PropertyTo<V2_float>(
		target, SequenceChannelKey{ "camera.zoom" }, vector,
		milliseconds{ static_cast<std::int64_t>(std::max(1.0f, number)) },
		[](Entity entity) { return SceneCamera{ entity }.GetZoom(); },
		[](Entity entity, V2_float zoom) { SceneCamera{ entity }.SetZoom(zoom); }
	);
}

void CameraResetCameraScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.Reset();
}

void CameraSetCameraBoundsScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.SetBounds(
		option
			? std::optional<BoundingBox>{ BoundingBox{ vector, Rect{ vector2 }, Origin::Center } }
			: std::nullopt
	);
}

void CameraSetPixelRoundingScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.SetPixelRounding(option);
}

void CameraSetRenderMaskScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	auto bits{ static_cast<RenderMaskBits>(std::max(0, index)) };
	if (property == "exclude") {
		camera.SetExcludeRenderMask(bits);
	} else {
		camera.SetIncludeRenderMask(bits);
	}
}

void CameraAddRenderMaskBitsScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	auto bits{ static_cast<RenderMaskBits>(std::max(0, index)) };
	if (property == "exclude") {
		camera.AddExcludeRenderMaskBits(bits);
	} else {
		camera.AddIncludeRenderMaskBits(bits);
	}
}

void CameraRemoveRenderMaskBitsScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	auto bits{ static_cast<RenderMaskBits>(std::max(0, index)) };
	if (property == "exclude") {
		camera.RemoveExcludeRenderMaskBits(bits);
	} else {
		camera.RemoveIncludeRenderMaskBits(bits);
	}
}

void CameraSetClearColorScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.SetClearColor(option ? std::optional<Color>{ selected_color } : std::nullopt);
}

void CameraSetRenderTargetScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	if (text.empty()) {
		camera.SetRenderTarget();
		return;
	}
	Entity parent{ GetScene().GetEntity(Tag{ text }) };
	if (parent && parent.Has<impl::RenderTargetDesc>()) {
		camera.SetRenderTarget(RenderTarget{ parent });
	}
}

void CameraSetViewportScript::OnStart() {
	if (!target) {
		return;
	}
	if (!target.Has<impl::CameraData>()) {
		return;
	}
	SceneCamera camera{ target };
	camera.SetViewport(
		option ? std::optional<Viewport>{ Viewport{ vector, vector2 } } : std::nullopt
	);
}

void EffectEnableEffectScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::EffectTag>()) {
		SetVisible(target, true);
	}
}

void EffectDisableEffectScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::EffectTag>()) {
		SetVisible(target, false);
	}
}

void EffectToggleEffectScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::EffectTag>()) {
		bool visible{ target.Has<Visible>() ? target.Get<Visible>().visible : true };
		SetVisible(target, !visible);
	}
}

void EffectSetEffectMarginScript::OnStart() {
	if (!target) {
		return;
	}
	target.Add<EffectMargin>(EffectMargin{ std::max(0, index) });
}

void EffectClearEffectsScript::OnStart() {
	if (!target) {
		return;
	}
	ClearEffects(target);
}

void EffectClearScreenEffectsScript::OnStart() {
	if (!target) {
		return;
	}
	auto& app{ impl::SceneContextAccessor::app(GetScene().ctx()) };
	impl::ClearScreenEffects(impl::ApplicationAccessor::ctx(app));
}

void EffectAddEffectScript::OnStart() {
	if (!target) {
		return;
	}
	const auto* effect{ impl::EffectRegistry::Find(text) };
	if (!effect || !effect->create) {
		return;
	}
	Entity instance{ GetScene().CreateEntity() };
	effect->create(instance, effect->make_default ? effect->make_default() : json::object());
	AddChild(target, instance);
	SetVisible(instance, false);
}

void EffectAddScreenEffectScript::OnStart() {
	if (!target) {
		return;
	}
	auto& app{ impl::SceneContextAccessor::app(GetScene().ctx()) };
	auto& app_context{ impl::ApplicationAccessor::ctx(app) };
	impl::CreateScreenEffectEntity(app_context, text);
}

void EffectRemoveScreenEffectScript::OnStart() {
	if (!target) {
		return;
	}
	auto& app{ impl::SceneContextAccessor::app(GetScene().ctx()) };
	auto& app_context{ impl::ApplicationAccessor::ctx(app) };
	if (auto snapshot{ impl::CaptureScreenEffect(target) }) {
		impl::RemoveScreenEffect(app_context, snapshot->runtime_id);
	}
}

void EffectRemoveEffectScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::EffectTag>()) {
		target.Destroy();
		return;
	}
	if (!HasChildren(target)) {
		return;
	}
	for (Entity child : GetChildren(target)) {
		if (!child.Has<impl::EffectTag>()) {
			continue;
		}
		if (!text.empty()) {
			const auto* registered{ impl::EffectRegistry::Find(text) };
			const auto* component{ registered ? ComponentRegistry::Find(registered->type_name)
											  : nullptr };
			if (!component || !component->Has(child)) {
				continue;
			}
		}
		child.Destroy();
		return;
	}
}

void EffectSetEffectPropertyScript::OnStart() {
	if (!target) {
		return;
	}
	const impl::RegisteredEffect* effect{ nullptr };
	if (!text.empty()) {
		effect = impl::EffectRegistry::Find(text);
	} else {
		for (const auto& candidate : impl::EffectRegistry::Entries()) {
			const auto* component{ ComponentRegistry::Find(candidate.type_name) };
			if (component && component->Has(target)) {
				effect = &candidate;
				break;
			}
		}
	}
	if (!effect || !effect->serialize || !effect->deserialize || property.empty()) {
		return;
	}
	const auto* component{ ComponentRegistry::Find(effect->type_name) };
	if (!component || !component->Has(target)) {
		return;
	}
	json values{ effect->serialize(target) };
	if (!values.is_object() || !values.contains(property)) {
		return;
	}
	json& field{ values[property] };
	if (property == "tint" || property == "color") {
		field = selected_color;
	} else if (field.is_boolean()) {
		field = option;
	} else if (field.is_number_unsigned() || field.is_number_integer()) {
		field = std::max(0, index);
	} else if (field.is_number()) {
		field = number;
	} else if (field.is_string()) {
		field = choice;
	} else {
		return;
	}
	try {
		effect->deserialize(target, values);
	} catch (const std::exception& error) {
		PTGN_WARN("Invalid effect property: ", error.what());
	}
}

} // namespace ptgn