
#include "core/ecs/components/draw.h"
#include "core/ecs/components/drawable.h"
#include "core/ecs/components/sprite.h"
#include "core/app/application.h"
#include "core/scripting/script.h"
#include "core/app/window.h"
#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/materials/shader.h"
#include "renderer/vfx/light.h"
#include "world/scene/scene.h"
#include "world/scene/scene_manager.h"

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

class PostProcessingEffect {
public:
	PostProcessingEffect() {}

	static void Draw(const Entity& entity) {
		impl::DrawShader(entity);
	}
};

PTGN_DRAWABLE_REGISTER(PostProcessingEffect);

Entity CreatePostFX(Scene& scene) {
	auto effect{ scene.CreateEntity() };

	SetDraw<PostProcessingEffect>(effect);
	Show(effect);
	SetBlendMode(effect, BlendMode::ReplaceRGBA);

	return effect;
}

struct WhirlpoolInfo {
	float timescale{ 1.0f };
	float scale{ 0.5f };
	float opacity{ 0.5f };
};

void SetWhirlpoolUniform(Entity entity, const Shader& shader) {
	/*auto transform{ GetDrawTransform(entity) };
	float radius{ radius * Abs(transform.scale.x) };*/

	float time{ game.time() };

	const auto& info = entity.Get<WhirlpoolInfo>();

	shader.SetUniform("u_Time", time / 1000.0f * info.timescale);
	shader.SetUniform("u_Scale", info.scale);
	shader.SetUniform("u_Opacity", info.opacity);
}

Entity CreateWhirlpoolEffect(
	Scene& scene, const WhirlpoolInfo& info = {}, const Color& tint = color::DarkGray
) {
	auto effect{ scene.CreateEntity() };

	SetBlendMode(effect, BlendMode::Blend);
	SetTint(effect, tint);
	effect.Add<impl::UsePreviousTexture>(false);
	effect.Add<WhirlpoolInfo>(info);
	const auto& shader{
		game.shader.TryLoad("whirlpool", "screen_default", "resources/whirlpool.glsl")
	};
	effect.Add<impl::ShaderPass>(shader, &SetWhirlpoolUniform);

	return effect;
}

Entity CreateBlur(Scene& scene) {
	auto blur{ CreatePostFX(scene) };
	blur.Add<impl::ShaderPass>(game.shader.Get("blur"), nullptr);
	return blur;
}

Entity CreateGrayscale(Scene& scene) {
	auto grayscale{ CreatePostFX(scene) };
	grayscale.Add<impl::ShaderPass>(game.shader.Get("grayscale"), nullptr);
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
			std::ranges::sort(combo);
			do {
				all_permutations.push_back(combo);
			} while (std::next_permutation(combo.begin(), combo.end()));
		}
	}

	return all_permutations;
}

float rect_thickness{ -1.0f };
float circle_thickness{ -1.0f };
V2_float rect1_pos{ -100, -100 };
V2_float rect1_size{ 400, 400 };
Color rect1_color{ color::Red };
V2_float rect2_pos{ -100, 100 };
V2_float rect2_size{ 400, 400 };
Color rect2_color{ color::Green };
V2_float circle1_pos{ 100, -100 };
float circle1_radius{ 200 };
Color circle1_color{ color::Blue };
V2_float circle2_pos{ 100, 100 };
float circle2_radius{ 200 };
Color circle2_color{ color::Gold };
V2_float light1_pos{ -200, -200 };
V2_float light2_pos{ 0, -100 };
float light1_radius{ 100.0f };
float light2_radius{ 100.0f };
V2_float sprite1_pos{ -200, -220 };
V2_float sprite2_pos{ 200, -220 };

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
	auto e = CreateSprite(s, "test", pos);
	PTGN_LOG("Sprite: ", pos);
	return e;
}

void TestAddPreFX(Entity e, Entity fx, std::string_view fx_name, std::string_view entity_name) {
	PTGN_LOG("Adding PRE ", fx_name, " to ", entity_name);
	AddPreFX(e, fx);
}

void TestAddPostFX(Entity e, Entity fx, std::string_view fx_name, std::string_view entity_name) {
	PTGN_LOG("Adding POST ", fx_name, " to ", entity_name);
	AddPostFX(e, fx);
}

Entity TestAddGrayscale(Scene& s) {
	PTGN_LOG("Grayscale");
	return CreateGrayscale(s);
}

Entity TestAddBlur(Scene& s) {
	PTGN_LOG("Blur");
	return CreateBlur(s);
}

