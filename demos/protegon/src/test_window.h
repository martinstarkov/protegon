#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "core/game.h"
#include "core/window.h"
#include "event/key.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "utility/string.h"
#include "utility/time.h"
#include "utility/timer.h"

void TestWindow() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new WindowSettingTest());

	AddTests(tests);
}