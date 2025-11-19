#include <algorithm>
#include <optional>
#include <vector>

#include "ecs/components/draw.h"
#include "ecs/components/effects.h"
#include "ecs/components/sprite.h"
#include "ecs/components/transform.h"
#include "ecs/entity.h"
#include "core/app/application.h"
#include "ecs/game_object.h"
#include "core/app/manager.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/mouse.h"
#include "math/geometry_utils.h"
#include "math/geometry/line.h"
#include "math/geometry/rect.h"
#include "math/geometry/shape.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "ecs/components/origin.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/material/shader.h"
#include "renderer/stencil_mask.h"
#include "renderer/vfx/light.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_input.h"
#include "scene/scene_manager.h"

// TODO: Move LightMap to engine.

using namespace ptgn;

namespace ptgn {

namespace impl {

struct LightMapInstance {
	// Entities which will form the shadow segments.
	std::vector<Entity> shadow_entities;
	std::vector<PointLight> light_entities;
	// TODO: Draw lights to this render target, and then draw shadows on top.
	GameObject<RenderTarget> light_render_target;
	bool hide_shadow_entities{ false };
	bool shadow_layering{ true };
};

} // namespace impl

struct ShadowDepth : public Depth {
	using Depth::Depth;
};

class LightMap : public Entity {
public:
	LightMap() = default;

	LightMap(const Entity& entity) : Entity{ entity } {}

	void DisableShadowLayering(bool disable = true) {
		auto& light_map{ Get<impl::LightMapInstance>() };
		light_map.shadow_layering = !disable;
	}

	void HideShadowEntities(bool hide = true) {
		auto& light_map{ Get<impl::LightMapInstance>() };
		light_map.hide_shadow_entities = hide;
	}

	void AddShadow(const Entity& entity) {
		auto& light_map{ Get<impl::LightMapInstance>() };

		PTGN_ASSERT(
			GetSpriteOrShape(entity).has_value(), "Cannot add shadow entity which has no shape"
		);

		light_map.shadow_entities.emplace_back(entity);
	}

	void AddLight(const Entity& entity) {
		auto& light_map{ Get<impl::LightMapInstance>() };

		PTGN_ASSERT(
			entity.Has<impl::LightProperties>(), "Cannot add light entity which is not a light"
		);

		light_map.light_entities.emplace_back(entity);
	}

	static void Draw(const Entity& entity) {
		const auto& light_map{ entity.Get<impl::LightMapInstance>() };

		Application::Get().render_.EnableStencilMask();

		const auto add_to_stencil_mask = [entity](const auto& shape, const Transform& transform) {
			Application::Get().render_.DrawShape(
				transform, shape, color::Black, -1.0f, Origin::Center, GetDepth(entity) + 1,
				BlendMode::ReplaceAlpha, entity.GetOrDefault<Camera>(),
				entity.GetOrDefault<PostFX>()
			);
		};

		auto shadows{ GetShadowInfo(light_map.shadow_entities) };

		if (!light_map.hide_shadow_entities) {
			for (const auto& [shape, transform] : shadows) {
				add_to_stencil_mask(shape, transform);
			}
		}

		auto shadow_segments{ GetShadowSegments(shadows) };

		for (const auto& light : light_map.light_entities) {
			if (!light.Has<impl::LightProperties>()) {
				continue;
			}

			auto origin{ GetPosition(light) };

			auto visibility_triangles{ GetVisibilityTriangles(origin, shadow_segments) };

			for (const auto& triangle : visibility_triangles) {
				add_to_stencil_mask(triangle, {});
			}
		}

		Application::Get().render_.DrawOutsideStencilMask();

		Application::Get().render_.DrawShape(
			{}, Rect{ Application::Get().render_.GetDisplaySize() }, color::Black.WithAlpha(0.5f), -1.0f,
			Origin::Center, {}, BlendMode::Blend, {}, {}, "color"
		);

		Application::Get().render_.DisableStencilMask();
	}

private:
	static void AddWorldBoundaries(std::vector<Line>& shadow_segments) {
		auto size{ Application::Get().render_.GetGameSize() };
		auto half_size{ size * 0.5f };

		shadow_segments.emplace_back(-half_size, V2_float{ half_size.x, -half_size.y });
		shadow_segments.emplace_back(V2_float{ half_size.x, -half_size.y }, half_size);
		shadow_segments.emplace_back(half_size, V2_float{ -half_size.x, half_size.y });
		shadow_segments.emplace_back(V2_float{ -half_size.x, half_size.y }, -half_size);
	}

