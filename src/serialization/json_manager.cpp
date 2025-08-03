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
	json j{ ptgn::LoadJson(filepath) };
#ifdef __EMSCRIPTEN__
	if (j.is_array()) {
		j = j.at(0);
	}
#endif
	return j;
}

} // namespace ptgn::impl