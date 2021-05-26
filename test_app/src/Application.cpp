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
	ParticleManager particle_manager_{ 1000 };
	void Enter() {
		TextureManager::Load("acorn", "resources/sprites/acorn.png");
		timer.Start();
		FontManager::Load("pixel-50", "resources/fonts/retro_gaming.ttf", 50);
		t = { "Hello World!", colors::BLACK, "pixel-50", { 50 + 200, 50 }, { 100, 50 } };
		m = manager.CreateEntity();
		auto circles{ 5 };
		auto aabbs{ 5 };
		auto window_size{ Window::GetSize() };
		RNG<double> rng(5, 30);
		for (auto c{ 0 }; c < circles; ++c) {
			CreateStatic(manager, 
						 V2_int::Random(0, window_size.x, 0, window_size.y), 
						 Circle{ rng() });
		}
		for (auto a{ 0 }; a < aabbs; ++a) {
			CreateStatic(manager, 
						 V2_int::Random(0, window_size.x, 0, window_size.y), 
						 AABB{ V2_int::Random(5, 30, 5, 30) });
		}
		//manager.AddSystem<engine::CameraSystem>();
		//engine::CameraSystem::ZOOM_IN_KEY = Key::G;
		//engine::CameraSystem::ZOOM_OUT_KEY = Key::H;

		CreateStatic(manager, { 300, 300 },
					 AABB{ { 200, 30 } });
		auto s = CreateStatic(manager, { 300 - 30, 300 - 200 },
					 AABB{ { 30, 200 } });
		//s.AddComponent<TagComponent>(69);

		//manager.AddSystem<InputSystem>();

		// Mouse
		//m.AddComponent<CameraComponent>(true);
		m.AddComponent<PlayerComponent>();
		m.AddComponent<RigidBodyComponent>();
		m.AddComponent<TransformComponent>();
		m.AddComponent<ColorComponent>(colors::BLUE);
		m.AddComponent<ShapeComponent>(mouse_box);
		manager.Refresh();

		t.SetStyles(FontStyle::BOLD, FontStyle::UNDERLINE, FontStyle::STRIKETHROUGH, FontStyle::ITALIC);

		particle_manager_.Init({ milliseconds{ 1000 }, new Circle(5), new Circle(30), colors::BLACK, colors::PINK });
	}
	void Update() {
		//manager.UpdateSystem<InputSystem>();
		auto [mouse, transform, shape, rigid_body, player] = manager.GetUniqueEntityAndComponents<TransformComponent, ShapeComponent, RigidBodyComponent, PlayerComponent>();
		
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

		if (InputHandler::KeyPressed(Key::P)) {
			ParticleProperties properties;
			properties.transform = { Window::GetSize() / 2 };
			for (auto i = 0; i < 10; ++i) {
				properties.body.velocity = V2_double::Random(-5, 5, -5, 5);
				particle_manager_.Emit(properties);
			}
		}

		rigid_body.body.velocity += rigid_body.body.acceleration;
		transform.transform.position += rigid_body.body.velocity;

		particle_manager_.Update();

		if (InputHandler::MouseHeld(Mouse::LEFT, seconds{ 3 })) {
			PrintLine("Held left for 3 seconds");
		}

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
		if (timer.Elapsed<seconds>() > seconds{ 15 }) {
			t.SetContent("Color, size, even shading");
			t.SetColor(colors::BLUE);
			t.SetArea({ 300, 100 });
			t.SetShadedRenderMode(colors::YELLOW);
		} else if (timer.Elapsed<seconds>() > seconds{ 10 }) {
			t.SetPosition({ 50 + 200, 50 + 100 });
			t.SetColor(colors::RED);
			t.SetBlendedRenderMode();
		} else if (timer.Elapsed<seconds>() > seconds{ 5 }) {
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
		Renderer::DrawSolidCircle({ 50, 50 }, 50, colors::RED);
		Renderer::DrawCircle({ 50 + 50, 50 }, 50, colors::BLUE);
		Renderer::DrawRectangle({ 50 + 50, 50 + 50 }, { 50, 50 }, colors::GREEN);
		Renderer::DrawSolidRectangle({ 50 + 50 + 50, 50 + 50 }, { 50, 50 }, colors::ORANGE);
		Renderer::DrawPoint({ 50 + 50 + 50 + 70, 50 + 50 + 50 }, colors::PURPLE);
		Renderer::DrawPoint({ 50 + 50 + 50 + 73, 50 + 50 + 50 }, colors::PURPLE);
		Renderer::DrawPoint({ 50 + 50 + 50 + 76, 50 + 50 + 50 }, colors::PURPLE);
		Renderer::DrawLine({ 50 + 50 + 50 + 50, 50 + 50 + 50 + 50 }, { 50 + 50 + 50, 50 + 50 + 50 }, colors::CYAN);
		particle_manager_.Render();
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
		auto window_size = Window::GetSize();
		RNG<double> rng(5, 30);
		for (auto c{ 0 }; c < circles; ++c) {
			CreateStatic(manager,
						 V2_int::Random(0, window_size.x, 0, window_size.y),
						 Circle{ rng() });
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

int main(int c, char** v) {

	Engine::Start<Test>("test_scene", "Squirhell", { 800, 600 }, 60);

	return 0;
}