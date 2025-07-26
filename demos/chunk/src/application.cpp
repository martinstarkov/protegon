
#include <string_view>
#include <vector>

#include "components/common.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "math/geometry.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/rect.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "tile/chunk.h"

using namespace ptgn;

class ChunkScene : public Scene {
public:
	Entity CreateSheep(const V2_float& position) {
		auto e = CreateEntity();
		e.SetPosition(position);
		e.Show();
		e.SetDepth(1);
		e.Add<TextureHandle>("sheep");
		return e;
	}

	Entity CreateTile(const V2_float& position, std::string_view texture_key) {
		auto e = CreateEntity();
		e.SetPosition(position);
		e.Show();
		e.SetOrigin(Origin::TopLeft);
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
				return CreateColorTile(coordinate, color::White.WithAlpha(noise));
			} });

		sheep = CreateSheep(V2_float{ 0, 0 });
		camera.primary.StartFollow(sheep);
	}

	void Update() override {
		MoveWASD(vel, speed, true);
		sheep.GetPosition() += vel * game.dt();

		if (game.input.KeyPressed(Key::Q)) {
			camera.primary.Zoom(-zoom_speed * game.dt());
		}
		if (game.input.KeyPressed(Key::E)) {
			camera.primary.Zoom(zoom_speed * game.dt());
		}

		chunk_manager.Update(*this, camera.primary);
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ChunkScene", { 1280, 720 }, color::Transparent);
	game.scene.Enter<ChunkScene>("");
	return 0;
}