
#include "runtime/graphics/custom_shader.h"

#include <chrono>

#include "app/application.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/resources/shader.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

using namespace ptgn;

class CustomShaderScene : public Scene {
public:
	CustomShader shader_entity;

	void OnEnter() override {
		ctx().asset.Load("whirlpool", "assets/shader.glsl");
		ctx().asset.Load("ripple", ShaderPair{ "texture", "assets/ripple.glsl" });
		ctx().asset.Load("noise", "assets/noise.png");

		shader_entity = CreateCustomShader(
			*this, "whirlpool", "noise", V2_float{}, V2_float{ 150 }, {}, Origin::Center
		);

		SetMaterialUpdate(shader_entity, [](auto entity) mutable {
			float timescale{ 1.0f };
			float scale{ 0.5f };
			float opacity{ 0.5f };
			float time{ static_cast<float>(entity.GetScene().ctx().TimeSinceStart().count()) };

			SetMaterialUniforms(
				entity, { { "u_Time", time / 1000.0f * timescale },
						  { "u_Scale", scale },
						  { "u_Opacity", opacity } }
			);
		});

		auto shader_entity2{ CreateCustomShader(
			*this, "ripple", {}, V2_float{ 200 }, V2_float{ 300 }, {}, Origin::Center
		) };

		SetMaterialUpdate(shader_entity2, [](auto entity) mutable {
			float timescale{ 1.0f };
			float time{ static_cast<float>(entity.GetScene().ctx().TimeSinceStart().count()) };

			SetMaterialUniforms(entity, { { "u_Time", time / 1000.0f * timescale } });
		});
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f };
		V2_float pos{ GetPosition(shader_entity) };
		float dt{ ctx().dt().count() };
		MoveWASD(*this, pos, speed * dt, false);
		SetPosition(shader_entity, pos);
	}
};

int main(int, char**) {
	Application game{ "CustomShaderScene: WASD: Move" };
	game.StartWith<CustomShaderScene>();
}
