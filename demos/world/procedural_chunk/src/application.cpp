
#include <string_view>

#include "core/ecs/components/draw.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/app/application.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/materials/texture.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"
#include "world/tile/chunk.h"
#include "tweens/tween_effects.h"

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

	void Enter() override {
		FractalNoise fractal_noise;
		fractal_noise.SetOctaves(3);
		fractal_noise.SetFrequency(0.001f);
		fractal_noise.SetLacunarity(20.0f);
		fractal_noise.SetPersistence(0.8f);

		game.texture.Load("sheep", "resources/test.png");
		game.texture.Load("red", "resources/red_tile.png");
		game.texture.Load("blue", "resources/blue_tile.png");
		game.texture.Load("green", "resources/green_tile.png");

		chunk_manager.AddNoiseLayer(NoiseLayer{
			fractal_noise, [&](const V2_float& coordinate, float noise) {
				return CreateColorTile(
					-game.renderer.GetGameSize() * 0.5f + coordinate, color::White.WithAlpha(noise)
				);
			} });

		sheep = CreateSheep(V2_float{ 0, 0 });
		StartFollow(camera, sheep);
	}

	void Update() override {
		MoveWASD(vel, speed, true);
		Translate(sheep, vel * game.dt());

		if (input.KeyPressed(Key::Q)) {
			camera.Zoom(-zoom_speed * game.dt());
		}
		if (input.KeyPressed(Key::E)) {
			camera.Zoom(zoom_speed * game.dt());
		}

		chunk_manager.Update(*this, camera);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ChunkScene", { 1280, 720 });
	game.scene.Enter<ChunkScene>("");
	return 0;
}