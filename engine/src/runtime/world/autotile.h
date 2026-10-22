#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "core/math/vector2.h"
#include "runtime/asset/asset_key.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

/// @brief An atlas's tile order is part of the format. Pixels are in row major order.
/// For 5x3 15 piece sheets, the empty corner mask is omitted (indices 0..14 = masks 1..15).
/// 4x4 Classic15 sheets may include the empty mask at slot zero.
/// For 47 piece sheets, the indices enumerate valid eight neighbor masks in ascending order.
/// Blob bits are N=1, NE=2, E=4, SE=8, S=16, SW=32, W=64, NW=128.
/// Four neighbor sheets use N=1, E=2, S=4, W=8.
/// Corner sheets use top left=1, top right=2, bottom left=4, bottom right=8.
enum class TilemapAutotileFormat : std::uint8_t {
	Classic15,
	Blob47,
	Subset16,
	DualGrid15,
};

struct TilemapAutotileStyle {
	std::uint64_t id{ 1 };
	TilemapAutotileFormat format{ TilemapAutotileFormat::Blob47 };
	TextureKey texture{};
	V2_int texture_size{};
	V2_int tile_size{ 16, 16 };
};

struct TilemapAutotileLayout {
	V2_int grid{};
	int count{};
};

[[nodiscard]] inline std::optional<TilemapAutotileLayout> GetAutotileLayout(
	TilemapAutotileFormat format, V2_int tiles
) {
	if (!tiles.IsPositive()) {
		return std::nullopt;
	}

	if (format == TilemapAutotileFormat::Classic15 || format == TilemapAutotileFormat::DualGrid15) {
		if (tiles == V2_int{ 5, 3 } || tiles == V2_int{ 3, 5 }) {
			return TilemapAutotileLayout{ tiles, 15 };
		}

		if (format == TilemapAutotileFormat::Classic15) {
			if (tiles == V2_int{ 4, 4 }) {
				return TilemapAutotileLayout{ tiles, 16 };
			}
			return std::nullopt;
		}
	}

	if (format == TilemapAutotileFormat::Blob47) {
		if (tiles == V2_int{ 8, 6 }) {
			return TilemapAutotileLayout{ tiles, 47 };
		}

		return std::nullopt;
	}

	for (V2_int grid :
		 { V2_int{ 4, 4 }, V2_int{ 8, 2 }, V2_int{ 2, 8 }, V2_int{ 16, 1 }, V2_int{ 1, 16 } }) {
		if (tiles == grid) {
			return TilemapAutotileLayout{ grid, 16 };
		}
	}

	return std::nullopt;
}

[[nodiscard]] inline std::optional<TilemapAutotileLayout> GetAutotileLayout(
	const TilemapAutotileStyle& style
) {
	if (!style.tile_size.IsPositive() || !style.texture_size.IsPositive() ||
		style.texture_size.x % style.tile_size.x != 0 ||
		style.texture_size.y % style.tile_size.y != 0) {
		return std::nullopt;
	}

	return GetAutotileLayout(
		style.format,
		{ style.texture_size.x / style.tile_size.x, style.texture_size.y / style.tile_size.y }
	);
}

[[nodiscard]] inline int AutotileBlobMask(
	const std::function<bool(V2_int)>& occupied, V2_int cell
) {
	bool n{ occupied(cell + V2_int{ 0, -1 }) };
	bool e{ occupied(cell + V2_int{ 1, 0 }) };
	bool south{ occupied(cell + V2_int{ 0, 1 }) };
	bool w{ occupied(cell + V2_int{ -1, 0 }) };

	int mask{ 0 };

	if (n) {
		mask |= 1;
	}
	if (e) {
		mask |= 4;
	}
	if (south) {
		mask |= 16;
	}
	if (w) {
		mask |= 64;
	}

	if (n && e && occupied(cell + V2_int{ 1, -1 })) {
		mask |= 2;
	}
	if (e && south && occupied(cell + V2_int{ 1, 1 })) {
		mask |= 8;
	}
	if (south && w && occupied(cell + V2_int{ -1, 1 })) {
		mask |= 32;
	}
	if (w && n && occupied(cell + V2_int{ -1, -1 })) {
		mask |= 128;
	}

	return mask;
}

