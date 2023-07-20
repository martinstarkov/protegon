#include "protegon/protegon.h"

#include <array> // std::array

using namespace ptgn;

struct HealthComponent {
	HealthComponent() = default;
	HealthComponent(int health) : current{ health }, original{ health } {}
	int current{ 0 };
	int GetOriginal() const {
		return original;
	}
private:
	int original{ 0 };
};

struct PathComponent {
	PathComponent() = default;
	PathComponent(const std::deque<V2_int>& waypoints) : waypoints{ waypoints } {}
	std::deque<V2_int> waypoints;
	float current_waypoint{ 0.0f };
};

struct PositionComponent {
	PositionComponent() = default;
	PositionComponent(const V2_int& pos) : pos{ pos } {}
	V2_int pos;
	V2_float point;
};

struct VelocityComponent {
	VelocityComponent() = default;
	VelocityComponent(const float vel) : vel{ vel } {}
	float vel{ 0.0f };
};

struct TurretComponent {
	TurretComponent() = delete;
	TurretComponent(const int key) : key{ key } {}
	int key{ 0 };
};

struct TextureComponent {
	TextureComponent() = delete;
	TextureComponent(const std::size_t key) : key{ key } {}
	std::size_t key{ 0 };
};

void DrawHealthbars(ecs::Manager& manager, bool moving, const V2_int& tile_size) {
	manager.ForEachEntityWith<PositionComponent, HealthComponent>(
		[&](ecs::Entity e, const PositionComponent& p, const HealthComponent& h) {
		assert(h.current >= 0);
		assert(h.current <= h.GetOriginal());
		float fraction{ 0.0f };
		if (h.GetOriginal() > 0) {
			fraction = (float)h.current / h.GetOriginal();
		}
		V2_int pos{ p.point };
		if (!moving)
			pos = p.pos * tile_size;
		Rectangle<int> full_bar{ pos, { 28, 5 } };
		full_bar.pos.x -= 4;
		full_bar.pos.y -= 10;
		full_bar.DrawSolid(color::RED);
		Rectangle<int> remaining_bar{ full_bar };
		remaining_bar.size.x = full_bar.size.x * fraction;
		remaining_bar.DrawSolid(color::GREEN);
	});
}

void RegulateHealthbars(ecs::Manager& manager) {
	bool down{ input::KeyPressed(Key::DOWN) };
	bool up{ input::KeyPressed(Key::UP) };
	if (up || down) {
		int sign = 1;
		if (down) sign = -1;
		manager.ForEachEntityWith<HealthComponent>([&](ecs::Entity e, HealthComponent& h) {
			int potential_new = h.current + sign;
			if (potential_new >= 0 &&
				potential_new <= h.GetOriginal())
				h.current = potential_new;
		});
	}
}

class TowerDefense :  public Engine {
	AStarGrid grid{ { 50, 30 } };
	V2_int tile_size{ 20, 20 };
	V2_int start;
	V2_int end;
	std::deque<V2_int> global_waypoints;
	ecs::Manager enemy_manager;
	ecs::Entity enemy1;
	int selected_slot{ 0 };
	static constexpr int turret_count{ 9 };
	std::array<ecs::Entity, turret_count> turrets{};
	std::array<std::pair<const char*, std::size_t>, turret_count> turret_resources;
	ecs::Manager turret_manager;
	Grid<ecs::Entity> entity_grid{ { 50, 30 } };
	//ecs::Manager turret_manager;


