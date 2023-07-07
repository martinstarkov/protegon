#include "protegon/protegon.h"

using namespace ptgn;

struct WallComponent {};
struct StartComponent {};
struct EndComponent {};
struct DrawComponent {};
struct EnemyComponent {};
struct TextureComponent {
	TextureComponent(std::size_t key) : key{ key } {}
	std::size_t key{ 0 };
};
struct TileComponent {
	TileComponent(const V2_int& coordinate) : coordinate{ coordinate } {}
	V2_int coordinate;
};
struct VelocityComponent {
	float velocity{ 0.0f };
};
struct WaypointComponent {
	float current{ 0.0f };
};

class GMTKJam2023 :  public Engine {
	Surface test_map{ "resources/maps/test_map.png" };
	V2_int grid_size{ 60, 33 };
	V2_int tile_size{ 32, 32 };
	AStarGrid node_grid{ grid_size };
	ecs::Manager manager;
	ecs::Entity CreateWall(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(1001);
		entity.Add<WallComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<int>>(rect);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateStart(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(1002);
		entity.Add<StartComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<int>>(rect);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateEnd(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(1003);
		entity.Add<EndComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<int>>(rect);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateEnemy(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(1004);
		entity.Add<DrawComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(Rectangle<float>{ rect.pos, rect.size });
		entity.Add<EnemyComponent>();
		entity.Add<VelocityComponent>();
		entity.Add<WaypointComponent>();
		manager.Refresh();
		return entity;
	}
	ecs::Entity start;
	ecs::Entity end;
	std::deque<V2_int> waypoints;
	void Create() final {
		window::SetColor(color::WHITE);
		window::Maximize();
		window::SetResizeable(true);
		texture::Load(1001, "resources/tile/wall.png");
		texture::Load(1002, "resources/tile/start.png");
		texture::Load(1003, "resources/tile/end.png");
		texture::Load(1004, "resources/tile/enemy.png");
		test_map.ForEachPixel([&](const V2_int& coordinate, const Color& color) {
			V2_int position = coordinate * tile_size;
			Rectangle<int> rect{ position, tile_size };
			if (color == color::MAGENTA) {
				CreateWall(rect, coordinate);
				node_grid.SetObstacle(coordinate, true);
			} else if (color == color::BLUE) {
				start = CreateStart(rect, coordinate);
			} else if (color == color::LIME) {
				end = CreateEnd(rect, coordinate);
			}
		});
		assert(start.Has<TileComponent>());
		assert(end.Has<TileComponent>());
		waypoints = node_grid.FindWaypoints(start.Get<TileComponent>().coordinate, end.Get<TileComponent>().coordinate);
	}
	void Update(float dt) final {
		V2_int mouse_pos = input::GetMousePosition();
		Circle<int> mouse_circle{ mouse_pos, 30 };
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size };
		
		if (input::KeyDown(Key::C)) {
			CreateEnemy(start.Get<Rectangle<int>>(), start.Get<TileComponent>().coordinate);
		}

		if (input::MouseDown(Mouse::LEFT)) {
			manager.ForEachEntityWith<VelocityComponent, EnemyComponent>([](
				auto& e, VelocityComponent& vel, EnemyComponent& enemy) {
				vel.velocity += 1.0f;
			});
		}
		if (input::MouseDown(Mouse::RIGHT)) {
			manager.ForEachEntityWith<VelocityComponent, EnemyComponent>([](
				auto& e, VelocityComponent& vel, EnemyComponent& enemy) {
				vel.velocity = std::clamp(vel.velocity, 0.0f, std::max(0.0f, vel.velocity - 1.0f));
			});
		}


		manager.ForEachEntityWith<Rectangle<int>, TextureComponent, DrawComponent, WallComponent>([](
			ecs::Entity e, Rectangle<int>& rect,
			TextureComponent& texture, DrawComponent& draw, WallComponent& wall) {
			texture::Get(texture.key)->Draw(rect);
		});
		node_grid.DisplayWaypoints(waypoints, tile_size, color::PURPLE);
		manager.ForEachEntityWith<TileComponent, Rectangle<float>, TextureComponent, VelocityComponent, EnemyComponent, WaypointComponent>([&](
			ecs::Entity e, TileComponent& tile, Rectangle<float>& rect, 
			TextureComponent& texture, VelocityComponent& vel,
			EnemyComponent& enemy, WaypointComponent& waypoint) {
			bool path_exists = tile.coordinate != end.Get<TileComponent>().coordinate;
			int idx = -1;
			if (path_exists) {
				idx = AStarGrid::FindWaypointIndex(waypoints, tile.coordinate);
			}
			path_exists = idx != -1;
			if (path_exists) {
				waypoint.current += dt * vel.velocity;
				assert(idx >= 0);
				assert(idx < waypoints.size());
				assert(idx + 1 < waypoints.size());
				// Keep moving character 1 tile forward on its path
				// until there is no longer enough "speed" for 1 full tile
				// in which case exit the loop and linearly interpolate
				// the position between the "in progress" tiles.
				while (waypoint.current >= 1.0f && idx + 1 < waypoints.size()) {
					tile.coordinate += waypoints[idx + 1] - waypoints[idx];
					waypoint.current -= 1.0f;
					idx++;
				}
			}
			if (path_exists && idx + 1 < waypoints.size()) {
				assert(waypoint.current <= 1.0f);
				assert(waypoint.current >= 0.0f);
				assert(idx >= 0);
				assert(idx < waypoints.size());
				assert(idx + 1 < waypoints.size());
				rect.pos = Lerp(V2_float{ tile.coordinate * tile_size },
								V2_float{ (tile.coordinate +
										   waypoints[idx + 1] - waypoints[idx]) * tile_size },
								waypoint.current);
				texture::Get(texture.key)->Draw(rect);
			} else {
				texture::Get(texture.key)->Draw(rect);
			}
		});

		if (node_grid.Has(mouse_tile))
			mouse_box.Draw(color::GOLD, 3);
	}
};

int main(int c, char** v) {
	GMTKJam2023 game;
	game.Construct("GMTK Jam 2023", { 1080, 720 });
	return 0;
}