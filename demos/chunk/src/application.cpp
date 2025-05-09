
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "math/geometry.h"
#include "math/noise.h"
#include "components/movement.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "serialization/file_stream_reader.h"
#include "serialization/file_stream_writer.h"
#include "tile/chunk.h"
#include "utility/file.h"
#include "utility/log.h"

using namespace ptgn;

class ChunkScene : public Scene {
public:
	ecs::Entity CreateSheep(ecs::Manager& m, const V2_float& position) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<Depth>(1);
		e.Add<TextureKey>("sheep");
		return e;
	}

	ecs::Entity CreateTile(
		ecs::Manager& m, const V2_float& position, std::string_view texture_key
	) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<Origin>(Origin::TopLeft);
		e.Add<TextureKey>(texture_key);
		return e;
	}

	ecs::Entity CreateColorTile(ecs::Manager& m, const V2_float& position, const Color& color) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<Rect>(chunk_manager.tile_size, Origin::TopLeft);
		e.Add<Tint>(color);
		return e;
	}

	ecs::Entity sheep;

	void Exit() override {}

	V2_float vel;

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

		chunk_manager.noise_layers.emplace_back(
			fractal_noise,
			[&](const V2_float& coordinate, float noise) {
				return CreateColorTile(manager, coordinate, color::White.WithAlpha(noise));
			}
		);

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		camera.primary.StartFollow(sheep);
	}

	void Update() override {
		MoveWASD(vel, { 3, 3 }, true);
		sheep.Get<Transform>().position += vel * game.dt();

		constexpr float zoom_speed{ 0.1f };

		if (game.input.KeyPressed(Key::Q)) {
			camera.primary.Zoom(-zoom_speed * game.dt());
		}
		if (game.input.KeyPressed(Key::E)) {
			camera.primary.Zoom(zoom_speed * game.dt());
		}

		chunk_manager.Update(camera.primary);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Chunk", { 1280, 720 }, color::Transparent);
	game.scene.Enter<ChunkScene>("chunk");
	return 0;
}