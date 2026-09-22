#include "runtime/world/paint_generator.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include "core/assert.h"
#include "core/math/transform.h"
#include "runtime/scene/scene.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

PaintGenerator::PaintGenerator(Entity entity) : Entity{ entity } {
	PTGN_ASSERT(!entity || entity.Has<impl::PaintGeneratorData>(), "Entity is not a PaintGenerator");
}

const impl::PaintGeneratorData& PaintGenerator::GetData() const {
	PTGN_ASSERT(*this, "Cannot access a null PaintGenerator");
	return Get<impl::PaintGeneratorData>();
}

impl::PaintGeneratorData& PaintGenerator::GetData() {
	return const_cast<impl::PaintGeneratorData&>(std::as_const(*this).GetData());
}

PaintGenerator& PaintGenerator::SetGeometry(PaintGeneratorGeometry geometry) {
	GetData().geometry = geometry;
	return *this;
}

PaintGenerator& PaintGenerator::SetGrid(V2_float size, V2_float offset) {
	PTGN_ASSERT(size.IsPositive(), "Generator grid size must be positive");
	GetData().grid_size = size;
	GetData().grid_offset = offset;
	return *this;
}

PaintGenerator& PaintGenerator::SetEnabled(bool enabled) {
	GetData().enabled = enabled;
	return *this;
}

bool PaintGenerator::SetTargetTilemap(std::optional<Tilemap> target) {
	if (!*this) {
		return false;
	}

	const auto generator_layer_id{ GetScene().GetLayers().GetLayerId(*this) };
	if (!generator_layer_id.has_value()) {
		return false;
	}
	const SceneLayer* generator_layer{ GetScene().GetLayers().Find(generator_layer_id.value()) };
	if (!generator_layer) {
		return false;
	}

	if (!target.has_value()) {
		GetData().target_tilemap.reset();
		return true;
	}

	if (!target.value() || &target->GetScene() != &GetScene() || !IsTilemap(target.value())) {
		return false;
	}
	if (generator_layer->kind != SceneLayerKind::Tile) {
		return false;
	}

	const auto target_layer{ GetScene().GetLayers().GetLayerId(target.value()) };
	if (!target_layer.has_value() || target_layer.value() != generator_layer_id.value()) {
		return false;
	}

	GetData().target_tilemap = target->Get<UUID>();
	return true;
}

bool PaintGenerator::IsSuppressed(V2_int cell) const {
	return std::ranges::contains(GetData().suppressed_cells, cell);
}

bool PaintGenerator::SetSuppressed(V2_int cell, bool suppressed) {
	auto& cells{ GetData().suppressed_cells };
	const auto it{ std::ranges::find(cells, cell) };
	if (suppressed) {
		if (it != cells.end()) {
			return false;
		}
		cells.emplace_back(cell);
		return true;
	}
	if (it == cells.end()) {
		return false;
	}
	cells.erase(it);
	return true;
}

void PaintGenerator::ClearSuppressions() {
	GetData().suppressed_cells.clear();
}

bool IsPaintGenerator(Entity entity) {
	return entity && entity.Has<impl::PaintGeneratorData>();
}

PaintGenerator CreatePaintGenerator(Scene& scene, SceneLayerId layer, Tag tag) {
	if (!scene.GetLayers().Find(layer)) {
		return {};
	}

	Entity entity{ scene.CreateEntity(std::move(tag)) };
	entity.TryAdd<Transform>();
	entity.Add<impl::PaintGeneratorData>();

	if (!scene.GetLayers().Assign(entity, layer, false)) {
		entity.Destroy();
		return {};
	}

	return PaintGenerator{ entity };
}

} // namespace ptgn
