
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
constexpr int start_test_index{ 0 };

using SceneBuilder = std::function<void(Scene&)>;
std::vector<SceneBuilder> tests;

RNG<float> pos_rngx{ 0.0f, static_cast<float>(window_size.x) };
RNG<float> pos_rngy{ 0.0f, static_cast<float>(window_size.y) };
RNG<float> size_rng{ 10.0f, 70.0f };
RNG<float> light_radius_rng{ 10.0f, 200.0f };
RNG<float> intensity_rng{ 0.0f, 10.0f };

class PostProcessingEffect : public Drawable<PostProcessingEffect> {
public:
	PostProcessingEffect() {}

	static void Draw(impl::RenderData& ctx, const Entity& entity) {
		impl::RenderState render_state;
		render_state.blend_mode	   = entity.GetBlendMode();
		render_state.shader_passes = entity.Get<impl::ShaderPass>();
		render_state.post_fx	   = entity.GetOrDefault<impl::PostFX>();
		ctx.AddShader(entity, render_state, BlendMode::None, color::Transparent, true);
	}
};

Entity CreatePostProcessingEffect(Scene& scene) {
	auto effect{ scene.CreateEntity() };

	effect.SetDraw<PostProcessingEffect>();
	effect.Show();
	effect.SetBlendMode(BlendMode::None);

	return effect;
}

Entity CreateBlur(Scene& scene) {
	auto blur{ CreatePostProcessingEffect(scene) };
	blur.Add<impl::ShaderPass>(game.shader.Get<ScreenShader::Blur>(), nullptr);
	return blur;
}

Entity CreateGrayscale(Scene& scene) {
	auto grayscale{ CreatePostProcessingEffect(scene) };
	grayscale.Add<impl::ShaderPass>(game.shader.Get<ScreenShader::Grayscale>(), nullptr);
	return grayscale;
}

// Helper to generate all combinations of k elements from base
void GenerateCombinations(
	const std::vector<std::size_t>& base, std::size_t k, std::size_t start,
	std::vector<std::size_t>& current, std::vector<std::vector<std::size_t>>& result
) {
	if (current.size() == k) {
		result.push_back(current);
		return;
	}

	for (std::size_t i = start; i < base.size(); ++i) {
		current.push_back(base[i]);
		GenerateCombinations(base, k, i + 1, current, result);
		current.pop_back();
	}
}

// Main function: generates all permutations of lengths 1 to N of [0, 1, ..., N-1]
std::vector<std::vector<std::size_t>> GenerateNumberPermutations(std::size_t N) {
	std::vector<std::vector<std::size_t>> all_permutations;

	if (N == 0) {
		return all_permutations;
	}

	std::vector<std::size_t> base(N);
	for (std::size_t i = 0; i < N; ++i) {
		base[i] = i;
	}

	for (std::size_t k = 1; k <= N; ++k) {
		std::vector<std::vector<std::size_t>> combinations;
		std::vector<std::size_t> current_comb;
		GenerateCombinations(base, k, 0, current_comb, combinations);

		for (auto& combo : combinations) {
			std::sort(combo.begin(), combo.end());
			do {
				all_permutations.push_back(combo);
			} while (std::next_permutation(combo.begin(), combo.end()));
		}
	}

	return all_permutations;
}

float rect_thickness{ -1.0f };
float circle_thickness{ -1.0f };
V2_float rect1_pos{ 300, 300 };
V2_float rect1_size{ 400, 400 };
Color rect1_color{ color::Red };
V2_float rect2_pos{ 300, 500 };
V2_float rect2_size{ 400, 400 };
Color rect2_color{ color::Green };
V2_float circle1_pos{ 500, 300 };
float circle1_radius{ 200 };
Color circle1_color{ color::Blue };
V2_float circle2_pos{ 500, 500 };
float circle2_radius{ 200 };
Color circle2_color{ color::Gold };

Entity AddRect(Scene& s, V2_float pos, V2_float size, Color color) {
	auto e = CreateRect(s, pos, size, color, rect_thickness);
	PTGN_LOG("Rect: ", color);
	return e;
}

Entity AddCircle(Scene& s, V2_float pos, float radius, Color color) {
	auto e = CreateCircle(s, pos, radius, color, circle_thickness);
	PTGN_LOG("Circle: ", color);
	return e;
}

Entity AddSprite(Scene& s, V2_float pos) {
	auto e = CreateSprite(s, "test");
	e.SetPosition(pos);
	PTGN_LOG("Sprite: ", pos);
	return e;
}

