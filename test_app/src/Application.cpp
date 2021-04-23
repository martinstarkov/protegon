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
		auto obj{ manager.CreateEntity() };
		obj.AddComponent<TransformComponent>(Transform{ position });
		obj.AddComponent<ColorComponent>(colors::RED);
		obj.AddComponent<ShapeComponent>(shape);
		return obj;
	}
	Timer timer;
	void Init() {
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


		CreateStatic(manager, { 300, 300 },
					 AABB{ { 200, 30 } });
		CreateStatic(manager, { 300 - 30, 300 - 200 },
					 AABB{ { 30, 200 } });


		// Mouse
		m.AddComponent<RigidBodyComponent>();
		m.AddComponent<TransformComponent>();
		m.AddComponent<ColorComponent>(colors::BLUE);
		m.AddComponent<ShapeComponent>(mouse_box);
		manager.Refresh();
		t.SetStyles(FontStyle::BOLD, FontStyle::UNDERLINE, FontStyle::STRIKETHROUGH, FontStyle::ITALIC);
	}
	void Update() {
		auto [m_t, m_s] = m.GetComponents<TransformComponent, ShapeComponent>();

		m_t.transform.position = InputHandler::GetMousePosition();

		if (m_s.shape->GetType() == ShapeType::AABB) {
			m_t.transform.position -= m_s.shape->CastTo<AABB>().size / 2.0;
		}

		if (InputHandler::KeyDown(Key::R)) {
			if (m_s.shape->GetType() == ShapeType::CIRCLE) {
				m.AddComponent<ShapeComponent>(mouse_box);
			} else if (m_s.shape->GetType() == ShapeType::AABB) {
				m.AddComponent<ShapeComponent>(mouse_circle);
			}
		}

		auto entities{ manager.GetEntityComponents<TransformComponent, ShapeComponent>() };
		std::vector<Manifold> manifolds;
		for (auto [entity, transform, shape] : entities) {
			if (entity != m) {
				auto manifold{ StaticCollisionCheck(m_t.transform, 
													transform.transform, 
													m_s.shape, 
													shape.shape) 
				};
				Print(manifold.penetration);
				m_t.transform.position -= manifold.penetration;
			}
		}
		PrintLine();

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
		auto entities{ manager.GetEntityComponents<TransformComponent, ShapeComponent, ColorComponent>() };
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
	ecs::Entity m;
	ecs::Manager manager;
};

int main(int c, char** v) {

	Engine::Start<Test>("Squirhell", { 800, 600 }, 60);

	return 0;
}