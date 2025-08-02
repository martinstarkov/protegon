#include "serialization/json_manager.h"

#include "components/generic.h"
#include "resources/resource_manager.h"
#include "serialization/json.h"
#include "utility/file.h"

namespace ptgn::impl {

const json& JsonManager::Get(const ResourceHandle& key) const {
	return ParentManager::Get(key);
}

json JsonManager::LoadFromFile(const path& filepath) {
	return ptgn::LoadJson(filepath);
}

} // namespace ptgn::impl