#pragma once

#include <unordered_map>

#include "inventory/Material.h"

struct TileSlot {
	const char* texture_key;
	int count{ 0 };
};

struct InventoryComponent {
	InventoryComponent() = default;
	std::unordered_map<Material, TileSlot> tiles;
	TileSlot GetTileSlot(Material type) {
		auto it{ tiles.find(type) };
		assert(it != tiles.end());
		return it->second;
	}
	void AddTile(Material type) {
		auto it{ tiles.find(type) };
		assert(it != tiles.end());
		++(it->second.count);
	}
	void RemoveTile(Material type) {
		auto it{ tiles.find(type) };
		assert(it != tiles.end());
		if (it->second.count > 0) {
			--(it->second.count);
		}
	}
};