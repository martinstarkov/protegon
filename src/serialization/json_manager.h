#pragma once

#include "components/generic.h"
#include "resources/resource_manager.h"
#include "serialization/json.h"
#include "utility/file.h"

namespace ptgn::impl {

class JsonManager : public ResourceManager<JsonManager, ResourceHandle, json> {
public:
	[[nodiscard]] const json& Get(const ResourceHandle& key) const;

private:
	friend class ParentManager;

	[[nodiscard]] static json LoadFromFile(const path& filepath);
};

} // namespace ptgn::impl