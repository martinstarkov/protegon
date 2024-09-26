#include "common.h"
#include "protegon/game.h"

class TemplateTest : public Test {
	void Init() final {}

	void Update() final {}
};

void TestTemplateNames() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new TemplateTest());

	AddTests(tests);
}