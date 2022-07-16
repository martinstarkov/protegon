#include "core/Engine.h"
#include "renderer/Renderer.h"
#include "input/Input.h"
#include "text/Text.h"
#include "managers/FontManager.h"
#include "managers/TextureManager.h"
#include "managers/SoundManager.h"
#include "math/Hash.h"
#include "math/RNG.h"

// TEMP:
#include "utility/Log.h"

using namespace ptgn;

#include <unordered_map>

enum class TileType {
	NONE = 0,
	PLAYER = 1,
	USED = 2,
	WIN = 3
};

struct Tile {
	TileType type{ TileType::NONE };
};

struct Grid {
	Grid(const V2_int& size, const V2_int& tile_size) : size{ size }, tile_size{ tile_size } {}
	~Grid() = default;
	bool InBound(const V2_int& coordinate) const {
		return coordinate.x < size.x && coordinate.x >= 0 &&
			   coordinate.y < size.y && coordinate.y >= 0;
	}
	void AddTile(const V2_int& coordinate, const Tile& tile) {
		assert(InBound(coordinate));
		tiles.emplace(coordinate, tile);
	}
	void AddTiles(const std::vector<V2_int>& sequence, const Tile& tile) {
		for (auto i = 0; i < sequence.size() - 1; ++i)
			AddTile(sequence[i], tile);
	}
	bool Permits(const std::vector<V2_int>& sequence) const {
		for (const auto& coordinate : sequence) {
			// If tile is being used or if tile is out of bounds.
			auto it = tiles.find(coordinate);
			if ((it != tiles.end() && it->second.type != TileType::WIN) ||
				!InBound(coordinate)) return false;
		}
		return true;
	}
	bool WinCondition(const std::vector<V2_int>& sequence) const {
		for (const auto& coordinate : sequence) {
			auto it = tiles.find(coordinate);
			if (it != tiles.end() && it->second.type == TileType::WIN) return true;
		}
		return false;
	}
	bool HasTile(const V2_int& coordinate) const {
		assert(InBound(coordinate));
		auto it = tiles.find(coordinate);
		return it != tiles.end();
	}
	const Tile& GetTile(const V2_int& coordinate) const {
		assert(InBound(coordinate));
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
	void Clear() {
		tiles.clear();
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
#include <tuple>
#include "math/RNG.h"

using Sequence = std::vector<V2_int>;
using Directions = std::vector<V2_int>;

inline bool Contains(const std::vector<V2_int>& container, const V2_int& value) {
	for (const auto& v : container)
		if (v == value) return true;
	return false;
}

Sequence GetRandomRollSequence(std::size_t count) {
	Sequence sequence;
	sequence.push_back({});
	std::array<V2_int, 4> directions{ V2_int{ 1, 0 }, V2_int{ -1, 0 }, V2_int{ 0, 1 }, V2_int{ 0, -1 } };
	V2_int previous_direction{ directions[0] };
	sequence.push_back(previous_direction);
	math::RNG<std::size_t> rng{ 0, 3 };
	for (auto i = 0; i < count - 1; ++i) {
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
	sequence.erase(sequence.begin());
	return sequence;
}

Sequence GetRotatedSequence(Sequence sequence, const double angle) {
	for (auto& vector : sequence)
		vector  = math::Round(vector.Rotate(angle));
	return sequence;
}
Sequence GetAbsoluteSequence(Sequence sequence, const V2_int& tile) {
	for (auto& vector : sequence)
		vector += tile;
	return sequence;
}

std::pair<Sequence, Directions> GetSequenceAndAllowedDirections(const Grid& grid, const std::size_t count, const V2_int& tile) {

	// TODO: Instead of doing 5000 attempts, just pick one out of all possible shapes.
	auto attempts = 0;
	Directions permitted_directions;
	Sequence sequence;
	while (permitted_directions.size() == 0 && attempts < 5000) {
		attempts++;
		sequence = GetRandomRollSequence(count);
		std::array<V2_int, 4> directions{ V2_int{ 1, 0 }, V2_int{ -1, 0 }, V2_int{ 0, 1 }, V2_int{ 0, -1 } };
		for (const auto& direction : directions) {
			auto rotated = GetRotatedSequence(sequence, direction.Angle());
			auto absolute = GetAbsoluteSequence(rotated, tile);
			if (grid.Permits(absolute)) {
				permitted_directions.emplace_back(direction);
			}
		}
	}
	return { sequence, permitted_directions };
}

class DiceGame : public Engine {
public:
	Grid grid{ { 20, 20 }, { 32, 32 } };
	V2_int grid_top_left_offset{ 32, 32 };
	V2_int dice_size{ 24, 24 };
	V2_int player_tile{ 1, 9 };
	V2_int win_tile{ 8, 8 };
	V2_int player_start_tile{ player_tile };
	math::RNG<std::size_t> dice_roll{ 1, 6 };
	Sequence sequence;
	Sequence absolute_sequence;
	Directions directions;
	managers::FontManager& font_manager = managers::GetManager<managers::FontManager>();
	managers::TextureManager& texture_manager = managers::GetManager<managers::TextureManager>();
	managers::SoundManager& sound_manager = managers::GetManager<managers::SoundManager>();
	std::size_t grid_key = math::Hash("grid");
	std::size_t choice_key = math::Hash("choice");
	std::size_t nochoice_key = math::Hash("nochoice");
	std::size_t used_key = math::Hash("used");
	std::size_t dice_key = math::Hash("dice");
	std::size_t win_key = math::Hash("win");
	std::size_t select_key = math::Hash("select");
	std::size_t move_key = math::Hash("move");
	std::size_t dice{ 1 };
	bool turn_allowed = false;
	bool game_over = false;
	bool generate_new = false;
	V2_int previous_direction;
	virtual void Init() override final {
		dice = dice_roll();
		auto pair = GetSequenceAndAllowedDirections(grid, dice, player_tile);
		sequence = pair.first;
		directions = pair.second;
		grid.AddTile(win_tile, Tile{ TileType::WIN });
		assert(pair.second.size() != 0 && "Could not find a valid starting positions, restart program");
		font_manager.Load(0, "resources/font/retro_gaming.ttf", 32);
		texture_manager.Load(grid_key, "resources/tile/grid.png");
		texture_manager.Load(choice_key, "resources/tile/choice.png");
		texture_manager.Load(nochoice_key, "resources/tile/nochoice.png");
		texture_manager.Load(used_key, "resources/tile/used.png");
		texture_manager.Load(dice_key, "resources/tile/dice.png");
		texture_manager.Load(win_key, "resources/tile/win.png");
		sound_manager.Load(select_key, "resources/sound/select_click.wav");
		sound_manager.Load(move_key, "resources/sound/move_click.wav");
		sound_manager.Load(win_key, "resources/sound/win.wav");
	}
	virtual void Update(double dt) override final {

		auto mouse = input::GetMouseScreenPosition();
		if (input::KeyDown(Key::R) || game_over) {
			grid.Clear();
			grid.AddTile(win_tile, Tile{ TileType::WIN });
			player_tile = player_start_tile;
			game_over = false;
			generate_new = true;
		}


		if (!game_over && generate_new) {
			generate_new = false;
			dice = dice_roll();
			auto pair = GetSequenceAndAllowedDirections(grid, dice, player_tile);
			sequence = pair.first;
			directions = pair.second;
		}

		game_over = directions.size() == 0;

		if (!game_over) {
			auto player_position = grid_top_left_offset + player_tile * grid.GetTileSize() + grid.GetTileSize() / 2;
			auto direction = static_cast<V2_double>(mouse - player_position);
			auto axis_direction = ClosestAxis(direction);

			if (previous_direction != axis_direction) {
				sound_manager.Get(move_key)->Play(-1, 0);
				previous_direction = axis_direction;
			}

			turn_allowed = Contains(directions, axis_direction);

			if (turn_allowed || previous_direction != axis_direction) {
				auto rotated = GetRotatedSequence(sequence, axis_direction.Angle());
				absolute_sequence = GetAbsoluteSequence(rotated, player_tile);
			}


			if (turn_allowed && input::KeyDown(Key::SPACE) && sequence.size() > 0) {
				grid.AddTile(player_tile, Tile{ TileType::USED });
				player_tile = absolute_sequence.back();
				grid.AddTiles(absolute_sequence, Tile{ TileType::USED });
				generate_new = true;
				if (grid.WinCondition(absolute_sequence)) {
					sound_manager.Get(win_key)->Play(-1, 0);
					game_over = true;
				} else {
					sound_manager.Get(select_key)->Play(-1, 0);
				}
			}

			for (auto i = 0; i < grid.GetSize().x; ++i) {
				for (auto j = 0; j < grid.GetSize().x; j++) {
					V2_int tile_position{ i, j };

					draw::Texture(*texture_manager.Get(grid_key), grid_top_left_offset + tile_position * grid.GetTileSize(), grid.GetTileSize());
				
					if (grid.HasTile(tile_position)) {
						auto tile = grid.GetTile(tile_position);
						if (tile.type == TileType::USED) {
							draw::Texture(*texture_manager.Get(used_key), grid_top_left_offset + tile_position * grid.GetTileSize(), grid.GetTileSize());
						} else if (tile.type == TileType::WIN) {
							draw::Texture(*texture_manager.Get(win_key), grid_top_left_offset + tile_position * grid.GetTileSize(), grid.GetTileSize());
						}
					}
				}
			}
			for (auto i = 0; i < absolute_sequence.size(); ++i) {
				auto pos = grid_top_left_offset + absolute_sequence[i] * grid.GetTileSize(); // + (grid.GetTileSize() - dice_size) / 2
				if (turn_allowed) {
					draw::Texture(*texture_manager.Get(choice_key), pos, grid.GetTileSize());
					auto key = math::Hash("temp_text");
					Text text{ key, 0, std::to_string(i + 1).c_str(), color::YELLOW };
					draw::Texture(*texture_manager.Get(key), pos + (grid.GetTileSize() - dice_size) / 2, dice_size);
				} else {
					auto rotated = GetRotatedSequence(sequence, axis_direction.Angle());
					absolute_sequence = GetAbsoluteSequence(rotated, player_tile);
					if (grid.InBound(absolute_sequence[i])) {
						auto pos = grid_top_left_offset + absolute_sequence[i] * grid.GetTileSize(); // + (grid.GetTileSize() - dice_size) / 2
						draw::Texture(*texture_manager.Get(nochoice_key), pos, grid.GetTileSize());
					}
				}
			}

			//auto player_dice = 1;
			draw::Texture(*texture_manager.Get(dice_key), grid_top_left_offset + player_tile * grid.GetTileSize(), grid.GetTileSize(), { 64 * (dice - 1), 0 }, { 64, 64 });
			//draw::SolidRectangle(grid_top_left_offset + player_tile * grid.GetTileSize() + (grid.GetTileSize() - dice_size) / 2, dice_size, color::GREY);

			draw::Line(player_position, player_position + axis_direction * 100, color::RED);
		}
	}
};

int main(int c, char** v) {
	DiceGame game;
	game.Start("Dice Game", V2_int{ 800, 800 }, true, V2_int{}, window::Flags::NONE, true, true);
	return 0;
}