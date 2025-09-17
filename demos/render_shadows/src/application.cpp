#include <algorithm>
#include <vector>

#include "components/sprite.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "input/input_handler.h"
#include "math/geometry.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/stencil_mask.h"
#include "renderer/vfx/light.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

// TODO: Move LightMap to engine.

using namespace ptgn;

class LightMap {
public:
	static void Filter(RenderTarget& render_target, FilterType type);
};

PTGN_DRAW_FILTER_REGISTER(LightMap);

class ShadowScene : public Scene {
public:
	PointLight mouse_light;
	PointLight static_light;

	std::vector<Line> shadow_segments;

	void AddShadow(Transform t1, Rect r1, Origin origin = Origin::Center) {
		auto verts1{ r1.GetWorldVertices(t1, origin) };
		for (std::size_t i{ 0 }; i < verts1.size(); i++) {
			shadow_segments.emplace_back(verts1[i], verts1[(i + 1) % verts1.size()]);
		}
	}

	void AddShadow(Entity e) {
		if (!e.Has<Rect>()) {
			return;
		}

		auto t1 = GetAbsoluteTransform(e);
		Rect r1{ e.Get<Rect>() };

		AddShadow(t1, r1, GetDrawOrigin(e));
	}

	void AddShadow(Sprite e) {
		auto t1 = GetAbsoluteTransform(e);
		Rect r1{ e.GetDisplaySize() };

		AddShadow(t1, r1, GetDrawOrigin(e));
	}

	void Enter() override {
		// game.renderer.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1.0f));

		game.window.SetResizable();
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		auto rt = CreateRenderTarget(*this, ResizeMode::DisplaySize, true, color::Transparent);
		rt.SetDrawFilter<LightMap>();
		SetBlendMode(rt, BlendMode::MultiplyRGBA);

		V2_float s{ game.renderer.GetGameSize() };

		shadow_segments.emplace_back(-s * 0.5f, V2_float{ s.x * 0.5f, -s.y * 0.5f });
		shadow_segments.emplace_back(V2_float{ s.x * 0.5f, -s.y * 0.5f }, s * 0.5f);
		shadow_segments.emplace_back(s * 0.5f, V2_float{ -s.x * 0.5f, s.y * 0.5f });
		shadow_segments.emplace_back(V2_float{ -s.x * 0.5f, s.y * 0.5f }, -s * 0.5f);

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			auto light	 = CreatePointLight(
				  *this, V2_float{ -rt.GetCamera().GetViewportSize() * 0.5f } + V2_float{ i * step },
				  radius, color, intensity, falloff
			  );
			i++;
			return light;
		};

		static_light = create_light(color::Cyan);
		rt.AddToDisplayList(static_light);

		mouse_light = CreatePointLight(*this, { -300, 300 }, 50.0f, color::Red, 0.8f, 1.0f);
		rt.AddToDisplayList(mouse_light);

		auto sprite2 = CreateSprite(*this, "test", { -200, 150 });

		SetDrawOrigin(sprite2, Origin::TopLeft);

		auto rect2 =
			CreateRect(*this, { 200, 200 }, { 100, 100 }, color::Red, -1.0f, Origin::TopLeft);

		AddShadow(sprite);
		AddShadow(sprite2);
		AddShadow(rect2);
	}

	void Update() override {
		auto pos{ input.GetMousePosition() };
		SetPosition(mouse_light, pos);

		if (input.MousePressed(Mouse::Right)) {
			SetPosition(static_light, pos);
		}
	}
};

void LightMap::Filter(RenderTarget& render_target, FilterType type) {
	auto& display_list{ render_target.GetDisplayList() };
	if (type == FilterType::Pre) {
	} else {
		game.renderer.EnableStencilMask();

		for (auto& entity : display_list) {
			if (!entity.Has<impl::LightProperties>()) {
				continue;
			}

			auto origin{ GetPosition(entity) };

			const auto& shadow_segments{ game.scene.Get<ShadowScene>("").shadow_segments };

			auto visibility_triangles{ GetVisibilityTriangles(origin, shadow_segments) };

			for (const auto& triangle : visibility_triangles) {
				game.renderer.DrawTriangle(
					{}, triangle, GetTint(entity), -1.0f, GetDepth(entity) + 1,
					BlendMode::ReplaceAlpha, entity.GetOrDefault<Camera>(),
					entity.GetOrDefault<PostFX>()
				);
			}
		}

		game.renderer.DrawOutsideStencilMask();

		game.renderer.DrawShape(
			{}, Rect{ game.renderer.GetDisplaySize() }, color::Transparent, -1.0f, Origin::Center,
			{}, BlendMode::ReplaceAlpha, {}, {}, "color"
		);

		game.renderer.DisableStencilMask();
		/*game.renderer.DrawShape(
			{}, Rect{ game.renderer.GetDisplaySize() }, color::Red, -1.0f, Origin::Center, {},
			BlendMode::ReplaceRGBA, {}, {}, "color"
		);*/
	}
}

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("ShadowScene: Right: Move static light", { 800, 800 });
	game.scene.Enter<ShadowScene>("");
	return 0;
}