void GenerateTestCases() {
	LoadResource("test", "resources/test1.jpg");

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, { 320, 240 }, rect1_color);
		AddSprite(s, rect1_pos)
			.AddPreFX(CreateGrayscale(s))
			.AddPreFX(CreateBlur(s))
			.SetRotation(DegToRad(45.0f));
		AddSprite(s, rect1_pos).AddPreFX(CreateBlur(s)).SetRotation(DegToRad(-45.0f));
		AddSprite(s, rect1_pos).SetRotation(DegToRad(-10.0f));
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color).AddPostFX(CreateGrayscale(s));
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color).AddPostFX(CreateGrayscale(s));
		AddRect(s, rect2_pos, rect2_size, rect2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color).AddPostFX(CreateGrayscale(s));
	});

	tests.emplace_back([](Scene& s) {
		auto effect{ CreateGrayscale(s) };
		AddRect(s, rect1_pos, rect1_size, rect1_color).AddPostFX(effect);
		AddRect(s, rect2_pos, rect2_size, rect2_color).AddPostFX(effect);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color).AddPostFX(CreateGrayscale(s));
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color).AddPostFX(CreateGrayscale(s));
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		auto effect{ CreateGrayscale(s) };
		AddRect(s, rect1_pos, rect1_size, rect1_color).AddPostFX(effect);
		AddRect(s, rect2_pos, rect2_size, rect2_color).AddPostFX(effect);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color).AddPostFX(CreateGrayscale(s));
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color).AddPostFX(CreateGrayscale(s));
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, circle1_pos).AddPreFX(CreateGrayscale(s)).AddPreFX(CreateBlur(s));
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddSprite(s, circle1_pos).AddPreFX(CreateGrayscale(s)).AddPreFX(CreateBlur(s));
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddSprite(s, circle1_pos).AddPreFX(CreateGrayscale(s)).AddPreFX(CreateBlur(s));
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		AddSprite(s, circle1_pos).AddPreFX(CreateGrayscale(s)).AddPreFX(CreateBlur(s));
	});

	tests.emplace_back([](Scene& s) { AddRect(s, rect1_pos, rect1_size, rect1_color); });
	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
	});

	tests.emplace_back([](Scene& s) { AddCircle(s, circle1_pos, circle1_radius, circle1_color); });
	tests.emplace_back([](Scene& s) {
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	auto rect = [](Scene& s) {
		CreateRect(s, { 100, 100 }, { 50, 50 }, color::Red, -1.0f);

		PTGN_LOG("Rect");
	};

	auto rect2 = [](Scene& s) {
		CreateRect(s, { 100, 100 }, { 50, 50 }, color::Red, -1.0f);
		CreateRect(s, { 100, 200 }, { 50, 50 }, color::Red, -1.0f);

		PTGN_LOG("2x Rect");
	};

	auto circle = [](Scene& s) {
		CreateCircle(s, { 200, 200 }, 30.0f, color::Blue, -1.0f);
		PTGN_LOG("Circle");
	};

	auto circle2 = [](Scene& s) {
		CreateCircle(s, { 200, 200 }, 30.0f, color::Blue, -1.0f);
		CreateCircle(s, { 200, 300 }, 30.0f, color::Blue, -1.0f);
		PTGN_LOG("2x Circle");
	};

	auto sprite = [](Scene& s) {
		CreateSprite(s, "test").SetPosition({ 500, 500 });
		PTGN_LOG("Sprite");
	};

	auto sprite2 = [](Scene& s) {
		CreateSprite(s, "test").SetPosition({ 500, 500 });
		CreateSprite(s, "test").SetPosition({ 500, 700 });
		PTGN_LOG("2x Sprite");
	};

	auto light = [](Scene& s) {
		CreatePointLight(s, { 400, 400 }, 100.0f, color::Purple, 1.0f, 1.0f);
		PTGN_LOG("Point light");
	};

	auto light2 = [](Scene& s) {
		CreatePointLight(s, { 400, 400 }, 100.0f, color::Purple, 1.0f, 1.0f);
		CreatePointLight(s, { 400, 500 }, 100.0f, color::Purple, 1.0f, 1.0f);
		PTGN_LOG("2x Point light");
	};

	auto blur = [](Scene& s) {
		CreateBlur(s);
		PTGN_LOG("Blur");
	};

	auto blur2 = [](Scene& s) {
		CreateBlur(s);
		CreateBlur(s);
		PTGN_LOG("2x Blur");
	};

	std::vector<std::function<void(Scene&)>> primitives = { blur2, rect2, circle2, sprite2,
															light2 };

	auto permutations{ GenerateNumberPermutations(primitives.size()) };
	for (const auto& permutation : permutations) {
		bool skip = false;
		for (auto i = 0; i < permutation.size(); i++) {
			auto index{ permutation[i] };
			if (i == 0 && index == 0) { //  Skip all permutations that start with 0.
				skip = true;
			}
		}
		if (!skip) {
			auto generate = [=](Scene& s) {
				for (auto index : permutation) {
					PTGN_ASSERT(index < primitives.size());
					std::invoke(primitives[index], s);
				}
			};
			tests.emplace_back(generate);
		}
	}
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
