#include <protegon.h>

using namespace engine;

template <typename T>
ecs::Entity CreateStatic(ecs::Manager& manager, const V2_double& position, const T& shape) {
	auto obj{ manager.CreateEntity() };
	obj.AddComponent<TransformComponent>(Transform{ position });
	obj.AddComponent<ColorComponent>(colors::RED);
	obj.AddComponent<ShapeComponent>(shape);
	return obj;
}

class Test : public Scene {
public:
	Test() = default;
	Text t;
	AABB mouse_box{ { 30, 30 } };
	Circle mouse_circle{ 30 };
	Timer timer;
	void Enter() {
		TextureManager::Load("acorn", "resources/sprites/acorn.png");
		timer.Start();
		FontManager::Load("pixel-50", "resources/fonts/retro_gaming.ttf", 50);
		t = { "Hello World!", colors::BLACK, "pixel-50", { 50 + 200, 50 }, { 100, 50 } };
		m = manager.CreateEntity();
		auto circles{ 5 };
		auto aabbs{ 5 };
		auto window_size{ Engine::GetDisplay().first.GetSize() };
		for (auto c{ 0 }; c < circles; ++c) {
			CreateStatic(manager, 
						 V2_int::Random(0, window_size.x, 0, window_size.y), 
						 Circle{ engine::math::Random<double>(5, 30) });
		}
		for (auto a{ 0 }; a < aabbs; ++a) {
			CreateStatic(manager, 
						 V2_int::Random(0, window_size.x, 0, window_size.y), 
						 AABB{ V2_int::Random(5, 30, 5, 30) });
		}
		manager.AddSystem<engine::CameraSystem>();
		engine::CameraSystem::ZOOM_IN_KEY = Key::G;
		engine::CameraSystem::ZOOM_OUT_KEY = Key::H;

		CreateStatic(manager, { 300, 300 },
					 AABB{ { 200, 30 } });
		auto s = CreateStatic(manager, { 300 - 30, 300 - 200 },
					 AABB{ { 30, 200 } });
		s.AddComponent<TagComponent>(69);

		manager.AddSystem<InputSystem>();

		// Mouse
		m.AddComponent<CameraComponent>(true);
		m.AddComponent<InputComponent>();
		m.AddComponent<RigidBodyComponent>();
		m.AddComponent<TransformComponent>();
		m.AddComponent<ColorComponent>(colors::BLUE);
		m.AddComponent<ShapeComponent>(mouse_box);
		manager.Refresh();
		t.SetStyles(FontStyle::BOLD, FontStyle::UNDERLINE, FontStyle::STRIKETHROUGH, FontStyle::ITALIC);
	}
	void Update() {
		manager.UpdateSystem<InputSystem>();
		auto [mouse, transform, shape, rigid_body, input] = manager.GetUniqueEntityAndComponents<TransformComponent, ShapeComponent, RigidBodyComponent, InputComponent>();
		
		if (InputHandler::KeyDown(Key::V)) {
			auto mouse_position{ InputHandler::GetMousePosition() };
			auto new_mouse{ manager.CopyEntity<TransformComponent, ShapeComponent, RigidBodyComponent>(mouse) };
			manager.Refresh();
			assert(new_mouse.HasComponent<TransformComponent>());
			assert(new_mouse.HasComponent<ShapeComponent>());
			assert(new_mouse.HasComponent<RigidBodyComponent>());
			new_mouse.AddComponent<ColorComponent>().color = colors::GREEN;
			new_mouse.GetComponent<TransformComponent>().transform.position = mouse_position;
		}

		rigid_body.body.velocity += rigid_body.body.acceleration;
		transform.transform.position += rigid_body.body.velocity;

		/*transform.transform.position = InputHandler::GetMousePosition();

		if (shape.shape->GetType() == ShapeType::AABB) {
			transform.transform.position -= shape.shape->CastTo<AABB>().size / 2.0;
		}*/

		if (InputHandler::MouseDown(MouseButton::LEFT)) {
			PrintLine("Clicked left");
		}

		if (InputHandler::MouseUp(MouseButton::LEFT)) {
			PrintLine("Let go of left");
		}


		/*if (InputHandler::KeyPressed(Key::X)) {
			PrintLine("Pressing X");
		}
		if (InputHandler::KeyDown(Key::C)) {
			PrintLine("Down C");
		}
		if (!InputHandler::KeyReleased(Key::V)) {
			PrintLine("Not releasing V");
		}
		if (InputHandler::KeyUp(Key::B)) {
			PrintLine("Lifted B");
		}*/

		if (InputHandler::KeyDown(Key::R)) {
			if (shape.shape->GetType() == ShapeType::CIRCLE) {
				mouse.AddComponent<ShapeComponent>(mouse_box);
			} else if (shape.shape->GetType() == ShapeType::AABB) {
				mouse.AddComponent<ShapeComponent>(mouse_circle);
			}
		}

		auto entities{ manager.GetEntitiesAndComponents<TransformComponent, ShapeComponent>() };
		std::vector<Manifold> manifolds;
		for (auto [entity2, transform2, shape2] : entities) {
			if (entity2 != mouse) {
				auto manifold{ StaticCollisionCheck(transform.transform, 
													transform2.transform, 
													shape.shape, 
													shape2.shape) 
				};
				//Print(manifold.penetration);
				transform.transform.position -= manifold.penetration;
			}
		}
		//PrintLine();

		/*for (const auto& manifold : manifolds) {
			if (!manifold.normal.IsZero()) {
				LOG_(manifold.penetration << ",");
				m_t.transform.position -= manifold.penetration;
			}
		}
		LOG("");*/

		//PrintLine(t.GetContent(), " ", t.GetColor(), " ", t.GetArea());

		if (timer.ElapsedSeconds() > 15) {
			t.SetContent("Color, size, even shading");
			t.SetColor(colors::BLUE);
			t.SetArea({ 300, 100 });
			t.SetShadedRenderMode(colors::YELLOW);
		} else if (timer.ElapsedSeconds() > 10) {
			t.SetPosition({ 50 + 200, 50 + 100 });
			t.SetColor(colors::RED);
			t.SetBlendedRenderMode();
		} else if (timer.ElapsedSeconds() > 5) {
			t.SetContent("I can change dynamically!");
			t.SetArea({ 200, 100 });
		}
	}
	void Render() {
		auto entities{ manager.GetEntitiesAndComponents<TransformComponent, ShapeComponent, ColorComponent>() };
		for (auto [entity, transform, shape, color] : entities) {
			if (shape.shape->GetType() == ShapeType::CIRCLE) {
				Renderer::DrawCircle(
					transform.transform.position,
					shape.shape->CastTo<Circle>().radius,
					color.color
				);
			} else if (shape.shape->GetType() == ShapeType::AABB) {
				Renderer::DrawRectangle(
					transform.transform.position,
					shape.shape->CastTo<AABB>().size,
					color.color
				);
			}
		}
		Renderer::DrawTexture("acorn",
							  { 200, 200 },
							  { 30, 30 },
							  {}, {}, nullptr, 20, Flip::HORIZONTAL);
		Renderer::DrawText(t);
		Renderer::DrawSolidCircle({ 50, 50 }, 50, colors::RED, 1);
		Renderer::DrawCircle({ 50 + 50, 50 }, 50, colors::BLUE, 1);
		Renderer::DrawRectangle({ 50 + 50, 50 + 50 }, { 50, 50 }, colors::GREEN, 1);
		Renderer::DrawSolidRectangle({ 50 + 50 + 50, 50 + 50 }, { 50, 50 }, colors::ORANGE, 1);
		Renderer::DrawPoint({ 50 + 50 + 50 + 70, 50 + 50 + 50 }, colors::PURPLE, 1);
		Renderer::DrawPoint({ 50 + 50 + 50 + 73, 50 + 50 + 50 }, colors::PURPLE, 1);
		Renderer::DrawPoint({ 50 + 50 + 50 + 76, 50 + 50 + 50 }, colors::PURPLE, 1);
		Renderer::DrawLine({ 50 + 50 + 50 + 50, 50 + 50 + 50 + 50 }, { 50 + 50 + 50, 50 + 50 + 50 }, colors::CYAN, 1);
	}
	void Exit() {
		PrintLine("Exiting test scene");
	}
	~Test() {
		PrintLine("Unloading test scene");
	}
	ecs::Entity m;
	ecs::Manager manager;
};

