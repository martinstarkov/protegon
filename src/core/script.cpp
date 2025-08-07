#include "core/script.h"

#include <vector>

#include "core/entity.h"
#include "scene/scene.h"

namespace ptgn {

void Scripts::Update(Scene& scene, float dt) {
	Invoke<&impl::IScript::OnUpdate>(scene, dt);

	scene.Refresh();
}

std::vector<Entity> Scripts::GetEntities(Scene& scene) {
	return scene.EntitiesWith<Scripts>().GetVector();
}

namespace impl {} // namespace impl

} // namespace ptgn
