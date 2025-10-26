#pragma once

#include "core/ecs/components/generic.h"
#include "core/asset/asset_manager.h"
#include "core/util/file.h"
#include "serialization/json/json.h"

namespace ptgn::impl {

class JsonManager : public ResourceManager<JsonManager, ResourceHandle, json> {
public:
	[[nodiscard]] const json& Get(const ResourceHandle& key) const;

private:
	friend ParentManager;

	[[nodiscard]] static json LoadFromFile(const path& filepath);
};

} // namespace ptgn::impl