class Other : public Scene {
public:
	AABB mouse_box{ { 30, 30 } };
	Circle mouse_circle{ 30 };
	void Enter() {
		auto circles = 50;
		auto window_size = GetWindowSize();
		for (auto c{ 0 }; c < circles; ++c) {
			CreateStatic(manager,
						 V2_int::Random(0, window_size.x, 0, window_size.y),
						 Circle{ engine::math::Random<double>(5, 30) });
		}
		auto mouse = manager.CreateEntity();
		mouse.AddComponent<TransformComponent>();
		mouse.AddComponent<PlayerComponent>();
		mouse.AddComponent<ColorComponent>(Color{ colors::PINK });
		mouse.AddComponent<ShapeComponent>(mouse_circle);
		manager.Refresh();
	}
	void Update() {
		auto [mouse, transform, shape, rigid_body] = manager.GetUniqueEntityAndComponents<TransformComponent, ShapeComponent, PlayerComponent>();

		transform.transform.position = InputHandler::GetMousePosition();

		if (shape.shape->GetType() == ShapeType::AABB) {
			transform.transform.position -= shape.shape->CastTo<AABB>().size / 2.0;
		}

		if (InputHandler::KeyDown(Key::R)) {
			if (shape.shape->GetType() == ShapeType::CIRCLE) {
				mouse.AddComponent<ShapeComponent>(mouse_box);
			} else if (shape.shape->GetType() == ShapeType::AABB) {
				mouse.AddComponent<ShapeComponent>(mouse_circle);
			}
		}
	}
	void Render() {
		auto entities{ manager.GetEntitiesAndComponents<TransformComponent, ShapeComponent, ColorComponent>() };
		for (auto [entity, transform, shape, color] : entities) {
			if (shape.shape->GetType() == ShapeType::CIRCLE) {
				Renderer::DrawCircle(
					transform.transform.position,
					shape.shape->CastTo<Circle>().radius,
					color.color
				);
			} else if (shape.shape->GetType() == ShapeType::AABB) {
				Renderer::DrawRectangle(
					transform.transform.position,
					shape.shape->CastTo<AABB>().size,
					color.color
				);
			}
		}
	}
	void Exit() {
		PrintLine("Exiting other scene");
	}
	~Other() {
		PrintLine("Unloading other scene");
	}
};

class Application : public Engine {
public:
	Display display;
	void Init() {
		engine::SceneManager::LoadScene<Test>("test_scene");
		engine::SceneManager::LoadScene<Other>("other_scene");
		engine::SceneManager::EnterScene("test_scene");
	}
	void Update() {
		engine::SceneManager::UpdateActiveScenes();
		if (engine::InputHandler::KeyDown(Key::O)) {
			engine::SceneManager::EnterScene("other_scene");
		} else if (engine::InputHandler::KeyDown(Key::T)) {
			//engine::SceneManager::EnterScene("test_scene", EnterArguments{ }, InitArguments{ });
		}
		if (engine::InputHandler::KeyDown(Key::U)) {
			engine::SceneManager::UnloadScene("test_scene");
		}

		if (engine::InputHandler::KeyDown(Key::L)) {
			engine::SceneManager::LoadScene<Test>("test_scene");
		}
	}
	void Render() {
		engine::SceneManager::RenderActiveScenes();
		engine::SceneManager::UnloadQueuedScenes();
	}
};

int main(int c, char** v) {

	Engine::Start<Application>("Squirhell", { 800, 600 }, 60);

	return 0;
}