[[nodiscard]] inline int AutotileSlot(
	TilemapAutotileFormat format, const std::function<bool(V2_int)>& occupied, V2_int cell
) {
	if (format == TilemapAutotileFormat::Blob47) {
		int wanted{ AutotileBlobMask(occupied, cell) };
		int slot{ 0 };

		for (int mask{}; mask < 256; ++mask) {
			bool n{ (mask & 1) != 0 };
			bool e{ (mask & 4) != 0 };
			bool south{ (mask & 16) != 0 };
			bool w{ (mask & 64) != 0 };

			if (((mask & 2) && !(n && e)) || ((mask & 8) && !(e && south)) ||
				((mask & 32) && !(south && w)) || ((mask & 128) && !(w && n))) {
				continue;
			}

			if (mask == wanted) {
				return slot;
			}

			++slot;
		}

		return 0;
	}

	int mask{ 0 };

	if (occupied(cell + V2_int{ 0, -1 })) {
		mask |= 1;
	}
	if (occupied(cell + V2_int{ 1, 0 })) {
		mask |= 2;
	}
	if (occupied(cell + V2_int{ 0, 1 })) {
		mask |= 4;
	}
	if (occupied(cell + V2_int{ -1, 0 })) {
		mask |= 8;
	}

	return mask;
}

[[nodiscard]] inline int AutotileCornerMask(
	const std::function<bool(V2_int)>& occupied, V2_int display_cell
) {
	int mask{ 0 };

	if (occupied(display_cell)) {
		mask |= 1;
	}
	if (occupied(display_cell + V2_int{ 1, 0 })) {
		mask |= 2;
	}
	if (occupied(display_cell + V2_int{ 0, 1 })) {
		mask |= 4;
	}
	if (occupied(display_cell + V2_int{ 1, 1 })) {
		mask |= 8;
	}

	return mask;
}