	struct ShadowInfo {
		Shape shape;
		Transform transform;
	};

	static std::vector<ShadowInfo> GetShadowInfo(const std::vector<Entity>& shadow_entities) {
		std::vector<ShadowInfo> shadow_info;

		for (const auto& entity : shadow_entities) {
			if (auto shape{ GetSpriteOrShape(entity) }) {
				auto transform{ GetAbsoluteTransform(entity) };

				transform = OffsetByOrigin(*shape, transform, entity);

				shadow_info.emplace_back(*shape, transform);
			}
		}

		return shadow_info;
	}

	static std::vector<Line> GetShadowSegments(const std::vector<ShadowInfo>& shadows) {
		std::vector<Line> shadow_segments;

		for (const auto& [shape, transform] : shadows) {
			auto edge_info{ GetEdges(shape, transform) };

			shadow_segments.insert(
				shadow_segments.end(), edge_info.edges.begin(), edge_info.edges.end()
			);
		}

		AddWorldBoundaries(shadow_segments);

		return shadow_segments;
	}
};

PTGN_DRAWABLE_REGISTER(LightMap);

LightMap CreateLightMap(Manager& manager) {
	LightMap light_map{ manager.CreateEntity() };

	Show(light_map);
	SetDraw<LightMap>(light_map);

	auto& instance{ light_map.Add<impl::LightMapInstance>() };

	// TODO: Fix.
	// instance.light_render_target = CreateRenderTarget(manager, ResizeMode::DisplaySize, true,
	// color::Transparent);

	return light_map;
}

} // namespace ptgn

class ShadowScene : public Scene {
public:
	PointLight mouse_light;
	PointLight static_light;

	LightMap light_map;

	void Enter() override {
		// Application::Get().render_.SetBackgroundColor(color::White);
		SetBackgroundColor(color::LightBlue.WithAlpha(1.0f));

		Application::Get().window_.SetResizable();
		LoadResource("test", "resources/test1.jpg");

		auto sprite = CreateSprite(*this, "test", { -200, -200 });
		SetDrawOrigin(sprite, Origin::TopLeft);

		float intensity{ 0.5f };
		float radius{ 30.0f };
		float falloff{ 2.0f };

		float step{ 80 };

		const auto create_light = [&](const Color& color) {
			static int i = 1;
			auto light	 = CreatePointLight(
				  *this, -Application::Get().render_.GetGameSize() * 0.5f + V2_float{ i * step }, radius, color,
				  intensity, falloff
			  );
			i++;
			return light;
		};

		static_light = create_light(color::Cyan);

		mouse_light = CreatePointLight(*this, { -300, 300 }, 50.0f, color::Red, 0.8f, 1.0f);

		auto sprite2 = CreateSprite(*this, "test", { -200, 150 });

		SetDrawOrigin(sprite2, Origin::TopLeft);

		auto rect2 =
			CreateRect(*this, { 200, 200 }, { 100, 100 }, color::Red, -1.0f, Origin::TopLeft);

		light_map = CreateLightMap(*this);

		light_map.AddLight(static_light);
		light_map.AddLight(mouse_light);

		light_map.AddShadow(sprite);
		light_map.AddShadow(sprite2);
		light_map.AddShadow(rect2);

		// light_map.HideShadowEntities();
	}

	void Update() override {
		auto pos{ input.GetMousePosition() };
		SetPosition(mouse_light, pos);

		if (input.MousePressed(Mouse::Right)) {
			SetPosition(static_light, pos);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init("ShadowScene: Right: Move static light", { 800, 800 });
	Application::Get().scene_.Enter<ShadowScene>("");
	return 0;
}