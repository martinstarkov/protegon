#include <memory>
#include <vector>

#include "common.h"
#include "core/game.h"

class TemplateTest : public Test {
public:
	void Init() final {}

	void Update() final {}
};

void TestTemplateNames() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TemplateTest());

	AddTests(tests);
}