
#include <chrono>

#include "app/application.h"
#include "app/context.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/shader.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/shader_component.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"

using namespace ptgn;

class CustomShaderScene : public Scene {
public:
	ShaderEntity shader_entity;

	void OnEnter() override {
		app().asset.LoadShader("whirlpool", "assets/shader.glsl", "whirlpool");
		app().asset.LoadTexture("noise", "assets/noise.png");

		shader_entity = CreateShaderEntity(
			*this, "whirlpool", "noise", V2_float{}, V2_float{ 200.0f },
			[this](auto s) mutable {
				float timescale{ 1.0f };
				float scale{ 0.5f };
				float opacity{ 0.5f };

				float time{ static_cast<float>(app().TimeSinceStart().count()) };
				s.SetUniform("u_Time", time / 1000.0f * timescale);
				s.SetUniform("u_Scale", scale);
				s.SetUniform("u_Opacity", opacity);
			},
			Origin::Center
		);
	}

	void OnUpdate() override {
		constexpr V2_float speed{ 300.0f };
		V2_float pos{ GetPosition(shader_entity) };
		float dt{ app().DeltaTime().count() };
		MoveWASD(*this, pos, speed * dt, false);
		SetPosition(shader_entity, pos);
	}
};

int main(int, char**) {
	Application game{ "CustomShaderScene: WASD: Move" };
	game.StartWith<CustomShaderScene>();
}
