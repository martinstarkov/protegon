#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct SetTextureScript : public Script {
	TextureKey texture_key{};
	SetTextureScript() = default;

	explicit SetTextureScript(TextureKey texture) : texture_key{ std::move(texture) } {}

	void OnStart() override;
	PTGN_REFLECT(SetTextureScript, texture_key)
};

struct SpriteSetDisplaySizeScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };

	PTGN_REFLECT(SpriteSetDisplaySizeScript, vector, option)
};

struct SpriteSetCropScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };
	V2_float vector2{};

	PTGN_REFLECT(SpriteSetCropScript, vector, option, vector2)
};

struct SpriteClearCropScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(SpriteClearCropScript)
};

struct SpriteSetTextureCoordinatesScript : public Script {
	void OnStart() override;
	std::array<V2_float, 4> texture_coordinates{ V2_float{}, V2_float{ 1.0f, 0.0f },
												 V2_float{ 1.0f, 1.0f }, V2_float{ 0.0f, 1.0f } };

	PTGN_REFLECT(SpriteSetTextureCoordinatesScript, texture_coordinates)
};

struct SpriteSetOriginScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(SpriteSetOriginScript, text)
};

struct SpriteFlipXScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(SpriteFlipXScript)
};

struct SpriteFlipYScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(SpriteFlipYScript)
};

struct SpriteStackSetTextureScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(SpriteStackSetTextureScript, text)
};

struct SpriteStackSetSliceCountScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(SpriteStackSetSliceCountScript, index)
};

struct SpriteStackSetSliceOrderScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(SpriteStackSetSliceOrderScript, text)
};

struct SpriteStackSetLayerOffsetScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(SpriteStackSetLayerOffsetScript, vector)
};

struct SpriteStackSetPixelSnappingScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(SpriteStackSetPixelSnappingScript, option)
};

struct TextSetTextScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextSetTextScript, text)
};

struct TextAppendTextScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextAppendTextScript, text)
};

struct TextClearTextScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TextClearTextScript)
};

struct TextSetRichTextScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextSetRichTextScript, text)
};

struct TextSetTextColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(TextSetTextColorScript, selected_color)
};

struct TextSetFontScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextSetFontScript, text)
};

struct TextSetFontSizeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(TextSetFontSizeScript, number)
};

struct TextSetRevealCountScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(TextSetRevealCountScript, index)
};

struct TextSetRevealFractionScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(TextSetRevealFractionScript, number)
};

struct TextRevealAllScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TextRevealAllScript)
};

struct TextTypewriterRevealScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(TextTypewriterRevealScript, number)
};

struct TextSetAlignmentScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextSetAlignmentScript, text)
};

struct TextSetWrapModeScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(TextSetWrapModeScript, text)
};

struct TextSetTextBoxScript : public Script {
	void OnStart() override;
	V2_float vector{};
	V2_float vector2{};

	PTGN_REFLECT(TextSetTextBoxScript, vector, vector2)
};

struct TextSetClipScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };
	V2_float vector2{};

	PTGN_REFLECT(TextSetClipScript, vector, option, vector2)
};

struct TextSetOutlineScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	Color selected_color{ color::White };

	PTGN_REFLECT(TextSetOutlineScript, number, selected_color)
};

struct TextSetShadowScript : public Script {
	void OnStart() override;
	V2_float vector{};
	Color selected_color{ color::White };

	PTGN_REFLECT(TextSetShadowScript, vector, selected_color)
};

struct TextSetGlowScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	Color selected_color{ color::White };

	PTGN_REFLECT(TextSetGlowScript, number, selected_color)
};

struct TextSetTextEffectScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };
	V2_float vector{};
	std::string property{};
	float number2{};

	PTGN_REFLECT(TextSetTextEffectScript, text, number, vector, property, number2)
};

struct ParticlesStartScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesStartScript)
};

struct ParticlesStopScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesStopScript)
};

struct ParticlesPauseScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesPauseScript)
};

