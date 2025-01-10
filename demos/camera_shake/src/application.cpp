#include "protegon/protegon.h"

using namespace ptgn;

class CameraShakeButtons : public Scene {
public:
	Grid<Button> grid{ { 1, 5 } };

	CameraShake& GetShake();

	void Init() override {
		grid.Set({ 0, 0 }, CreateButton("Reset Shake", [&](){
			GetShake().Reset();
		}));
		grid.Set({ 0, 1 }, CreateButton("Induce 0.10 Shake", [&](){
			GetShake().Induce(0.1f);
		}));
		grid.Set({ 0, 2 }, CreateButton("Induce 0.25 Shake", [&](){
			GetShake().Induce(0.25f);
		}));
		grid.Set({ 0, 3 }, CreateButton("Induce 0.75 Shake", [&](){
			GetShake().Induce(0.5f);
		}));
		grid.Set({ 0, 4 }, CreateButton("Induce 1.00 Shake", [&](){
			GetShake().Induce(1.0f);
		}));

		V2_float screen_offset{ 10, 30 };
		V2_float offset{ 6, 6 };
		V2_float size{ 200, 50 };

		grid.ForEach([&](auto coord, Button& b) {
			b.SetRect({ screen_offset + (offset + size) * coord, size, Origin::TopLeft });
		});
	}

	void Update() override {
		Text{ "WASD to move", color::Black }.Draw({ { 0, 0 }, {}, Origin::TopLeft });

		grid.ForEachElement([](Button& b) {
			b.Draw();
		});
	}

	Button CreateButton(std::string_view content, const ButtonCallback& on_activate) {
		Button b;
		b.Set<ButtonProperty::BackgroundColor>(color::Gold);
		b.Set<ButtonProperty::Bordered>(true);
		b.Set<ButtonProperty::BorderColor>(color::LightGray);
		b.Set<ButtonProperty::BorderThickness>(3.0f);
		Text text{ content, color::Black };
		b.Set<ButtonProperty::Text>(text);
		b.Set<ButtonProperty::OnActivate>(on_activate);
		return b;
	}
};

class CameraShakeExample : public Scene {
public:
	ecs::Manager manager;
	ecs::Entity player;

	float speed{ 50.0f };

	void Init() override {
		manager.Reset();

		player = manager.CreateEntity();

		player.Add<Transform>(V2_float{ 60.0f, 60.0f });
		player.Add<CameraShake>();

		manager.Refresh();

		game.scene.LoadActive<CameraShakeButtons>("camera_shake_buttons");
	}

	void Update() override {
		auto& cam_shake = player.Get<CameraShake>();

		cam_shake.Update();

		float dt = game.dt();

		auto& p = player.Get<Transform>().position;
		if (game.input.KeyPressed(Key::W)) {
			p.y -= speed * dt;
		}
		if (game.input.KeyPressed(Key::S)) {
			p.y += speed * dt;
		}
		if (game.input.KeyPressed(Key::A)) {
			p.x -= speed * dt;
		}
		if (game.input.KeyPressed(Key::D)) {
			p.x += speed * dt;
		}

		auto cam = game.camera.GetPrimary();
		cam.SetPosition(p + cam_shake.local_position);
		cam.SetRotation(cam_shake.local_rotation);

		Draw();
	}

	void Draw() {
		Rect{ { 200, 200 }, { 300, 300 }, Origin::TopLeft }.Draw(color::Gray);
		DrawRect(
			player, { player.Get<Transform>().position, V2_float{ 30.0f, 30.0f }, Origin::Center }
		);
		Rect{ { 0, 0 }, { 50.0f, 50.0f }, Origin::TopLeft }.Draw(color::Orange);
	}
};

CameraShake& CameraShakeButtons::GetShake() {
	PTGN_ASSERT(game.scene.Has("camera_shake"), "Failed to find camera shake main scene");
	return game.scene.Get<CameraShakeExample>("camera_shake")->player.Get<CameraShake>();
}

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("CameraShake");
	game.scene.LoadActive<CameraShakeExample>("camera_shake");
	return 0;
}
