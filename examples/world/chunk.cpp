
#include "runtime/world/chunk.h"

#include <string_view>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/noise.h"
#include "core/math/vector2.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/input/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_manager.h"

using namespace ptgn;

class ChunkScene : public Scene {
public:
	Entity CreateSheep(const V2_float& position) {
		auto e = CreateEntity();
		SetPosition(e, position);
		Show(e);
		SetDepth(e, 1);
		e.Add<TextureHandle>("sheep");
		return e;
	}

	Entity CreateTile(const V2_float& position, std::string_view texture_key) {
		auto e = CreateEntity();
		SetPosition(e, position);
		Show(e);
		SetDrawOrigin(e, Origin::TopLeft);
		e.Add<TextureHandle>(texture_key);
		return e;
	}

	Entity CreateColorTile(const V2_float& position, const Color& color) {
		auto e =
			CreateRect(*this, position, chunk_manager.tile_size, color, -1.0f, Origin::TopLeft);
		return e;
	}

	Entity sheep;

	V2_float vel;
	V2_float speed{ 30, 30 };
	static constexpr float zoom_speed{ 0.3f };

	ChunkManager chunk_manager;

	void OnEnter() override {
		FractalNoise fractal_noise;
		fractal_noise.SetOctaves(3);
		fractal_noise.SetFrequency(0.001f);
		fractal_noise.SetLacunarity(20.0f);
		fractal_noise.SetPersistence(0.8f);

		game.texture.Load("sheep", "assets/test.png");
		game.texture.Load("red", "assets/red_tile.png");
		game.texture.Load("blue", "assets/blue_tile.png");
		game.texture.Load("green", "assets/green_tile.png");

		chunk_manager.AddNoiseLayer(NoiseLayer{
			fractal_noise, [&](const V2_float& coordinate, float noise) {
				return CreateColorTile(
					-app().renderer.GetGameSize() * 0.5f + coordinate, color::White.WithAlpha(noise)
				);
			} });

		sheep = CreateSheep(V2_float{ 0, 0 });
		StartFollow(camera, sheep);
	}

	void OnUpdate() override {
		MoveWASD(vel, speed, true);
		Translate(sheep, vel * app().DeltaTime());

		if (input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * app().DeltaTime());
		}
		if (input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * app().DeltaTime());
		}

		chunk_manager.Update(*this, camera);
	}
};

int main(int, char**) {
	Application app{ "ChunkScene", { 1280, 720 } };
	app.StartWith<ChunkScene>();
}