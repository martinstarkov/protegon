#include "protegon/protegon.h"

using namespace ptgn;

struct WallComponent {};
struct StartComponent {};
struct EndComponent {};
struct DrawComponent {};
struct EnemyComponent {};
struct StaticComponent {};
struct TurretComponent {};
struct BulletComponent {};
struct ShooterComponent {
	ShooterComponent(int range, milliseconds delay) : range{ range }, delay{ delay } {}
	int range{ 0 };
	milliseconds delay{};
	void RestartCountdown() {
		reload.Start();
	}
	bool CanShoot() const {
		return !reload.IsRunning() || reload.Elapsed<milliseconds>() >= delay;
	}
private:
	Timer reload;
};
struct TextureComponent {
	TextureComponent(std::size_t key) : key{ key } {}
	std::size_t key{ 0 };
};
struct TileComponent {
	TileComponent(const V2_int& coordinate) : coordinate{ coordinate } {}
	V2_int coordinate;
};
struct VelocityComponent {
	VelocityComponent(float maximum, float initial = 0.0f) : maximum{ maximum }, velocity{ initial } {}
	float maximum{ 10.0f };
	float velocity{ 0.0f };
};
struct Velocity2DComponent {
	Velocity2DComponent(const V2_float& initial, const V2_float& maximum) : velocity{ initial }, maximum{ maximum } {}
	V2_float velocity;
	V2_float maximum{ 10.0f, 10.0f };
};
struct WaypointComponent {
	float current{ 0.0f };
};
struct LifeTimerComponent {
	LifeTimerComponent(milliseconds lifetime) : lifetime{ lifetime } {}
	void Start() {
		countdown.Start();
	}
	void Stop() {
		countdown.Stop();
	}
	bool IsDead() const {
		return countdown.Elapsed<milliseconds>() >= lifetime;
	}
	milliseconds lifetime{};
	Timer countdown;
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
		entity.Add<StaticComponent>();
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
		entity.Add<StaticComponent>();
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
		entity.Add<StaticComponent>();
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
		entity.Add<Rectangle<float>>(rect);
		entity.Add<EnemyComponent>();
		entity.Add<VelocityComponent>(10.0f, 5.0f);
		entity.Add<WaypointComponent>();
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateShooterTurret(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(j["turrets"]["shooter"]["texture_key"]);
		entity.Add<DrawComponent>();
		entity.Add<StaticComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<int>>(rect);
		entity.Add<TurretComponent>();
		entity.Add<ShooterComponent>(150, milliseconds{ 500 });
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateLaserTurret(const Rectangle<int>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<TextureComponent>(j["turrets"]["laser"]["texture_key"]);
		entity.Add<DrawComponent>();
		entity.Add<StaticComponent>();
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<int>>(rect);
		entity.Add<TurretComponent>();
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateBullet(const V2_float& start_position, const V2_float& normalized_direction) {
		auto entity = manager.CreateEntity();
		//entity.Add<TextureComponent>(j["turrets"]["laser"]["texture_key"]);
		entity.Add<DrawComponent>();
		entity.Add<Circle<float>>(Circle<float>{ start_position, 10.0f });
		entity.Add<BulletComponent>();
		entity.Add<Color>(color::GREY);
		entity.Add<Velocity2DComponent>(normalized_direction * 3.0f, V2_float{ 10.0f, 10.0f });
		entity.Add<LifeTimerComponent>(milliseconds{ 3000 }).Start();
		manager.Refresh();
		return entity;
	}
	
	ecs::Entity start;
	ecs::Entity end;
	std::deque<V2_int> waypoints;
	json j;
	int current_level{ 0 };
	int levels{ 0 };
	int current_wave{ 0 };
	int current_max_waves{ 0 };

	void DestroyTurrets() {
		manager.ForEachEntityWith<TurretComponent>([](auto& e, auto& t) {
			e.Destroy();
		});
		manager.Refresh();
	}

	void CreateTurrets() {
		auto& enemies = j["levels"][current_level]["waves"][current_wave]["enemies"];
		for (auto& enemy : enemies) {
			V2_int coordinate{ enemy["position"][0], enemy["position"][1] };
			Rectangle<int> rect{ coordinate * tile_size, tile_size };
			if (enemy["type"] == "shooter")
				CreateShooterTurret(rect, coordinate);
			else if (enemy["type"] == "laser")
				CreateLaserTurret(rect, coordinate);
		}
	}

	void Create() final {
		// Setup window configuration.
		window::SetColor(color::WHITE);
		window::Maximize();
		window::SetResizeable(true);

		// Load json data.
		std::ifstream f("resources/data/level_data.json");
		j = json::parse(f);
		levels = j["levels"].size();

		// Load textures.
		texture::Load(1001, "resources/tile/wall.png");
		texture::Load(1002, "resources/tile/start.png");
		texture::Load(1003, "resources/tile/end.png");
		texture::Load(1004, "resources/tile/enemy.png");
		texture::Load(1005, "resources/turret/shooter_turret.png");
		texture::Load(1006, "resources/turret/laser_turret.png");

		// Setup node grid for the map.
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
		// Calculate waypoints for the current map.
		waypoints = node_grid.FindWaypoints(start.Get<TileComponent>().coordinate, end.Get<TileComponent>().coordinate);
	
		// Create turrets for the current wave.
		current_max_waves = j["levels"][current_level]["waves"].size();
		CreateTurrets();
	}
	void Update(float dt) final {
		// Get mouse position on screen and tile grid.
		V2_int mouse_pos = input::GetMousePosition();
		Circle<int> mouse_circle{ mouse_pos, 30 };
		V2_int mouse_tile = mouse_pos / tile_size;
		Rectangle<int> mouse_box{ mouse_tile* tile_size, tile_size };

		bool new_level = false;

		if (input::KeyDown(Key::UP)) { // Increment level.
			current_level = ModFloor(current_level + 1, levels);
			new_level = true;
		} else if (input::KeyDown(Key::DOWN)) { // Decrement level.
			current_level = ModFloor(current_level - 1, levels);
			new_level = true;
		}

		if (new_level) { // Remake turrets and reset wave upon level change.
			current_max_waves = j["levels"][current_level]["waves"].size();
			current_wave = 0;
			DestroyTurrets();
			CreateTurrets();
		}

		bool new_wave = false;

		if (input::KeyDown(Key::Q)) { // Decrement wave.
			current_wave = ModFloor(current_wave - 1, current_max_waves);
			new_wave = true;
		} else if (input::KeyDown(Key::E)) { // Increment wave.
			current_wave = ModFloor(current_wave + 1, current_max_waves);
			new_wave = true;
		}

		if (new_wave) { // Remake turrets upon wave change.
			DestroyTurrets();
			CreateTurrets();
		}

		// Determine nearest enemy to a shooter turret and shoot a bullet toward it.
		manager.ForEachEntityWith<ShooterComponent, Rectangle<int>, TurretComponent>(
			[&](ecs::Entity& entity, ShooterComponent& s, Rectangle<int>& r, TurretComponent& t) {
			float closest_dist2 = INFINITY;
			float range2 = s.range * s.range;
			ecs::Entity closest_enemy = ecs::null;
			V2_float closest_dir;
			manager.ForEachEntityWith<Rectangle<float>, EnemyComponent>(
				[&](ecs::Entity& enemy, Rectangle<float>& enemy_r, EnemyComponent& e) {
				V2_float dir = enemy_r.Center() - r.Center();
				float dist2 = dir.MagnitudeSquared();
				if (dist2 < closest_dist2 && dist2 <= range2) {
					closest_dir = dir;
					closest_dist2 = dist2;
					closest_enemy = enemy;
				}
			});
			if (closest_enemy != ecs::null && s.CanShoot()) {
				s.RestartCountdown();
				CreateBullet(r.Center(), closest_dir.Normalized());
			}
		});
		
		// Create enemy when hitting C.
		if (input::KeyDown(Key::C)) {
			CreateEnemy(start.Get<Rectangle<int>>(), start.Get<TileComponent>().coordinate);
		}
		
		// Increase enemy velocity on right click.
		if (input::MouseDown(Mouse::LEFT)) {
			manager.ForEachEntityWith<VelocityComponent, EnemyComponent>([](
				auto& e, VelocityComponent& vel, EnemyComponent& enemy) {
				vel.velocity = std::min(vel.maximum, vel.velocity + 1.0f);
			});
		}

		// Decrease enemy velocity on right click.
		if (input::MouseDown(Mouse::RIGHT)) {
			manager.ForEachEntityWith<VelocityComponent, EnemyComponent>([](
				auto& e, VelocityComponent& vel, EnemyComponent& enemy) {
				vel.velocity = std::max(0.0f, vel.velocity - 1.0f);
			});
		}

		// Draw shooter tower range.
		manager.ForEachEntityWith<ShooterComponent, Rectangle<int>, TurretComponent>(
			[&](ecs::Entity& entity, ShooterComponent& s, Rectangle<int>& r, TurretComponent& t) {
			Circle<int> circle{ r.Center(), s.range };
			circle.DrawSolid(Color{ 128, 0, 0, 70 });
		});

		// Move bullet position forward by their velocity.
		manager.ForEachEntityWith<Circle<float>, Velocity2DComponent>([](
			auto& e, Circle<float>& c, Velocity2DComponent& v) {
			c.c += v.velocity;
		});

		// Draw circles.
		manager.ForEachEntityWith<DrawComponent, Circle<float>, Color>([](
			auto& e, DrawComponent& d, Circle<float>& c, Color& color) {
			c.DrawSolid(color);
		});

		// Draw static rectangular structures with textures.
		manager.ForEachEntityWith<Rectangle<int>, TextureComponent, DrawComponent, StaticComponent>([](
			ecs::Entity e, Rectangle<int>& rect,
			TextureComponent& texture, DrawComponent& draw, StaticComponent& s) {
			texture::Get(texture.key)->Draw(rect);
		});

		// Display node grid paths from start to finish.
		node_grid.DisplayWaypoints(waypoints, tile_size, color::PURPLE);

		// Move enemies along their path.
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
				// Linearly interpolate between the turret tile coordinate and the next one.
				rect.pos = Lerp(V2_float{ tile.coordinate * tile_size },
								V2_float{ (tile.coordinate +
										   waypoints[idx + 1] - waypoints[idx]) * tile_size },
								waypoint.current);
				texture::Get(texture.key)->Draw(rect);
			} else {
				// Destroy enemy when it reaches the end or when no path remains for it.
				e.Destroy();
			}
		});

		// Draw mouse hover square.
		if (node_grid.Has(mouse_tile))
			mouse_box.Draw(color::GOLD, 3);

		// Destroy enemies which run out of lifetime.
		manager.ForEachEntityWith<LifeTimerComponent>([](
			auto& e, LifeTimerComponent& l) {
			if (l.IsDead())
				e.Destroy();
		});

		manager.Refresh();
	}
};

int main(int c, char** v) {
	GMTKJam2023 game;
	game.Construct("GMTK Jam 2023", { 1080, 720 });
	return 0;
}