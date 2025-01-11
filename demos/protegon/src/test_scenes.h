#include <memory>
#include <string_view>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "event/key.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/time.h"

void TestScenes() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new SceneTransitionTest());

	AddTests(tests);
}