	void Create() final {
		turret_resources[0] = { "resources/turrets/1.png", 2001 };
		turret_resources[1] = { "resources/turrets/2.png", 2002 };
		turret_resources[2] = { "resources/turrets/3.png", 2003 };
		turret_resources[3] = { "resources/turrets/4.png", 2004 };
		turret_resources[4] = { "resources/turrets/5.png", 2005 };
		turret_resources[5] = { "resources/turrets/6.png", 2006 };
		turret_resources[6] = { "resources/turrets/7.png", 2007 };
		turret_resources[7] = { "resources/turrets/8.png", 2008 };
		turret_resources[8] = { "resources/turrets/9.png", 2009 };
		
		texture::Load(3000, "resources/ui/inventory_slot.png");
		texture::Load(2000, "resources/tile/thick_nochoice.png");

		//turret_manager.Refresh();
		
		for (auto i = 0; i < turret_resources.size(); ++i) {
			texture::Load(turret_resources[i].second, turret_resources[i].first);
			auto e = turret_manager.CreateEntity();
			e.Add<TextureComponent>(turret_resources[i].second);
			e.Add<TurretComponent>(i);
			turrets[i] = e;
		}
		
		for (int i = 0; i < entity_grid.GetSize().x; i++) {
			for (int j = 0; j < entity_grid.GetSize().y; j++) {
				entity_grid.Set({ i, j }, turret_manager.CreateEntity());
			}
		}

		turret_manager.Refresh();
		
		start = { 1, grid.GetSize().y / 2 };
		end = { grid.GetSize().x - 6, grid.GetSize().y / 2 };

		/*
		enemy1 = enemy_manager.CreateEntity();

		enemy_manager.Refresh();

		enemy1.AddComponent<PositionComponent>().pos = start;
		enemy1.AddComponent<VelocityComponent>().vel = 7.0f;
		enemy1.AddComponent<PathComponent>();
		enemy1.AddComponent<HealthComponent>(100);
		*/
	}
	void Update(float dt) final {
		//turret_manager.Refresh();
		
		//auto& enemy1_p = enemy1.GetComponent<PositionComponent>();
		//auto& enemy1_path = enemy1.GetComponent<PathComponent>();

		V2_int mouse_pos{ input::GetMousePosition() };
		V2_int mouse_tile{ mouse_pos / tile_size };
		Rectangle<int> mouse_box{ mouse_tile * tile_size, tile_size };

		/*
		if (input::MousePressed(Mouse::RIGHT)) {
			if (grid.SetObstacle(mouse_tile, false)) {
				global_waypoints = grid.FindWaypoints(start, end);
				assert(entity_grid.Has(mouse_tile));
				auto& e = *entity_grid.Get(mouse_tile);
				// TODO: Fix memory leak here?
				e.RemoveComponents();
			}
		}
		*/
		/*
		if (input::MousePressed(Mouse::LEFT)) {
			if (grid.Has(mouse_tile)) {
				if (input::KeyPressed(Key::LEFT_SHIFT)) {
					start = mouse_tile;
					enemy1_p.pos = start;
					global_waypoints = grid.FindWaypoints(start, end);
				} else if (input::KeyPressed(Key::LEFT_CTRL)) {
					end = mouse_tile;
					global_waypoints = grid.FindWaypoints(start, end);
				} else if (grid.SetObstacle(mouse_tile, true)) {
					assert(entity_grid.Has(mouse_tile));
					auto& e = *entity_grid.Get(mouse_tile);
					e.RemoveComponents();
					//e = turret_manager.CopyEntity(turrets[selected_slot]);
					e.AddComponent<TextureComponent>(turrets[selected_slot].GetComponent<TextureComponent>().key);
					e.AddComponent<TurretComponent>(selected_slot);
					global_waypoints = grid.FindWaypoints(start, end);
				}
			}
		}
		*/
		//turret_manager.Refresh();

		//auto bg_tile = texture::Get(2000);
		/*
		grid.ForEach([&](const V2_int& tile) {
			Rectangle<int> r{ tile * tile_size, tile_size };
			Color c;
			assert(entity_grid.Has(tile));
			auto& e = *entity_grid.Get(tile);
			if (input::KeyPressed(Key::V) && grid.IsVisited(tile))
				c = color::CYAN;
			else if (tile == start)
				c = color::GREEN;
			else if (tile == end)
				c = color::GOLD;
			else if (e.HasComponent<TurretComponent>()) {
				assert(e.HasComponent<TextureComponent>());
				auto turret_id = e.GetComponent<TurretComponent>().id;
				texture::Get(turrets[turret_id].GetComponent<TextureComponent>().key)->Draw(r);
				return;
			} else {
				bg_tile->Draw(r);
				return;
			}
			r.DrawSolid(c);
		});
		*/
		if (grid.Has(mouse_tile))
			mouse_box.Draw(color::YELLOW);

		/*
		enemy1_path.waypoints = global_waypoints;
		int idx = AStarGrid::FindWaypointIndex(enemy1_path.waypoints, enemy1_p.pos);
		// path is obviously finished if character is at the end tile.
		bool path_exists = enemy1_p.pos != end;
		if (idx == -1 && path_exists) { // look for a local path if the character is not on the global path or at the end
			enemy1_path.waypoints = grid.FindWaypoints(enemy1_p.pos, end);
			
			idx = AStarGrid::FindWaypointIndex(enemy1_path.waypoints, enemy1_p.pos);
			path_exists = idx != -1;
		}
		*/
		//AStarGrid::DisplayWaypoints(enemy1_path.waypoints, tile_size, color::PURPLE);
		//AStarGrid::DisplayWaypoints(global_waypoints, tile_size, color::GREEN);
		/*
		if (path_exists) { // global or local path exists
			enemy1_path.current_waypoint += dt * enemy1.GetComponent<VelocityComponent>().vel;
			assert(idx >= 0);
			assert(idx < enemy1_path.waypoints.size());
			assert(idx + 1 < enemy1_path.waypoints.size());
			// Keep moving character 1 tile forward on its path
			// until there is no longer enough "speed" for 1 full tile
			// in which case exit the loop and linearly interpolate
			// the position between the "in progress" tiles.
			while (enemy1_path.current_waypoint >= 1.0f && idx + 1 < enemy1_path.waypoints.size()) {
				enemy1_p.pos += enemy1_path.waypoints[idx + 1] - enemy1_path.waypoints[idx];
				enemy1_path.current_waypoint -= 1.0f;
				idx++;
			}
		}
		*/
		//bool moving{ path_exists && idx + 1 < enemy1_path.waypoints.size() };
		/*
		if (moving) {
			assert(enemy1_path.current_waypoint <= 1.0f);
			assert(enemy1_path.current_waypoint >= 0.0f);
			assert(idx >= 0);
			assert(idx < enemy1_path.waypoints.size());
			assert(idx + 1 < enemy1_path.waypoints.size());
			enemy1_p.point = Lerp(V2_float{ enemy1_p.pos * tile_size }, V2_float{ (enemy1_p.pos + enemy1_path.waypoints[idx + 1] - enemy1_path.waypoints[idx]) * tile_size }, enemy1_path.current_waypoint);
			Rectangle<int> enemy{ V2_int{ enemy1_p.point }, tile_size };
			enemy.DrawSolid(color::PURPLE);
		} else {
			Rectangle<int> enemy{ enemy1_p.pos * tile_size, tile_size };
			enemy.DrawSolid(color::PURPLE);
		}
		*/
		auto t = texture::Get(3000);
		
		Rectangle<int> s{ { window::GetSize().x - 32 - 3, 140 }, { 32, 32 } };
		for (int i = 0; i < turrets.size(); i++) {
			auto r = s.Offset({ 0, s.size.y * i });
			t->Draw(r);
			auto key = turrets[i].Get<TextureComponent>().key;
			texture::Get(key)->Draw(r.Offset({ 4, 4 }, { -8, -8 }));
		}
		int scroll{ input::MouseScroll() };
		if (scroll) {
			selected_slot -= scroll;
			selected_slot = ModFloor(selected_slot, turrets.size());
		}
		Rectangle<int> o{ { window::GetSize().x - 32 - 3, 140 }, { 32, 32 } };
		o.pos.y += o.size.y * selected_slot;
		o.Draw(color::BLACK, 4);
		
		//RegulateHealthbars(enemy_manager);

		//DrawHealthbars(enemy_manager, moving, tile_size);

		PrintLine(debug::CurrentUsage());
	}
};

int main(int c, char** v) {
	TowerDefense game;
	game.Construct("Tower Defense", { 1000, 600 });
	return 0;
}
