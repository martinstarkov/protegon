#include <Protegon.h>

using namespace ptgn;

class ThirdScene : public Scene {
public:
    int size = 0;
    ThirdScene() = delete;
    ThirdScene(int size) : size{ size } {}
    void Enter() {
        PrintLine("Entering third scene");
    }
    void Update() {
        if (InputHandler::KeyDown(Key::K_1)) {
            SceneManager::SetActiveScene("intro_scene");
        } else if (InputHandler::KeyDown(Key::K_2)) {
            SceneManager::SetActiveScene("other_scene");
        } else if (InputHandler::KeyDown(Key::K_3)) {
            SceneManager::SetActiveScene("third_scene");
        }

        if (InputHandler::KeyDown(Key::ESCAPE)) {
            SceneManager::SetActiveScene("intro_scene");
            SceneManager::UnloadScene("third_scene");
        }
    }
    void Render() {
        ScreenRenderer::DrawCircle({ 300, 300 }, size, colors::BLACK);
    }
    void Exit() {}
};

class OtherScene : public Scene {
public:
    void Enter() {
        SceneManager::LoadScene<ThirdScene>("third_scene", 100);
        PrintLine("Entering other scene");
    }
    void Update() {
        if (InputHandler::KeyDown(Key::K_1)) {
            SceneManager::SetActiveScene("intro_scene");
        } else if (InputHandler::KeyDown(Key::K_2)) {
            SceneManager::SetActiveScene("other_scene");
        } else if (InputHandler::KeyDown(Key::K_3)) {
            SceneManager::SetActiveScene("third_scene");
        }
    }
    void Render() {
        ScreenRenderer::DrawSolidCircle({ 300, 300 }, 50, colors::BLUE);
    }
    void Exit() {}
};

class IntroScene : public Scene {
public:
    void Enter() {
        SceneManager::LoadScene<OtherScene>("other_scene");
        PrintLine("Entering intro scene");
    }
    void Update() {
        if (InputHandler::KeyDown(Key::K_1)) {
            SceneManager::SetActiveScene("intro_scene");
        } else if (InputHandler::KeyDown(Key::K_2)) {
            SceneManager::SetActiveScene("other_scene");
        } else if (InputHandler::KeyDown(Key::K_3)) {
            SceneManager::SetActiveScene("third_scene");
        }
    }
    void Render() {
        ScreenRenderer::DrawSolidRectangle({ 300, 300 }, { 50, 50 }, colors::RED);
    }
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