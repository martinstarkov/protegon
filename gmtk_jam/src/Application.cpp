#include "core/Engine.h"
#include "renderer/Renderer.h"
#include "input/Input.h"
#include "text/Text.h"
#include "managers/FontManager.h"
#include "managers/TextureManager.h"
#include "math/Hash.h"

// TEMP:
#include "utility/Log.h"

using namespace ptgn;

#include <unordered_map>

enum class TileType {
	NONE = 0,
	PLAYER = 1,
};

struct Tile {
	TileType type{ TileType::NONE };
};

struct Grid {
	Grid(const V2_int& size, const V2_int& tile_size) : size{ size }, tile_size{ tile_size } {}
	~Grid() = default;
	void AddTile(const V2_int& coordinate, const Tile& tile) {
		assert(coordinate.x < size.x);
		assert(coordinate.y < size.y);
		tiles.emplace(coordinate, tile);
	}
	bool HasTile(const V2_int& coordinate) const {
		assert(coordinate.x < size.x);
		assert(coordinate.y < size.y);
		auto it = tiles.find(coordinate);
		return it != tiles.end();
	}
	const Tile& GetTile(const V2_int& coordinate) const {
		assert(coordinate.x < size.x);
		assert(coordinate.y < size.y);
		auto it = tiles.find(coordinate);
		assert(it != tiles.end());
		return it->second;
	}
	const V2_int& GetSize() const {
		return size;
	}
	const V2_int& GetTileSize() const {
		return tile_size;
	}
private:
	V2_int tile_size;
	V2_int size;
	std::unordered_map<V2_int, Tile> tiles;
};


inline V2_int ClosestAxis(const V2_double& direction) {
	auto dir{ direction.DotProduct(V2_double{ 1, 0 }) };
	V2_int axis{ 1, 0 };
	auto temp_dir{ direction.DotProduct(V2_double{ -1, 0 }) };
	if (temp_dir > dir) {
		dir = temp_dir;
		axis = { -1, 0 };
	}
	temp_dir = direction.DotProduct(V2_double{ 0, 1 });
	if (temp_dir > dir) {
		dir = temp_dir;
		axis = { 0, 1 };
	}
	temp_dir = direction.DotProduct(V2_double{ 0, -1 });
	if (temp_dir > dir) {
		dir = temp_dir;
		axis = { 0, -1 };
	}
	return axis;
}

#include <array>
#include <vector>
#include "math/RNG.h"

inline V2_int Sum(const std::vector<V2_int>& container) {
	V2_int sum;
	for (const auto& v : container)
		sum += v;
	return sum;
}
inline bool Contains(const std::vector<V2_int>& container, const V2_int& value) {
	for (const auto& v : container)
		if (v == value) return true;
	return false;
}

std::vector<V2_int> GetRandomRollSequence(std::size_t count) {
	std::vector<V2_int> sequence;
	sequence.push_back({});
	std::array<V2_int, 4> directions{ V2_int{ 1, 0 }, V2_int{ -1, 0 }, V2_int{ 0, 1 }, V2_int{ 0, -1 } };
	V2_int previous_direction{ directions[2] };
	math::RNG rng{ 0, 3 };
	for (auto i = 0; i < count; ++i) {
		start:
		auto dir = rng();
		auto current_direction = directions[dir] + sequence.back();
		if (directions[dir] != -previous_direction && !Contains(sequence, current_direction)) {
			sequence.emplace_back(current_direction);
			previous_direction = directions[dir];
		} else {
			goto start;
		}
	}
	return sequence;
}

class DiceGame : public Engine {
public:
	Grid grid{ { 10, 10 }, { 64, 64 } };
	V2_int grid_top_left_offset{ 32, 32 };
	V2_int dice_size{ 24 * 2, 24 * 2 };
	V2_int player_tile{ 1, 9 };
	std::vector<V2_int> sequence;
	managers::FontManager& font_manager = managers::GetManager<managers::FontManager>();
	managers::TextureManager& texture_manager = managers::GetManager<managers::TextureManager>();
	virtual void Init() override final {
		grid.AddTile(player_tile, Tile{ TileType::PLAYER });
		sequence = GetRandomRollSequence(6);
		font_manager.Load(0, "resources/fonts/retro_gaming.ttf", 32);
	}
	virtual void Update(double dt) override final {

		if (input::KeyDown(Key::R)) {
			sequence = GetRandomRollSequence(6);
		}

		auto mouse = input::GetMouseScreenPosition();
		auto player_position = grid_top_left_offset + player_tile * grid.GetTileSize() + grid.GetTileSize() / 2;
		auto direction = static_cast<V2_double>(mouse - player_position);
		auto axis_direction = ClosestAxis(direction);
		draw::Line(player_position, player_position + axis_direction * 50, color::RED);
		if (input::KeyDown(Key::SPACE)) {
			player_tile += axis_direction;
		}

		for (auto i = 0; i < grid.GetSize().x; ++i) {
			for (auto j = 0; j < grid.GetSize().x; j++) {
				V2_int tile_position{ i, j };
				draw::Rectangle(grid_top_left_offset + tile_position * grid.GetTileSize(), grid.GetTileSize(), color::BLACK);
				
				if (grid.HasTile(tile_position)) {
					auto tile = grid.GetTile(tile_position);
					//if (tile.type == TileType::)
				}
			}
		}

		for (auto i = 0; i < sequence.size(); ++i) {
			auto pos = grid_top_left_offset + (player_tile + sequence[i]) * grid.GetTileSize() + (grid.GetTileSize() - dice_size) / 2;
			draw::SolidRectangle(pos, dice_size, color::RED);
			auto key = math::Hash("temp_text");
			Text text{ key, 0, std::to_string(i).c_str(), color::YELLOW };
			draw::Texture(*texture_manager.Get(key), pos, dice_size);
		}

		draw::SolidRectangle(grid_top_left_offset + player_tile * grid.GetTileSize() + (grid.GetTileSize() - dice_size) / 2, dice_size, color::GREY);

	}
};

int main(int c, char** v) {
	DiceGame game;
	game.Start("Dice Game", V2_int{}, false, V2_int{}, window::Flags::NONE, true, true);
	return 0;
}