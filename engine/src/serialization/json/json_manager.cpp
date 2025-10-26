#include "serialization/json/json_manager.h"

#include "core/ecs/components/generic.h"
#include "core/asset/asset_manager.h"
#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn::impl {

const json& JsonManager::Get(const ResourceHandle& key) const {
	return ParentManager::Get(key);
}

json JsonManager::LoadFromFile(const path& filepath) {
	json j = ptgn::LoadJson(filepath);
	return j;
}

} // namespace ptgn::impl