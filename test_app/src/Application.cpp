#include <protegon.h>

using namespace engine;

class Test : public Engine {
public:
	Test() = default;
	Text t;
	AABB mouse_box{ { 30, 30 } };
	Circle mouse_circle{ 30 };
	template <typename T>
	ecs::Entity CreateStatic(ecs::Manager& manager, const V2_double& position, const T& shape) {
		auto obj = manager.CreateEntity();
		obj.AddComponent<TransformComponent>(Transform{ position });
		obj.AddComponent<ColorComponent>(colors::RED);
		obj.AddComponent<ShapeComponent>(shape);
		return obj;
	}
	Timer timer;
	void Init() {
		timer.Start();
		FontManager::Load("pixel-50", "resources/fonts/retro_gaming.ttf", 50);
		t = { "Hello World!", colors::BLACK, "pixel-50", { 50, 50 }, { 100, 50 } };
		m = manager.CreateEntity();
		auto circles = 5;
		auto aabbs = 5;
		for (auto c{ 0 }; c < circles; ++c) {
			CreateStatic(manager, V2_int::Random(0, Engine::GetScreenWidth(), 0, Engine::GetScreenHeight()), Circle{ engine::math::Random<double>(5, 30) });
		}
		for (auto a{ 0 }; a < aabbs; ++a) {
			CreateStatic(manager, V2_int::Random(0, Engine::GetScreenWidth(), 0, Engine::GetScreenHeight()), AABB{ V2_int::Random(5, 30, 5, 30) });
		}
		// Mouse
		m.AddComponent<RigidBodyComponent>();
		m.AddComponent<TransformComponent>();
		m.AddComponent<ColorComponent>(colors::BLUE);
		m.AddComponent<ShapeComponent>(mouse_box);
		manager.Refresh();
	}
	void Update() {
		auto [m_t, m_s] = m.GetComponents<TransformComponent, ShapeComponent>();

		m_t.transform.position = InputHandler::GetMousePosition();// - m_shape.size / 2;

		if (InputHandler::KeyDown(Key::R)) {
			circle = !circle;
			if (!circle) {
				m.AddComponent<ShapeComponent>(mouse_box);
			} else {
				m.AddComponent<ShapeComponent>(mouse_circle);
			}
		}
		auto entities{ manager.GetEntityComponents<TransformComponent, ShapeComponent>() };
		std::vector<Manifold> manifolds;
		for (auto [entity, transform, shape] : entities) {
			if (entity != m) {
				auto manifold = StaticCollisionCheck(m_t.transform, transform.transform, m_s.shape, shape.shape);
				m_t.transform.position -= manifold.penetration;
			}
		}

		/*for (const auto& manifold : manifolds) {
			if (!manifold.normal.IsZero()) {
				LOG_(manifold.penetration << ",");
			}
		}
		LOG("");*/
		if (timer.ElapsedSeconds() > 15) {
			t.SetContent("Color, size, even shading");
			t.SetColor(colors::BLUE);
			t.SetArea({ 300, 100 });
			t.SetShaded(colors::YELLOW);
		} else if (timer.ElapsedSeconds() > 10) {
			t.SetPosition({ 50, 50 + 100 });
			t.SetColor(colors::RED);
		} else if (timer.ElapsedSeconds() > 5) {
			t.SetContent("I can change dynamically!");
			t.SetArea({ 200, 100 });
		}
	}
	void Render() {
		auto entities{ manager.GetEntityComponents<TransformComponent, ShapeComponent, ColorComponent>() };
		for (auto [entity, transform, shape, color] : entities) {
			if (shape.shape->GetType() == ShapeType::CIRCLE) {
				TextureManager::DrawCircle(
					transform.transform.position,
					shape.shape->CastTo<Circle>().radius,
					color.color
				);
			} else if (shape.shape->GetType() == ShapeType::AABB) {
				TextureManager::DrawRectangle(
					transform.transform.position,
					shape.shape->CastTo<AABB>().size,
					color.color
				);
			}
		}
		t.Draw();
	}
	bool circle{ true };
	ecs::Entity m;
	ecs::Manager manager;
};

int main(int c, char** v) {

	Engine::Start<Test>("Squirhell", 800, 600, 60);

	return 0;
}