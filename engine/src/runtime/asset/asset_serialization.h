#pragma once

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"

namespace ptgn {

/// @brief Persistent path-backed asset entry stored in the project asset catalog.
struct SerializedAsset {
	AssetKey key;
	AssetKind kind{ AssetKind::Unknown };
	path source_path;

	PTGN_REFLECT(SerializedAsset, key, kind, source_path)
};

} // namespace ptgn
