
#include <string_view>

#include "ecs/components/draw.h"
#include "ecs/components/movement.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/app/application.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "ecs/components/origin.h"
#include "renderer/renderer.h"

#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "world/tile/chunk.h"
#include "tween/tween_effect.h"

using namespace ptgn;

class ChunkScene : public Scene {
public:
	Entity CreateSheep(V2_float position) {
		auto e = CreateEntity();
		SetPosition(e, position);
		Show(e);
		SetDepth(e, 1);
		e.Add<TextureHandle>("sheep");
		return e;
	}

	Entity CreateTile(V2_float position, std::string_view texture_key) {
		auto e = CreateEntity();
		SetPosition(e, position);
		Show(e);
		SetDrawOrigin(e, Origin::TopLeft);
		e.Add<TextureHandle>(texture_key);
		return e;
	}

	Entity CreateColorTile(V2_float position, Color color) {
		auto e =
			CreateRect(*this, position, chunk_manager.tile_size, color, -1.0f, Origin::TopLeft);
		return e;
	}

	Entity sheep;

	V2_float vel;
	V2_float speed{ 30, 30 };
	static constexpr float zoom_speed{ 0.3f };

	ChunkManager chunk_manager;

	void Enter() override {
		FractalNoise fractal_noise;
		fractal_noise.SetOctaves(3);
		fractal_noise.SetFrequency(0.001f);
		fractal_noise.SetLacunarity(20.0f);
		fractal_noise.SetPersistence(0.8f);

		Application::Get().texture.Load("sheep", "resources/test.png");
		Application::Get().texture.Load("red", "resources/red_tile.png");
		Application::Get().texture.Load("blue", "resources/blue_tile.png");
		Application::Get().texture.Load("green", "resources/green_tile.png");

		chunk_manager.AddNoiseLayer(NoiseLayer{
			fractal_noise, [&](V2_float coordinate, float noise) {
				return CreateColorTile(
					-Application::Get().render_.GetGameSize() * 0.5f + coordinate, color::White.WithAlpha(noise)
				);
			} });

		sheep = CreateSheep(V2_float{ 0, 0 });
		StartFollow(camera, sheep);
	}

	void Update() override {
		MoveWASD(vel, speed, true);
		Translate(sheep, vel * Application::Get().dt());

		if (input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * Application::Get().dt());
		}
		if (input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * Application::Get().dt());
		}

		chunk_manager.Update(*this, camera);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ChunkScene", { 1280, 720 });
	Application::Get().scene_.Enter<ChunkScene>("");
	return 0;
}