struct ParticlesResumeScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesResumeScript)
};

struct ParticlesToggleScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesToggleScript)
};

struct ParticlesResetScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ParticlesResetScript)
};

struct ParticlesEmitBurstScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(ParticlesEmitBurstScript, index)
};

struct ParticlesSetEmissionRateScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ParticlesSetEmissionRateScript, number)
};

struct ParticlesSetStartColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };
	Color selected_color2{ color::White };
	bool use_range{};

	PTGN_REFLECT(ParticlesSetStartColorScript, selected_color, selected_color2, use_range)
};

struct ParticlesSetStartSpeedScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	float number2{};
	bool use_range{};

	PTGN_REFLECT(ParticlesSetStartSpeedScript, number, number2, use_range)
};

struct ParticlesSetStartSizeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	float number2{};
	bool use_range{};

	PTGN_REFLECT(ParticlesSetStartSizeScript, number, number2, use_range)
};

struct ParticlesSetLifetimeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	float number2{};
	bool use_range{};

	PTGN_REFLECT(ParticlesSetLifetimeScript, number, number2, use_range)
};

struct ParticlesSetEmissionShapeScript : public Script {
	void OnStart() override;
	EmissionShape emission_shape{};

	PTGN_REFLECT(ParticlesSetEmissionShapeScript, emission_shape)
};

struct ParticlesSetGravityScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };

	PTGN_REFLECT(ParticlesSetGravityScript, vector, option)
};

struct ParticlesSetSimulationSpeedScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ParticlesSetSimulationSpeedScript, number)
};

struct ParticlesSetMaximumParticlesScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(ParticlesSetMaximumParticlesScript, index)
};

struct ParticlesSetEndColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };
	bool option{ true };
	Color selected_color2{ color::White };
	bool use_range{};

	PTGN_REFLECT(ParticlesSetEndColorScript, selected_color, selected_color2, use_range, option)
};

struct ParticlesSetEndSizeScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	bool option{ true };
	float number2{};
	bool use_range{};

	PTGN_REFLECT(ParticlesSetEndSizeScript, number, number2, use_range, option)
};

struct ParticlesSetEndVelocityScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };
	V2_float vector2{};
	bool use_range{};

	PTGN_REFLECT(ParticlesSetEndVelocityScript, vector, vector2, use_range, option)
};

struct ParticlesSetParticleTypeScript : public Script {
	void OnStart() override;
	std::string text{};
	bool option{ true };
	Shape selected_shape{};

	PTGN_REFLECT(ParticlesSetParticleTypeScript, text, option, selected_shape)
};

struct LightSetColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(LightSetColorScript, selected_color)
};

struct LightSetIntensityScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(LightSetIntensityScript, number)
};

struct LightSetRadiusScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(LightSetRadiusScript, number)
};

struct LightSetFalloffScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(LightSetFalloffScript, number)
};

struct LightSetConeAngleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	bool option{ true };

	PTGN_REFLECT(LightSetConeAngleScript, number, option)
};

struct LightSetDirectionAngleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(LightSetDirectionAngleScript, number)
};

struct LightSetAmbientColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(LightSetAmbientColorScript, selected_color)
};

struct LightSetAmbientIntensityScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(LightSetAmbientIntensityScript, number)
};

struct LightSetLightConfigScript : public Script {
	void OnStart() override;
	LightData light_data{};

	PTGN_REFLECT(LightSetLightConfigScript, light_data)
};

struct ShaderSetUniformScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };
	int index{};
	V2_float vector{};
	bool option{ true };
	std::string property{};

	PTGN_REFLECT(ShaderSetUniformScript, property, text, number, index, vector, option)
};

struct ShaderSetShaderScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(ShaderSetShaderScript, text)
};

struct ShaderSetMaterialScript : public Script {
	void OnStart() override;
	Material material{};

	PTGN_REFLECT(ShaderSetMaterialScript, material)
};

struct ShaderSetMultipleUniformsScript : public Script {
	void OnStart() override;
	std::vector<UniformWrite> uniforms{};

