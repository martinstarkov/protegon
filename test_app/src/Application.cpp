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
            SceneManager::UnloadScene("other_scene");
        }
    }
    void Render() {
        WorldRenderer::DrawCircle({ 300, 300 }, size, colors::BLACK);
    }
    void Exit() {
        std::cout << "exiting third" << std::endl;
    }
};

class OtherScene : public Scene {
public:
    void Init() {
        SceneManager::LoadScene<ThirdScene>("third_scene", 100);
        V2_double hello{ 3.0, 5.0 };
        V2_int test = hello;
    }
    void Enter() {
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
        WorldRenderer::DrawSolidCircle({ 300, 300 }, 50, colors::BLUE);
    }
    void Exit() {
        std::cout << "exiting other" << std::endl;
    }
};

class IntroScene : public Scene {
public:
    ecs::Entity player;
    void Init() {
        
        SceneManager::LoadScene<OtherScene>("other_scene");

        player = manager.CreateEntity();
        player.AddComponent<InputComponent>();
        auto& transform = player.AddComponent<TransformComponent>().transform;
        transform.position = { 300, 300 };
        player.AddComponent<RigidBodyComponent>();
        player.AddComponent<ColorComponent>().color = colors::RED;
        player.AddComponent<ShapeComponent>(AABB{ { 30, 30 } });

        manager.Refresh();

    }
    void Enter() {
        PrintLine("Entering intro scene");
    }
    void Update() {
        auto [transform, color, shape, rigid_body] = player.GetComponents<TransformComponent, ColorComponent, ShapeComponent, RigidBodyComponent>();
        V2_double speed{ 4, 4 };
        bool w{ InputHandler::KeyPressed(Key::W) };
        bool a{ InputHandler::KeyPressed(Key::A) };
        bool s{ InputHandler::KeyPressed(Key::S) };
        bool d{ InputHandler::KeyPressed(Key::D) };
        if (a && !d) {
            rigid_body.body.velocity.x = -speed.x;
        } else if (!a && d) {
            rigid_body.body.velocity.x = speed.x;
        } else {
            rigid_body.body.velocity.x = 0;
        }
        if (w && !s) {
            rigid_body.body.velocity.y = -speed.y;
        } else if (!w && s) {        
            rigid_body.body.velocity.y = speed.y;
        } else {                     
            rigid_body.body.velocity.y = 0;
        }

        if (InputHandler::KeyDown(Key::K_1)) {
            SceneManager::SetActiveScene("intro_scene");
        } else if (InputHandler::KeyDown(Key::K_2)) {
            SceneManager::SetActiveScene("other_scene");
        } else if (InputHandler::KeyDown(Key::K_3)) {
            SceneManager::SetActiveScene("third_scene");
        }
        
        rigid_body.body.velocity *= 0.99;
        transform.transform.position += rigid_body.body.velocity;
        
        if (InputHandler::KeyPressed(Key::Q)) {
            camera.scale -= camera.zoom_speed;
            camera.ClampToBound();
        }
        if (InputHandler::KeyPressed(Key::E)) {
            camera.scale += camera.zoom_speed;
            camera.ClampToBound();
        }
        if (InputHandler::KeyPressed(Key::SPACE)) {
            camera.CenterOn(transform.transform.position, shape.GetSize());
        }
    }
    void Render() {
        auto [transform, color, shape] = player.GetComponents<TransformComponent, ColorComponent, ShapeComponent>();
        WorldRenderer::DrawSolidRectangle(transform.transform.position, shape.GetSize(), color.color);
        WorldRenderer::DrawSolidRectangle({ 400, 400 }, { 60, 60 }, colors::BLUE);
        WorldRenderer::DrawSolidRectangle({ 200, 300 }, { 60, 90 }, colors::BLACK);
    }
    void Exit() {
        std::cout << "exiting intro" << std::endl;
    }
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