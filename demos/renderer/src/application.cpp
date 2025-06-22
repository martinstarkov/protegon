
#include "components/draw.h"
#include "components/drawable.h"
#include "core/game.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
#include "rendering/graphics/circle.h"
#include "rendering/graphics/rect.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/resources/shader.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 800, 800 };
constexpr int start_test_index{ 2 };

using SceneBuilder = std::function<void(Scene&)>;
std::vector<SceneBuilder> tests;

RNG<float> pos_rngx{ 0.0f, static_cast<float>(window_size.x) };
RNG<float> pos_rngy{ 0.0f, static_cast<float>(window_size.y) };
RNG<float> size_rng{ 10.0f, 70.0f };
RNG<float> light_radius_rng{ 10.0f, 200.0f };
RNG<float> intensity_rng{ 0.0f, 10.0f };

class Blur : public Drawable<Blur> {
public:
	Blur() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode	   = BlendMode::None;
		render_state.shader_passes = { impl::ShaderPass{ game.shader.Get<ScreenShader::Blur>(),
														 nullptr } };
		ctx.AddShader(entity, render_state, BlendMode::None, color::Transparent, true);
	}
};

class Grayscale : public Drawable<Grayscale> {
public:
	Grayscale() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode	   = BlendMode::None;
		render_state.shader_passes = { impl::ShaderPass{ game.shader.Get<ScreenShader::Grayscale>(),
														 nullptr } };
		ctx.AddShader(entity, render_state, BlendMode::None, color::Transparent, true);
	}
};

void CreateBlur(Scene& scene) {
	auto blur{ scene.CreateEntity() };

	blur.SetDraw<Blur>();
	blur.Show();
}

void CreateGrayscale(Scene& scene) {
	auto grayscale{ scene.CreateEntity() };

	grayscale.SetDraw<Grayscale>();
	grayscale.Show();
}

void GenerateTestCases() {
	LoadResource("test", "resources/test1.jpg");

	auto rect = [](Scene& s) {
		CreateRect(s, { 100, 100 }, { 50, 50 }, color::Red, -1.0f);

		PTGN_LOG("Rect");
	};

	auto circle = [](Scene& s) {
		CreateCircle(s, { 200, 200 }, 30.0f, color::Blue, -1.0f);
		PTGN_LOG("Circle");
	};

	auto sprite = [](Scene& s) {
		CreateSprite(s, "test").SetPosition({ 500, 500 });
		PTGN_LOG("Sprite");
	};

	auto light = [](Scene& s) {
		CreatePointLight(s, { 400, 400 }, 100.0f, color::Purple, 1.0f, 1.0f);
		PTGN_LOG("Point light");
	};

	auto fx = [](Scene& s) {
		CreateGrayscale(s);
		CreateBlur(s);
		PTGN_LOG("Grayscale");
		PTGN_LOG("Blur");
		// CreateFX(s, "Bloom");
	};

	std::vector<std::function<void(Scene&)>> primitives = { rect, circle, sprite, light, fx };

	// Generate all combinations of 1, 2, and 3 object creations in different orders
	for (size_t i = 0; i < primitives.size(); ++i) {
		tests.emplace_back([=](Scene& s) { primitives[i](s); });

		for (size_t j = 0; j < primitives.size(); ++j) {
			if (j == i) {
				continue;
			}
			tests.emplace_back([=](Scene& s) {
				primitives[i](s);
				primitives[j](s);
			});

			for (size_t k = 0; k < primitives.size(); ++k) {
				if (k == i || k == j) {
					continue;
				}
				tests.emplace_back([=](Scene& s) {
					primitives[i](s);
					primitives[j](s);
					primitives[k](s);
				});
			}
		}
	}

	// Add one test with all 4
	tests.emplace_back([](Scene& s) {
		CreateRect(s, { 100, 100 }, { 40, 40 }, color::Magenta, -1);
		CreateCircle(s, { 200, 200 }, 35.0f, color::Cyan, -1);
		CreateSprite(s, "test").SetPosition({ 500, 500 });
		CreatePointLight(s, { 400, 400 }, 90.0f, color::Orange, 1.0f, 2.0f);
		// CreateFX(s, "Grain");
		PTGN_LOG("All 4 test case");
	});

	// Add empty scene test
	tests.emplace_back([](Scene& s) { PTGN_LOG("Empty scene"); });
}

struct RendererScene : public Scene {
	int test_index{ start_test_index };

	RendererScene() {
		GenerateTestCases();
	}

	void CycleTest(bool condition, int amount) {
		if (!condition) {
			return;
		}
		test_index = Mod(test_index + amount, static_cast<int>(tests.size()));
		ReEnter();
	}

	void Enter() override {
		PTGN_LOG("-------- Test ", test_index, " --------");
		PTGN_ASSERT(test_index < tests.size());
		if (tests[test_index]) {
			std::invoke(tests[test_index], *this);
		}
	}

	void Update() override {
		CycleTest(game.input.KeyDown(Key::Q), -1);
		CycleTest(game.input.KeyDown(Key::E), 1);
	}
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
	game.Init("RendererScene", window_size, color::White);
	game.scene.Enter<RendererScene>("");
	return 0;
}
