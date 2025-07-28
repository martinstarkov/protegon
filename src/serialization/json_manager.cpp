#include "serialization/json_manager.h"

#include "serialization/json.h"
#include "utility/file.h"

namespace ptgn::impl {

json JsonManager::LoadFromFile(const path& filepath) {
	return ptgn::LoadJson(filepath);
}

} // namespace ptgn::impl