struct FollowMouseScript : public Script<FollowMouseScript> {
	void OnUpdate() override {
		SetPosition(entity, entity.GetScene().input.GetMousePosition());
		float timescale{ 1000 };
		V2_float size{ V2_float{ Abs(std::sin(game.time() / timescale) * 256),
								 Abs(std::sin(game.time() / timescale) * 256) } +
					   V2_float{ 256, 256 } };
		Sprite{ entity }.SetDisplaySize(size);
	}
};

void GenerateTestCases() {
	LoadResource("test", "resources/test1.jpg");
	LoadResource("noise", "resources/noise.png");

	tests.emplace_back([](Scene& s) { auto sprite{ AddSprite(s, rect1_pos) }; });

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };
		AddSprite(s, rect2_pos);
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPreFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPreFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPreFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
		TestAddPreFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPreFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
		TestAddPreFX(sprite, CreateBlur(s), "blur", "sprite");
		TestAddPreFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPostFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPostFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPostFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite{ AddSprite(s, rect1_pos) };

		TestAddPostFX(sprite, CreateGrayscale(s), "grayscale", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
		TestAddPostFX(sprite, CreateBlur(s), "blur", "sprite");
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);

		TestAddGrayscale(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);

		TestAddGrayscale(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);

		TestAddGrayscale(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);
		TestAddGrayscale(s);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);

		TestAddGrayscale(s);
		TestAddBlur(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);

		TestAddBlur(s);
		TestAddGrayscale(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddSprite(s, rect2_pos);
		TestAddGrayscale(s);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		TestAddBlur(s);
	});

	tests.emplace_back([](Scene& s) {
		AddSprite(s, rect1_pos);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		TestAddBlur(s);
		AddSprite(s, rect2_pos);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		TestAddGrayscale(s);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, { 320, 240 }, rect1_color);
		auto sprite{ AddSprite(s, rect1_pos) };
		TestAddPreFX(sprite, CreateGrayscale(s), "grayscale", "sprite1");
		TestAddPreFX(sprite, CreateBlur(s), "blur", "sprite1");
		SetRotation(sprite, DegToRad(45.0f));
		auto sprite2 = AddSprite(s, rect1_pos);
		TestAddPreFX(sprite2, CreateBlur(s), "blur", "sprite2");
		SetRotation(sprite2, DegToRad(-45.0f));
		auto sprite3 = AddSprite(s, rect1_pos);
		SetRotation(sprite3, DegToRad(-10.0f));
	});

	tests.emplace_back([](Scene& s) {
		// AddRect(s, rect1_pos, rect1_size, rect1_color);

		auto sprite = CreateSprite(s, "noise", rect2_pos);
		TestAddPreFX(
			sprite, CreateWhirlpoolEffect(s, { 0.25f, 0.5f, 0.8f }), "whirlpool", "sprite"
		);
		TestAddPreFX(
			sprite, CreateWhirlpoolEffect(s, { 0.5f, 0.25f, 0.7f }, color::White), "whirlpool",
			"sprite"
		);
		TestAddPreFX(
			sprite, CreateWhirlpoolEffect(s, { 1.0f, 0.5f, 0.7f }, color::White), "whirlpool",
			"sprite"
		);
		TestAddPreFX(sprite, CreateWhirlpoolEffect(s, { 3.0f, 0.2f, 1.0f }), "whirlpool", "sprite");
		TestAddPreFX(sprite, CreateWhirlpoolEffect(s, { 5.0f, 0.1f, 1.0f }), "whirlpool", "sprite");
		AddScript<FollowMouseScript>(sprite);
	});

	tests.emplace_back([](Scene& s) {
		auto r1 = AddRect(s, rect1_pos, rect1_size, rect1_color);
		TestAddPostFX(r1, CreateGrayscale(s), "grayscale", "rect1");
	});

	tests.emplace_back([](Scene& s) {
		auto r1 = AddRect(s, rect1_pos, rect1_size, rect1_color);
		TestAddPostFX(r1, CreateGrayscale(s), "grayscale", "rect1");
		AddRect(s, rect2_pos, rect2_size, rect2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		auto r2 = AddRect(s, rect2_pos, rect2_size, rect2_color);
		TestAddPostFX(r2, CreateGrayscale(s), "grayscale", "rect2");
	});

	tests.emplace_back([](Scene& s) {
		auto effect{ CreateGrayscale(s) };
		auto r1 = AddRect(s, rect1_pos, rect1_size, rect1_color);
		TestAddPostFX(r1, effect, "grayscale", "rect1");
		auto r2 = AddRect(s, rect2_pos, rect2_size, rect2_color);
		TestAddPostFX(r2, effect, "grayscale", "rect2");
	});

	tests.emplace_back([](Scene& s) {
		auto r1 = AddRect(s, rect1_pos, rect1_size, rect1_color);
		TestAddPostFX(r1, CreateGrayscale(s), "grayscale", "rect1");
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		auto r2 = AddRect(s, rect2_pos, rect2_size, rect2_color);
		TestAddPostFX(r2, CreateGrayscale(s), "grayscale", "rect2");
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		auto effect{ CreateGrayscale(s) };
		auto r1 = AddRect(s, rect1_pos, rect1_size, rect1_color);
		TestAddPostFX(r1, effect, "grayscale", "rect1");
		auto r2 = AddRect(s, rect2_pos, rect2_size, rect2_color);
		TestAddPostFX(r2, effect, "grayscale", "rect2");
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		auto c1 = AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		TestAddPostFX(c1, CreateGrayscale(s), "grayscale", "circle1");
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle1_pos, circle1_radius, circle1_color);
		auto c2 = AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		TestAddPostFX(c2, CreateGrayscale(s), "grayscale", "circle2");
	});

	tests.emplace_back([](Scene& s) {
		auto sprite1 = AddSprite(s, circle1_pos);
		TestAddPreFX(sprite1, CreateGrayscale(s), "grayscale", "sprite1");
		TestAddPreFX(sprite1, CreateBlur(s), "blur", "sprite1");
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		auto sprite1 = AddSprite(s, circle1_pos);
		TestAddPreFX(sprite1, CreateGrayscale(s), "grayscale", "sprite1");
		TestAddPreFX(sprite1, CreateBlur(s), "blur", "sprite1");
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		auto sprite1 = AddSprite(s, circle1_pos);
		TestAddPreFX(sprite1, CreateGrayscale(s), "grayscale", "sprite1");
		TestAddPreFX(sprite1, CreateBlur(s), "blur", "sprite1");
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
	});

	tests.emplace_back([](Scene& s) {
		AddRect(s, rect1_pos, rect1_size, rect1_color);
		AddRect(s, rect2_pos, rect2_size, rect2_color);
		AddCircle(s, circle2_pos, circle2_radius, circle2_color);
		auto sprite1 = AddSprite(s, circle1_pos);
		TestAddPreFX(sprite1, CreateGrayscale(s), "grayscale", "sprite1");
		TestAddPreFX(sprite1, CreateBlur(s), "blur", "sprite1");
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

	auto rect1 = [](Scene& s) {
		CreateRect(s, rect1_pos, { 50, 50 }, color::Red, -1.0f);

		PTGN_LOG("Rect");
	};

	auto rect2 = [](Scene& s) {
		CreateRect(s, rect1_pos, { 50, 50 }, color::Red, -1.0f);
		CreateRect(s, rect2_pos, { 50, 50 }, color::Red, -1.0f);

		PTGN_LOG("2x Rect");
	};

	auto circle1 = [](Scene& s) {
		CreateCircle(s, circle1_pos, 30.0f, color::Blue, -1.0f);
		PTGN_LOG("Circle");
	};

	auto circle2 = [](Scene& s) {
		CreateCircle(s, circle1_pos, 30.0f, color::Blue, -1.0f);
		CreateCircle(s, circle2_pos, 30.0f, color::Blue, -1.0f);
		PTGN_LOG("2x Circle");
	};

	auto sprite1 = [](Scene& s) {
		CreateSprite(s, "test", sprite1_pos);
		PTGN_LOG("Sprite");
	};

	auto sprite2 = [](Scene& s) {
		CreateSprite(s, "test", sprite1_pos);
		CreateSprite(s, "test", sprite2_pos);
		PTGN_LOG("2x Sprite");
	};

	auto light1 = [](Scene& s) {
		CreatePointLight(s, light1_pos, light1_radius, color::Purple, 1.0f, 1.0f);
		PTGN_LOG("Point light");
	};

	auto light2 = [](Scene& s) {
		CreatePointLight(s, light1_pos, light1_radius, color::Purple, 1.0f, 1.0f);
		CreatePointLight(s, light2_pos, light2_radius, color::Purple, 1.0f, 1.0f);
		PTGN_LOG("2x Point light");
	};

	auto blur1 = [](Scene& s) {
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
		SetBackgroundColor(color::LightBlue);
		game.window.SetResizable();
		PTGN_LOG("-------- Test ", test_index, " --------");
		PTGN_ASSERT(test_index < tests.size());
		if (tests[test_index]) {
			std::invoke(tests[test_index], *this);
		}
	}

	void Update() override {
		CycleTest(input.KeyDown(Key::Q), -1);
		CycleTest(input.KeyDown(Key::E), 1);
	}
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
	game.Init("RendererScene", window_size);
	game.scene.Enter<RendererScene>("");
	return 0;
}