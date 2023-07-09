#include "protegon/protegon.h"

using namespace ptgn;

struct WallComponent {};
struct StartComponent {};
struct EndComponent {};
struct DrawComponent {};
struct EnemyComponent {};
struct StaticComponent {};
struct ColliderComponent {};
struct TurretComponent {};
struct BulletComponent {};
struct LaserComponent {
	LaserComponent(milliseconds damage_delay) : damage_delay{ damage_delay } {}
	milliseconds damage_delay{};
	bool CanDamage() const {
		return !cooldown.IsRunning() || cooldown.Elapsed<milliseconds>() >= damage_delay;
	}
	Timer cooldown;
};
struct ShooterComponent {
	ShooterComponent(milliseconds delay) : delay{ delay } {}
	milliseconds delay{};
	bool CanShoot() const {
		return !reload.IsRunning() || reload.Elapsed<milliseconds>() >= delay;
	}
	Timer reload;
};
struct RangeComponent {
	RangeComponent(float range) : range{ range } {}
	float range{ 0.0f };
};
struct TargetComponent {
	TargetComponent(const ecs::Entity& target, milliseconds begin) : target{ target }, begin{ begin } {}
	ecs::Entity target;
	milliseconds begin{};
	Timer timer;
};
struct TextureComponent {
	TextureComponent(std::size_t key) : key{ key } {}
	TextureComponent(std::size_t key, int index) : key{ key }, index{ index } {}
	std::size_t key{ 0 };
	int index{ 0 };
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
struct DirectionComponent {
	enum Direction {
		DOWN = 0,
		RIGHT = 2,
		UP = 4,
		LEFT = 6,
	};
	DirectionComponent(Direction current = Direction::DOWN) : current{ current } {}
	void RecalculateCurrentDirection(const V2_int& normalized_direction) {
		if (normalized_direction != previous_direction) {
			if (normalized_direction.x < 0)
				current = LEFT;
			else if (normalized_direction.x > 0)
				current = RIGHT;
			else if (normalized_direction.y < 0)
				current = UP;
			else // Default direction.
				current = DOWN;
		}
		previous_direction = normalized_direction;
	}
	Direction current;
private:
	V2_int previous_direction;
};

struct Velocity2DComponent {
	Velocity2DComponent(const V2_float& initial_direction, float magnitude) :
		direction{ initial_direction }, magnitude{ magnitude } {}
	float magnitude{ 0.0f };
	V2_float direction;
};
struct WaypointComponent {
	float current{ 0.0f };
};
struct HealthComponent {
	HealthComponent(int start_health) : current{ start_health }, original{ current } {}
	int current{ 0 };
	// Returns true if the health was decreased by the given amount.
	bool Decrease(int amount) {
		int potential_new = current - amount;
		if (potential_new >= 0 && potential_new <= original) {
			current = potential_new;
			return true;
		}
		return false;
	}
	int GetOriginal() const {
		return original;
	}
	bool IsDead() const {
		return current <= 0;
	}
private:
	int original{ 0 };
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

enum class Enemy {
	REGULAR = 0,
	WIZARD = 1,
	ELF = 2,
	FAIRY = 3
};

struct ClosestInfo {
	ecs::Entity entity{ ecs::null };
	float distance2{ INFINITY };
	V2_float dir;
};

template <typename T>
ClosestInfo GetClosestInfo(ecs::Manager& manager, V2_float& position, float range) {
	float closest_dist2{ INFINITY };
	float range2{ range * range };
	ecs::Entity closest_target{ ecs::null};
	V2_float closest_dir;
	manager.ForEachEntityWith<Rectangle<float>, T>(
		[&](ecs::Entity& target, Rectangle<float>& target_r, T& e) {
		V2_float dir = target_r.Center() - position;
		float dist2 = dir.MagnitudeSquared();
		if (dist2 < closest_dist2 && dist2 <= range2) {
			closest_dir = dir;
			closest_dist2 = dist2;
			closest_target = target;
		}
	});
	return ClosestInfo{ closest_target, closest_dist2, closest_dir };
}

class GMTKJam2023 :  public Engine {
	Surface test_map{ "resources/maps/test_map.png" };
	V2_int grid_size{ 30, 15 };
	V2_int tile_size{ 32, 32 };
	V2_int map_size{ grid_size * tile_size };
	AStarGrid node_grid{ grid_size };
	ecs::Manager manager;
	ecs::Entity start;
	ecs::Entity end;
	std::deque<V2_int> waypoints;
	json j;
	int current_level{ 0 };
	int levels{ 0 };
	int current_wave{ 0 };
	int current_max_waves{ 0 };
	