	PTGN_REFLECT(ShaderSetMultipleUniformsScript, uniforms)
};

struct RenderTargetSetClearColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(RenderTargetSetClearColorScript, selected_color)
};

struct RenderTargetClearColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };
	bool option{ true };

	PTGN_REFLECT(RenderTargetClearColorScript, selected_color, option)
};

struct RenderTargetClearDepthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RenderTargetClearDepthScript, number)
};

struct RenderTargetClearStencilScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(RenderTargetClearStencilScript, index)
};

struct RenderTargetSetSizeScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(RenderTargetSetSizeScript, vector)
};

struct RenderTargetFollowDisplaySizeScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(RenderTargetFollowDisplaySizeScript, option)
};

struct RenderTargetSetClearDepthValueScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(RenderTargetSetClearDepthValueScript, number)
};

struct RenderTargetSetClearStencilValueScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(RenderTargetSetClearStencilValueScript, index)
};

struct CameraSetZoomScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(CameraSetZoomScript, vector)
};

struct CameraZoomByScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(CameraZoomByScript, vector)
};

struct CameraZoomToScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	V2_float vector{};

	PTGN_REFLECT(CameraZoomToScript, number, vector)
};

struct CameraResetCameraScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(CameraResetCameraScript)
};

struct CameraSetCameraBoundsScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };
	V2_float vector2{};

	PTGN_REFLECT(CameraSetCameraBoundsScript, vector, option, vector2)
};

struct CameraSetPixelRoundingScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(CameraSetPixelRoundingScript, option)
};

struct CameraSetRenderMaskScript : public Script {
	void OnStart() override;
	int index{};
	std::string property{};

	PTGN_REFLECT(CameraSetRenderMaskScript, index, property)
};

struct CameraAddRenderMaskBitsScript : public Script {
	void OnStart() override;
	int index{};
	std::string property{};

	PTGN_REFLECT(CameraAddRenderMaskBitsScript, index, property)
};

struct CameraRemoveRenderMaskBitsScript : public Script {
	void OnStart() override;
	int index{};
	std::string property{};

	PTGN_REFLECT(CameraRemoveRenderMaskBitsScript, index, property)
};

struct CameraSetClearColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };
	bool option{ true };

	PTGN_REFLECT(CameraSetClearColorScript, selected_color, option)
};

struct CameraSetRenderTargetScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(CameraSetRenderTargetScript, text)
};

struct CameraSetViewportScript : public Script {
	void OnStart() override;
	V2_float vector{};
	bool option{ true };
	V2_float vector2{};

	PTGN_REFLECT(CameraSetViewportScript, vector, option, vector2)
};

struct EffectEnableEffectScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EffectEnableEffectScript)
};

struct EffectDisableEffectScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EffectDisableEffectScript)
};

struct EffectToggleEffectScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EffectToggleEffectScript)
};

struct EffectSetEffectPropertyScript : public Script {
	void OnStart() override;
	std::string text{};
	float number{ 1.0f };
	int index{};
	Color selected_color{ color::White };
	bool option{ true };
	std::string property{};
	std::string choice{};

	PTGN_REFLECT(
		EffectSetEffectPropertyScript, text, number, index, selected_color, option, property, choice
	)
};

struct EffectAddEffectScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EffectAddEffectScript, text)
};

struct EffectRemoveEffectScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EffectRemoveEffectScript, text)
};

struct EffectAddScreenEffectScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EffectAddScreenEffectScript, text)
};

struct EffectRemoveScreenEffectScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(EffectRemoveScreenEffectScript, text)
};

struct EffectClearEffectsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EffectClearEffectsScript)
};

struct EffectClearScreenEffectsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(EffectClearScreenEffectsScript)
};

struct EffectSetEffectMarginScript : public Script {
	void OnStart() override;
	int index{};

	PTGN_REFLECT(EffectSetEffectMarginScript, index)
};

} // namespace ptgn