/// @brief Rebuild the display tiles touching a changed logical terrain cell.
/// Caller maintains one unique nonzero style id per terrain; this edits Tilemap's
/// serializable terrain and derived display data, without a dependency on the editor.
inline void UpdateAutotiles(
	Tilemap tilemap, V2_int changed_cell, const std::vector<TilemapAutotileStyle>& styles
) {
	if (!tilemap) {
		return;
	}

	auto find_style = [&](std::uint64_t id) -> const TilemapAutotileStyle* {
		for (const auto& style : styles) {
			if (style.id == id) {
				return &style;
			}
		}

		return nullptr;
	};

	auto draw_tile = [&](V2_int cell, const TilemapAutotileStyle& style, int mask,
						 V2_float offset) {
		auto layout{ GetAutotileLayout(style) };
		int index{ layout && layout->count == 15 ? mask - 1 : mask };

		if (!layout || index < 0 || index >= layout->count) {
			if (const TilemapTile* old{ tilemap.FindTile(cell) };
				old && old->terrain_ruleset_id == style.id) {
				tilemap.EraseTile(cell);
			}
			return;
		}

		int x{ index % layout->grid.x };
		int y{ index / layout->grid.x };

		V2_float uv0{
			static_cast<float>(x * style.tile_size.x) / static_cast<float>(style.texture_size.x),
			static_cast<float>(y * style.tile_size.y) / static_cast<float>(style.texture_size.y)
		};

		V2_float uv1{ static_cast<float>((x + 1) * style.tile_size.x) /
						  static_cast<float>(style.texture_size.x),
					  static_cast<float>((y + 1) * style.tile_size.y) /
						  static_cast<float>(style.texture_size.y) };

		tilemap.SetTile(TilemapTile{
			.coordinate = cell,
			.texture = style.texture,
			.texture_coordinates = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } },
			.pixel_size = {
				std::max(1, static_cast<int>(std::lround(tilemap.GetData().cell_size.x))),
				std::max(1, static_cast<int>(std::lround(tilemap.GetData().cell_size.y))),
			},
			.origin = Origin::TopLeft,
			.offset = offset,
			.tint = color::White,
			.terrain_ruleset_id = style.id,
		});
	};

	// A two by two logical neighborhood controls each corner based display tile.
	for (V2_int delta : { V2_int{ 0, 0 }, V2_int{ -1, 0 }, V2_int{ 0, -1 }, V2_int{ -1, -1 } }) {
		V2_int display{ changed_cell + delta };
		const TilemapAutotileStyle* style{ nullptr };

		for (V2_int c : { display, display + V2_int{ 1, 0 }, display + V2_int{ 0, 1 },
						  display + V2_int{ 1, 1 } }) {
			if (auto id{ tilemap.GetTerrainRuleset(c) }) {
				const auto* candidate{ find_style(*id) };
				if (candidate && (candidate->format == TilemapAutotileFormat::Classic15 ||
								  candidate->format == TilemapAutotileFormat::DualGrid15)) {
					style = candidate;
					break;
				}
			}
		}
		if (!style) {
			if (const TilemapTile* old{ tilemap.FindTile(display) };
				old && old->terrain_ruleset_id && find_style(*old->terrain_ruleset_id) &&
				(find_style(*old->terrain_ruleset_id)->format == TilemapAutotileFormat::Classic15 ||
				 find_style(*old->terrain_ruleset_id)->format ==
					 TilemapAutotileFormat::DualGrid15)) {
				tilemap.EraseTile(display);
			}

			continue;
		}

		auto occupied = [&](V2_int c) {
			return tilemap.GetTerrainRuleset(c) == style->id;
		};

		int mask{ AutotileCornerMask(occupied, display) };

		V2_float offset{ style->format == TilemapAutotileFormat::DualGrid15
							 ? tilemap.GetData().cell_size * 0.5f
							 : V2_float{} };

		draw_tile(display, *style, mask, offset);
	}

	for (V2_int delta :
		 { V2_int{ 0, 0 }, V2_int{ -1, 0 }, V2_int{ 1, 0 }, V2_int{ 0, -1 }, V2_int{ 0, 1 },
		   V2_int{ -1, -1 }, V2_int{ 1, -1 }, V2_int{ -1, 1 }, V2_int{ 1, 1 } }) {
		V2_int cell{ changed_cell + delta };

		auto id{ tilemap.GetTerrainRuleset(cell) };

		const TilemapAutotileStyle* style{ id ? find_style(*id) : nullptr };

		if (style && (style->format == TilemapAutotileFormat::Blob47 ||
					  style->format == TilemapAutotileFormat::Subset16)) {
			auto occupied = [&](V2_int c) {
				return tilemap.GetTerrainRuleset(c) == style->id;
			};
			draw_tile(cell, *style, AutotileSlot(style->format, occupied, cell), {});
		} else if (
			const TilemapTile* old{ tilemap.FindTile(cell) }; old && old->terrain_ruleset_id
		) {
			const TilemapAutotileStyle* previous{ find_style(*old->terrain_ruleset_id) };
			if (previous && (previous->format == TilemapAutotileFormat::Blob47 ||
							 previous->format == TilemapAutotileFormat::Subset16)) {
				tilemap.EraseTile(cell);
			}
		}
	}
}

inline void PaintAutotile(
	Tilemap tilemap, V2_int cell, std::optional<std::uint64_t> id,
	const std::vector<TilemapAutotileStyle>& styles
) {
	if (!tilemap) {
		return;
	}

	tilemap.SetTerrainRuleset(cell, id);
	UpdateAutotiles(tilemap, cell, styles);
}

} // namespace ptgn
