
#include "runtime/world/chunk.h"

#include <chrono>
#include <string_view>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/key.h"
#include "core/math/geometry/origin.h"
#include "core/math/noise.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "runtime/animation/tween_effect.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/physics/movement.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

class ChunkScene : public Scene {
public:
	Entity CreatePlayer(V2_float position) {
		auto e = CreateSprite(*this, "white_smile", position);
		SetDepth(e, 1);
		return e;
	}

	Entity CreateTile(V2_float position, std::string_view texture_key) {
		auto e = CreateSprite(*this, texture_key, position, Origin::TopLeft);
		return e;
	}

	Entity CreateColorTile(V2_float position, const Color& color) {
		auto e =
			CreateRect(*this, position, chunk_manager.tile_size, color, Solid{}, Origin::TopLeft);
		return e;
	}

	Entity player;

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

		ctx().asset.LoadTexture("white_smile", "assets/white_smile.png");
		ctx().asset.LoadTexture("red", "assets/red_tile.png");
		ctx().asset.LoadTexture("blue", "assets/blue_tile.png");
		ctx().asset.LoadTexture("green", "assets/green_tile.png");

		chunk_manager.AddNoiseLayer(NoiseLayer{
			fractal_noise, [&](V2_float coordinate, float noise) {
				return CreateColorTile(
					-ctx().renderer.GetGameSize() * 0.5f + coordinate, color::White.WithAlpha(noise)
				);
			} });

		player = CreatePlayer(V2_float{ 0, 0 });
		StartFollow(ctx().camera, player);
	}

	void OnUpdate() override {
		float dt{ ctx().dt().count() };

		MoveWASD(*this, vel, speed * dt, true);

		Translate(player, vel * dt);

		if (ctx().input.KeyHeld(Key::Q)) {
			ctx().camera.Zoom(-zoom_speed * dt);
		}
		if (ctx().input.KeyHeld(Key::E)) {
			ctx().camera.Zoom(zoom_speed * dt);
		}

		chunk_manager.Update(*this, ctx().camera.operator Camera());
	}
};

int main(int, char**) {
	Application app{ "ChunkScene", { 1280, 720 } };
	app.StartWith<ChunkScene>();
}