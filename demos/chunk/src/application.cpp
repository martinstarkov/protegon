
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "math/noise.h"
#include "physics/movement.h"
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
		e.Add<TextureKey>("sheep");
		return e;
	}

	ecs::Entity CreateTile(ecs::Manager& m, const V2_float& position) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<TextureKey>("tile");
		return e;
	}

	ecs::Entity sheep;

	void Exit() override {}

	V2_float vel;

	ChunkManager chunk_manager;

	void Enter() override {
		FractalNoise fractal_noise;
		fractal_noise.SetOctaves(2);
		fractal_noise.SetFrequency(0.055f);
		fractal_noise.SetLacunarity(5);
		fractal_noise.SetPersistence(3);

		game.texture.Load("sheep", "resources/test.png");
		game.texture.Load("tile", "resources/tile.png");

		CreateTile(manager, V2_float{ 0, 0 });

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		camera.primary.StartFollow(sheep);
	}

	void Update() override {
		MoveWASD(vel, { 10, 10 }, true);
		sheep.Get<Transform>().position += vel * game.dt();

		chunk_manager.Update(camera.primary);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Chunk", { 1280, 720 }, color::Transparent);
	game.scene.Enter<ChunkScene>("chunk");
	return 0;
}