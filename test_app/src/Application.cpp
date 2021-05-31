#include <Protegon.h>

using namespace ptgn;

class IntroScene : public Scene {
public:
    void Enter() {}
    void Update() {}
    void Render() {}
    void Exit() {}
};

void TestMath() {
	PrintLine("Running math tests...");

	assert(math::Clamp(0.5, 0.1, 1.0) == 0.5);
    assert(math::Clamp(1.3, 0.1, 1.0) == 1.0);
    assert(math::Clamp(0.05, 0.1, 1.0) == 0.1);

    assert(math::Sign(-5) == -1);
    assert(math::Sign(0.0) == 0);
    assert(math::Sign(5) == 1);

    assert(math::Floor(1.4) == 1.0);
    assert(math::Floor(1.8) == 1.0);
    assert(math::Floor(1.0) == 1.0);
    assert(math::Floor(2.0) == 2.0);

    assert(math::Ceil(1.4) == 2.0);
    assert(math::Ceil(1.8) == 2.0);
    assert(math::Ceil(1.0) == 1.0);
    assert(math::Ceil(2.0) == 2.0);
    
    assert(math::Round(1.4) == 1.0);
    assert(math::Round(1.8) == 2.0);
    assert(math::Round(1.0) == 1.0);
    assert(math::Round(2.0) == 2.0);

    assert(math::Abs(-5) == 5);
    assert(math::Abs(0.0) == 0);
    assert(math::Abs(5) == 5);

    assert(math::Sqrt(4) == 2);

    assert(math::Lerp(100, 200, 0.5) == 150);

	PrintLine("Math tests passed!");
}

int main(int c, char** v) {

	TestMath();

	Engine::Start<IntroScene>("intro_scene", "Squirhell", { 800, 600 }, 60);
	

	return 0;
}