	int max_queue_size{ 15 };
	std::deque<Enemy> enemy_queue;
	milliseconds enemy_release_delay{ 500 };
	Timer enemy_release_timer;

	ecs::Entity CreateWall(const Rectangle<float>& rect, const V2_int& coordinate, int key) {
		auto entity = manager.CreateEntity();
		entity.Add<WallComponent>();
		entity.Add<StaticComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TextureComponent>(key);
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateStart(const Rectangle<float>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<StartComponent>();
		entity.Add<StaticComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TextureComponent>(1002);
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateEnd(const Rectangle<float>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<EndComponent>();
		entity.Add<StaticComponent>();
		entity.Add<DrawComponent>();
		entity.Add<TextureComponent>(1003);
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		entity.Add<HealthComponent>(100);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateEnemy(const Rectangle<float>& rect, const V2_int& coordinate, Enemy index) {
		auto entity = manager.CreateEntity();
		entity.Add<DrawComponent>();
		entity.Add<ColliderComponent>();
		entity.Add<EnemyComponent>();
		entity.Add<WaypointComponent>();
		entity.Add<DirectionComponent>();
		entity.Add<TextureComponent>(2000, static_cast<int>(index));
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		entity.Add<HealthComponent>(50);
		entity.Add<VelocityComponent>(10.0f, 3.0f);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateShooterTurret(const Rectangle<float>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<DrawComponent>();
		entity.Add<TurretComponent>();
		entity.Add<StaticComponent>();
		entity.Add<ClosestInfo>();
		entity.Add<TextureComponent>(j["turrets"]["shooter"]["texture_key"]);
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		entity.Add<RangeComponent>(300.0f);
		entity.Add<ShooterComponent>(milliseconds{ 100 });
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateLaserTurret(const Rectangle<float>& rect, const V2_int& coordinate) {
		auto entity = manager.CreateEntity();
		entity.Add<DrawComponent>();
		entity.Add<TurretComponent>();
		entity.Add<LaserComponent>(milliseconds{ 50 });
		entity.Add<StaticComponent>();
		entity.Add<ClosestInfo>();
		entity.Add<TextureComponent>(j["turrets"]["laser"]["texture_key"]);
		entity.Add<TileComponent>(coordinate);
		entity.Add<Rectangle<float>>(rect);
		entity.Add<RangeComponent>(300.0f);
		manager.Refresh();
		return entity;
	}
	ecs::Entity CreateBullet(const V2_float& start_position, const V2_float& normalized_direction, ecs::Entity target = ecs::null) {
		auto entity = manager.CreateEntity();
		//entity.Add<TextureComponent>(j["turrets"]["laser"]["texture_key"]);
		entity.Add<DrawComponent>();
		entity.Add<BulletComponent>();
		entity.Add<ColliderComponent>();
		entity.Add<Circle<float>>(Circle<float>{ start_position, 5.0f });
		entity.Add<Color>(color::BLACK);
		entity.Add<TargetComponent>(target, milliseconds{ 3000 });
		entity.Add<Velocity2DComponent>(normalized_direction, 1000.0f);
		entity.Add<LifeTimerComponent>(milliseconds{ 6000 }).Start();
		manager.Refresh();
		return entity;
	}
	
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
			Rectangle<float> rect{ coordinate * tile_size, tile_size };
			if (enemy["type"] == "shooter")
				CreateShooterTurret(rect, coordinate);
			else if (enemy["type"] == "laser")
				CreateLaserTurret(rect, coordinate);
		}
	}

	void Create() final {

		music::Load(Hash("in_game"), "resources/music/in_game.wav");
		music::Get(Hash("in_game"))->Play(-1);
		// Setup window configuration.
		window::SetColor(color::BLACK);
		window::Maximize();
		window::SetResizeable(true);
		window::SetLogicalSize(map_size);

		// Load json data.
		std::ifstream f("resources/data/level_data.json");
		j = json::parse(f);
		levels = j["levels"].size();

		// Load textures.
		texture::Load(500, "resources/tile/wall.png");
		texture::Load(501, "resources/tile/top_wall.png");
		texture::Load(1002, "resources/tile/start.png");
		texture::Load(1003, "resources/tile/end.png");
		texture::Load(1004, "resources/tile/enemy.png");
		texture::Load(1005, "resources/turret/shooter_turret.png");
		texture::Load(1006, "resources/turret/laser_turret.png");
		texture::Load(2000, "resources/enemy/enemy.png");
		texture::Load(3000, "resources/ui/queue_frame.png");
		texture::Load(3001, "resources/ui/arrow.png");

		// Setup node grid for the map.
		test_map.ForEachPixel([&](const V2_int& coordinate, const Color& color) {
			V2_int position = coordinate * tile_size;
			Rectangle<float> rect{ position, tile_size };
			if (color == color::MAGENTA) {
				CreateWall(rect, coordinate, 501);
				node_grid.SetObstacle(coordinate, true);
			} else if (color == color::LIGHT_PINK) {
				CreateWall(rect, coordinate, 500);
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
		Rectangle<float> bg{ {}, window::GetLogicalSize() };
		bg.DrawSolid(color::WHITE);

		// Get mouse position on screen and tile grid.
		V2_int mouse_pos = input::GetMousePosition();
		Circle<float> mouse_circle{ mouse_pos, 30 };
		V2_int mouse_tile = V2_int{ V2_float{ mouse_pos } / V2_float{ tile_size } };
		Rectangle<float> mouse_box{ mouse_tile* tile_size, tile_size };

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

		// Determine nearest enemy to a turret.
		manager.ForEachEntityWith<RangeComponent, Rectangle<float>, TurretComponent, ClosestInfo>(
			[&](ecs::Entity& entity, RangeComponent& s, Rectangle<float>& r, TurretComponent& t, ClosestInfo& closest) {
			closest = GetClosestInfo<EnemyComponent>(manager, r.Center(), s.range);
			
		});

		// Shoot bullet from shooter turret if there is an enemy nearby.
		manager.ForEachEntityWith<RangeComponent, Rectangle<float>, TurretComponent, ClosestInfo, ShooterComponent>(
			[&](ecs::Entity& entity, RangeComponent& s, Rectangle<float>& r, TurretComponent& t, ClosestInfo& closest, ShooterComponent& shooter) {
			if (closest.entity.IsAlive()) {
				if (shooter.CanShoot()) {
					shooter.reload.Start();
					CreateBullet(r.Center(), closest.dir.Normalized(), closest.entity);
				}
			}
		});

		// Draw laser turret laser toward closest enemy.
		manager.ForEachEntityWith<RangeComponent, Rectangle<float>, TurretComponent, ClosestInfo, LaserComponent>(
			[&](ecs::Entity& entity, RangeComponent& s, Rectangle<float>& r, TurretComponent& t, ClosestInfo& closest, LaserComponent& laser) {
			if (closest.entity.IsAlive()) {
				if (laser.CanDamage()) {
					laser.cooldown.Start();
					if (closest.entity.Has<HealthComponent>()) {
						auto& h = closest.entity.Get<HealthComponent>();
						h.Decrease(1);
					}
				}
			}
		});

		// Add enemies to queue using number keys when enemies are not being released.
		static bool releasing_enemies = false;
		if (!releasing_enemies && enemy_queue.size() < max_queue_size) {
			if (input::KeyDown(Key::K_1)) {
				enemy_queue.push_back(Enemy::REGULAR);
			} else if (input::KeyDown(Key::K_2)) {
				enemy_queue.push_back(Enemy::WIZARD);
			} else if (input::KeyDown(Key::K_3)) {
				enemy_queue.push_back(Enemy::ELF);
			} else if (input::KeyDown(Key::K_4)) {
				enemy_queue.push_back(Enemy::FAIRY);
			}
		}

		// Hitting space triggers the emptying of the queue.
		if (input::KeyDown(Key::SPACE) && !releasing_enemies) {
			releasing_enemies = true;
		}

		if (releasing_enemies) {
			// Start the queue release timer.
			if (!enemy_release_timer.IsRunning())
				enemy_release_timer.Start();
			// Every time the delay has been passed, send one enemy from the queue.
			if (enemy_release_timer.Elapsed<milliseconds>() >= enemy_release_delay) {
				if (enemy_queue.size() > 0) {
					Enemy queue_element = enemy_queue.front();
					switch (queue_element) {
						// TODO: Will eventually break these up once enemies get custom mechanics.
						case Enemy::REGULAR:
						case Enemy::WIZARD:
						case Enemy::ELF:
						case Enemy::FAIRY:
						{
							CreateEnemy(start.Get<Rectangle<float>>(), start.Get<TileComponent>().coordinate, queue_element);
							break;
						}
					}
					enemy_queue.pop_front();
					enemy_release_timer.Reset();
				} else {
					// Once the queue is empty, stop the timer and stop releasing enemies.
					if (enemy_release_timer.IsRunning()) {
						enemy_release_timer.Reset();
						enemy_release_timer.Stop();
					}
					releasing_enemies = false;
				}
			}
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

		// Collide bullets with enemies, decrease health of enemies, and destroy bullets.
		manager.ForEachEntityWith<BulletComponent, Circle<float>, ColliderComponent>([&](
			auto& e, BulletComponent& d, Circle<float>& c, ColliderComponent& collider) {
			manager.ForEachEntityWith<Rectangle<float>, ColliderComponent, EnemyComponent>([&](
				auto& e2, Rectangle<float>& r2, ColliderComponent& c2, EnemyComponent& enemy2) {
				if (e.IsAlive() && overlap::CircleRectangle(c, r2)) {
					if (e2.Has<HealthComponent>()) {
						HealthComponent& h = e2.Get<HealthComponent>();
						h.Decrease(1);
					}
					e.Destroy();
				}
			});
		});

		// Draw shooter tower range.
		manager.ForEachEntityWith<RangeComponent, Rectangle<float>, TurretComponent>(
			[&](ecs::Entity& entity, RangeComponent& s, Rectangle<float>& r, TurretComponent& t) {
			Circle<float> circle{ r.Center(), s.range };
			circle.DrawSolid(Color{ 128, 0, 0, 70 });
		});

		// Move bullet position forward by their velocity.
		manager.ForEachEntityWith<Circle<float>, Velocity2DComponent>([&](
			auto& e, Circle<float>& c, Velocity2DComponent& v) {
			c.c += v.direction * v.magnitude * dt;
		});

		// Move targetted projectile bullets toward targets.
		manager.ForEachEntityWith<Circle<float>, Velocity2DComponent, TargetComponent>([](
			auto& e, Circle<float>& c, Velocity2DComponent& v, TargetComponent& t) {
			if (t.target.IsAlive()) {
				V2_float target_position;
				// TODO: Add generalized shape parent with position function.
				if (t.target.Has<Circle<float>>()) {
					target_position = t.target.Get<Circle<float>>().c;
				} else if (t.target.Has<Rectangle<float>>()) {
					target_position = t.target.Get<Rectangle<float>>().Center();
					assert((t.target.HasAny<Circle<float>, Rectangle<float>>()));
					v.direction = (target_position - c.c).Normalized();
				}
			}
		});

			// Draw static rectangular structures with textures.
		manager.ForEachEntityWith<Rectangle<float>, TextureComponent, DrawComponent, StaticComponent>([](
			ecs::Entity e, Rectangle<float>& rect,
			TextureComponent& texture, DrawComponent& draw, StaticComponent& s) {
			texture::Get(texture.key)->Draw(rect);
		});

		// Display node grid paths from start to finish.
		node_grid.DisplayWaypoints(waypoints, tile_size, color::PURPLE);

		// Move enemies along their path.
		manager.ForEachEntityWith<TileComponent, Rectangle<float>, TextureComponent,
			VelocityComponent, EnemyComponent, WaypointComponent, DirectionComponent>([&](
				ecs::Entity e, TileComponent& tile, Rectangle<float>& rect,
				TextureComponent& texture, VelocityComponent& vel,
				EnemyComponent& enemy, WaypointComponent& waypoint, DirectionComponent& dir) {
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
				V2_int direction = waypoints[idx + 1] - waypoints[idx];
				// Linearly interpolate between the turret tile coordinate and the next one.
				rect.pos = Lerp(V2_float{ tile.coordinate * tile_size },
					V2_float{ (tile.coordinate +
							   direction) * tile_size },
					waypoint.current);
				dir.RecalculateCurrentDirection(direction);
				Rectangle<float> source_rect{ V2_float{ static_cast<float>(dir.current), static_cast<float>(texture.index) } * tile_size, tile_size };
				texture::Get(texture.key)->Draw(rect, source_rect);
			} else {
				// Destroy enemy when it reaches the end or when no path remains for it.
				e.Destroy();
				// Decrease health of end tower by 1.
				assert(end.Has<HealthComponent>());
				HealthComponent& h = end.Get<HealthComponent>();
				h.Decrease(1);
				if (h.IsDead()) {
					// TODO: Lose condition.
				}
			}
		});

		// Draw bullet circles.
		manager.ForEachEntityWith<DrawComponent, Circle<float>, Color, BulletComponent>([](
			auto& e, DrawComponent& d, Circle<float>& c, Color& color, BulletComponent& b) {
			c.DrawSolid(color);
		});

		// Draw laser turret laser toward closest enemy.
		manager.ForEachEntityWith<RangeComponent, Rectangle<float>, TurretComponent, ClosestInfo, LaserComponent>(
			[&](ecs::Entity& entity, RangeComponent& s, Rectangle<float>& r, TurretComponent& t, ClosestInfo& closest, LaserComponent& laser) {
			if (closest.entity.IsAlive()) {
				assert(closest.entity.Has<Rectangle<float>>());
				Line<float> beam{ r.Center(), closest.entity.Get<Rectangle<float>>().Center() };
				beam.Draw(color::RED, 3);
			}
		});

		// Draw healthbars
		manager.ForEachEntityWith<Rectangle<float>, HealthComponent, EnemyComponent>(
			[&](auto& e, const Rectangle<float>& p, const HealthComponent& h, const EnemyComponent& ene) {
			assert(h.current >= 0);
			assert(h.current <= h.GetOriginal());
			float fraction{ 0.0f };
			if (h.GetOriginal() > 0)
				fraction = (float)h.current / h.GetOriginal();
			Rectangle<float> full_bar{ p.pos, V2_float{ 20, 2.0f } };
			full_bar = full_bar.Offset({ 6, 3 }, { 0, 0 });
			full_bar.DrawSolid(color::RED);
			Rectangle<float> remaining_bar{ full_bar };
			if (fraction >= 0.1f) { // Stop drawing green bar after health reaches below 1%.
				remaining_bar.size.x = full_bar.size.x * fraction;
				remaining_bar.DrawSolid(color::GREEN);
			}
		});

		V2_float queue_frame_size{ 28, 32 };
		const Rectangle<float> queue_frame{ { map_size.x / 2 - queue_frame_size.x * max_queue_size / 2, map_size.y - queue_frame_size.y }, queue_frame_size };

		// Draw border around queue frame.
		Rectangle<float> queue_frame_border = queue_frame.Offset({ -4, -4 }, { queue_frame.size.x * (max_queue_size - 1) + 8, 8 });
		queue_frame_border.Draw(color::DARK_BROWN, 6);
		queue_frame_border.Draw(color::BLACK, 3);
		
		// Draw queue.
		for (int i = 0; i < max_queue_size; i++) {
			texture::Get(3000)->Draw(queue_frame.Offset({ queue_frame.size.x * i, 0 }));
		}
		// Draw UI displaying enemies in queue.
		int facing_direction = 7; // characters point to the bottom left.
		for (int i = 0; i < enemy_queue.size(); i++) {
			Enemy type = enemy_queue[i];
			Rectangle<float> source_rect{V2_float{ static_cast<float>(facing_direction), static_cast<float>(type) } * tile_size, tile_size };
			texture::Get(2000)->Draw(queue_frame.Offset({ queue_frame.size.x * i, 0 }), source_rect);
		}
		// Draw arrow over first enemy in queue.
		if (enemy_queue.size() > 0) {
			V2_float arrow_size{ 15, 21 };
			Rectangle<float> arrow = queue_frame.Offset({ 0.0f, -arrow_size.y });
			texture::Get(3001)->Draw(arrow);
		}

		// Draw mouse hover square.
		if (overlap::PointRectangle(mouse_pos, bg) && node_grid.IsObstacle(mouse_tile))
			mouse_box.Draw(color::GOLD, 3);

		// Destroy enemies which run out of lifetime.
		manager.ForEachEntityWith<LifeTimerComponent>([](
			auto& e, LifeTimerComponent& l) {
			if (l.IsDead())
				e.Destroy();
		});

		// Destroy enemies which run out of health.
		manager.ForEachEntityWith<HealthComponent>([](
			auto& e, HealthComponent& h) {
			if (h.IsDead())
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