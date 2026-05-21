#include <algorithm>
#include <cstdint>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/blend_mode.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_input.h"

using namespace ptgn;

constexpr V2_int game_size{ 1280, 720 };

class FluidContainer {
public:
	V2_int size;
	int length{ 0 };

	float dt;
	float diff;
	float visc;

	std::vector<float> px;
	std::vector<float> py;
	std::vector<float> x;
	std::vector<float> y;
	std::vector<float> previous_density;
	std::vector<float> density;

	std::vector<bool> obstacles; // true means obstacle (solid wall)

	FluidContainer(const V2_int& size, float dt, float diff, float visc) :
		size{ size }, length{ size.x * size.y }, dt{ dt }, diff{ diff }, visc{ visc } {
		px.resize(length, 0);
		py.resize(length, 0);
		x.resize(length, 0);
		y.resize(length, 0);
		previous_density.resize(length, 0);
		density.resize(length, 0);
		obstacles.resize(length, false); // Initially no obstacles
	}

	~FluidContainer() = default;

	void Reset() {
		std::fill(px.begin(), px.end(), 0.0f);
		std::fill(py.begin(), py.end(), 0.0f);
		std::fill(x.begin(), x.end(), 0.0f);
		std::fill(y.begin(), y.end(), 0.0f);
		std::fill(previous_density.begin(), previous_density.end(), 0.0f);
		std::fill(density.begin(), density.end(), 0.0f);
		std::fill(obstacles.begin(), obstacles.end(), false); // Reset obstacles if needed
	}

	void DecreaseDensity(float fraction = 0.999f) {
		for (auto& d : density) {
			d *= fraction;
		}
	}

	void AddDensity(int xcoord, int ycoord, float amount, int radius = 0) {
		if (xcoord < 0 || xcoord >= size.x || ycoord < 0 || ycoord >= size.y) {
			return;
		}

		if (radius <= 0) {
			if (auto index{ xcoord + ycoord * size.x }; !obstacles[index]) {
				this->density[index] += amount;
			}
			return;
		}

		auto ycoordsize{ ycoord * size.x };
		for (auto j{ -radius }; j <= radius; ++j) {
			int row = ycoordsize + j * size.x;
			for (auto i{ -radius }; i <= radius; ++i) {
				if (i * i + j * j > radius * radius) {
					continue;
				}

				auto index{ xcoord + i + row };
				if (index >= 0 && index < length && !obstacles[index]) {
					this->density[index] += amount;
				}
			}
		}
	}

	void AddVelocity(int xcoord, int ycoord, float pxs, float pys) {
		if (xcoord < 0 || xcoord >= size.x || ycoord < 0 || ycoord >= size.y) {
			return;
		}

		int index = xcoord + ycoord * size.x;
		if (!obstacles[index]) {
			this->x[index] += pxs;
			this->y[index] += pys;
		}
	}

	void SetBoundaries(int b, std::vector<float>& xs) const {
		for (auto j{ 1uz }; j < size.y - 1uz; ++j) {
			for (auto i{ 1uz }; i < size.x - 1uz; ++i) {
				auto index{ i + j * size.x };
				if (obstacles[index]) {
					xs[index] = 0.0f;
					continue;
				}

				if (b == 1) { // horizontal velocity
					if (obstacles[index - 1] || obstacles[index + 1]) {
						xs[index] = 0.0f;
					}
				} else if (b == 2) { // vertical velocity
					if (obstacles[index - size.x] || obstacles[index + size.x]) {
						xs[index] = 0.0f;
					}
				}
			}
		}

		// Container edges
		for (auto i{ 1uz }; i < size.x - 1uz; ++i) {
			xs[i] = (b == 2 ? -xs[i + size.x] : xs[i + size.x]);
			xs[(size.y - 1uz) * size.x + i] =
				(b == 2 ? -xs[(size.y - 2uz) * size.x + i] : xs[(size.y - 2uz) * size.x + i]);
		}
		for (auto j{ 1uz }; j < size.y - 1uz; ++j) {
			xs[j * size.x] = (b == 1 ? -xs[j * size.x + 1] : xs[j * size.x + 1]);
			xs[j * size.x + size.x - 1uz] =
				(b == 1 ? -xs[j * size.x + size.x - 2] : xs[j * size.x + size.x - 2]);
		}

		auto offset{ length - size.x };

		// Corners
		xs[0]			 = 0.33f * (xs[1] + xs[size.x] + xs[0]);
		xs[size.x - 1uz] = 0.33f * (xs[size.x - 2uz] + xs[2uz * size.x - 1uz] + xs[size.x - 1uz]);
		xs[offset]		 = 0.33f * (xs[offset + 1uz] + xs[length - 2uz * size.x] + xs[offset]);
		xs[length - 1uz] = 0.33f * (xs[length - 2uz] + xs[offset - 1uz] + xs[length - 1uz]);
	}

	void LinSolve(
		int b, std::vector<float>& xs, std::vector<float>& x0, float a, float c,
		std::size_t iterations
	) const {
		PTGN_ASSERT(!NearlyEqual(c, 0.0f));

		auto c_reciprocal{ 1.0f / c };

		for (auto iteration{ 0uz }; iteration < iterations; ++iteration) {
			for (auto j{ 1uz }; j < size.y - 1uz; ++j) {
				auto row{ j * size.x };
				for (auto i{ 1uz }; i < size.x - 1uz; ++i) {
					auto index{ row + i };
					if (obstacles[index]) {
						xs[index] = 0.0f; // zero inside obstacle
						continue;
					}
					xs[index] = (x0[index] + a * (xs[index + 1] + xs[index - 1] +
												  xs[index + size.x] + xs[index - size.x])) *
								c_reciprocal;
				}
			}
			SetBoundaries(b, xs);
		}
	}

	void Diffuse(
		int b, std::vector<float>& xs, std::vector<float>& x0, float diffusion, float delta_time,
		std::size_t iterations
	) {
		float a{ delta_time * diffusion * (static_cast<float>(size.x) - 2.0f) *
				 (static_cast<float>(size.y) - 2.0f) };
		LinSolve(b, xs, x0, a, 1.0f + 4.0f * a, iterations);
	}

	void Project(
		std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p,
		std::vector<float>& div, std::size_t iterations
	) {
		for (auto j{ 1z }; j < size.y - 1uz; ++j) {
			auto row{ j * size.x };
			for (auto i{ 1z }; i < size.x - 1uz; ++i) {
				auto index{ row + i };
				if (obstacles[index]) {
					div[index] = 0.0f;
					p[index]   = 0.0f;
					continue;
				}
				div[index] =
					-0.5f *
					((vx[index + 1uz] - vx[index - 1uz]) / static_cast<float>(size.x) +
					 (vy[index + size.x] - vy[index - size.x]) / static_cast<float>(size.y));
				p[index] = 0.0f;
			}
		}

		SetBoundaries(0, div);
		SetBoundaries(0, p);

		LinSolve(0, p, div, 1, 4, iterations);

		for (auto j{ 1z }; j < size.y - 1uz; ++j) {
			auto row{ j * size.x };
			for (auto i{ 1z }; i < size.x - 1uz; ++i) {
				auto index{ row + i };
				if (obstacles[index]) {
					vx[index] = 0.0f;
					vy[index] = 0.0f;
					continue;
				}
				vx[index] -= 0.5f * (p[index + 1uz] - p[index - 1uz]) * static_cast<float>(size.x);
				vy[index] -=
					0.5f * (p[index + size.x] - p[index - size.x]) * static_cast<float>(size.y);
			}
		}

		SetBoundaries(1, vx);
		SetBoundaries(2, vy);
	}

	void Advect(
		int b, std::vector<float>& d, std::vector<float>& d0, std::vector<float>& u,
		std::vector<float>& v, float delta_time
	) {
		auto dt0x{ delta_time * static_cast<float>(size.x) };
		auto dt0y{ delta_time * static_cast<float>(size.y) };
		for (auto j{ 1z }; j < size.y - 1uz; ++j) {
			auto row{ j * size.x };
			for (auto i{ 1z }; i < size.x - 1uz; ++i) {
				auto index{ row + i };

				if (obstacles[index]) {
					d[index] = 0.0f;
					continue;
				}

				auto xs{ static_cast<float>(i) - dt0x * u[index] };
				auto ys{ static_cast<float>(j) - dt0y * v[index] };

				xs = std::clamp(xs, 0.5f, static_cast<float>(size.x) - 1.0f, 0.5f);
				ys = std::clamp(ys, 0.5f, static_cast<float>(size.y) - 1.0f, 0.5f);

				auto i0{ static_cast<int>(xs) };
				auto i1{ i0 + 1 };
				auto j0{ static_cast<int>(ys) };
				auto j1{ j0 + 1 };

				// Avoid sampling inside obstacles:
				auto i0j0{ i0 + j0 * size.x };
				auto i0j1{ i0 + j1 * size.x };
				auto i1j0{ i1 + j0 * size.x };
				auto i1j1{ i1 + j1 * size.x };

				if (obstacles[i0j0] || obstacles[i0j1] || obstacles[i1j0] || obstacles[i1j1]) {
					d[index] = 0.0f;
					continue;
				}

				auto s1{ xs - static_cast<float>(i0) };
				auto s0{ 1.0f - s1 };
				auto t1{ ys - static_cast<float>(j0) };
				auto t0{ 1.0f - t1 };

				d[index] =
					s0 * (t0 * d0[i0j0] + t1 * d0[i0j1]) + s1 * (t0 * d0[i1j0] + t1 * d0[i1j1]);
			}
		}
		SetBoundaries(b, d);
	}

	void Update() {
		Diffuse(1, px, x, visc, dt, 4);
		Diffuse(2, py, y, visc, dt, 4);
		Project(px, py, x, y, 4);
		Advect(1, x, px, px, py, dt);
		Advect(2, y, py, px, py, dt);
		Project(x, y, px, py, 4);
		Diffuse(0, previous_density, density, diff, dt, 4);
		Advect(0, density, previous_density, x, y, dt);
	}
};

class FluidScene : public Scene {
public:
	V2_float scale{ 6, 6 };
	FluidContainer fluid{ game_size / scale, 0.1f, 0.0001f, 0.000001f };
	V2_float gravity;
	float gravity_increment{ 1.0f };

	bool initialized{ false };

	void OnUpdate() override {
		if (!initialized) {
			// No automatic obstacles here
			initialized = true;
		}

		if (ctx().input.KeyPressed(Key::Space)) {
			fluid.Reset();
		}
		if (ctx().input.KeyPressed(Key::R)) {
			gravity = {};
		}
		if (ctx().input.KeyHeld(Key::Down)) {
			gravity.y += gravity_increment;
		}
		if (ctx().input.KeyHeld(Key::Up)) {
			gravity.y -= gravity_increment;
		}
		if (ctx().input.KeyHeld(Key::Left)) {
			gravity.x -= gravity_increment;
		}
		if (ctx().input.KeyHeld(Key::Right)) {
			gravity.x += gravity_increment;
		}

		// Left click: add fluid
		if (ctx().input.MouseHeld(Mouse::Left)) {
			auto mouse_position = ctx().input.GetMousePosition() + game_size * 0.5f;
			V2_int pos			= mouse_position / scale;
			fluid.AddDensity(pos.x, pos.y, 1000, static_cast<int>(10.0f / scale.x));
			fluid.AddVelocity(pos.x, pos.y, gravity.x, gravity.y);
		}

		// Right click: draw obstacles
		if (ctx().input.MouseHeld(Mouse::Right)) {
			auto mouse_position = ctx().input.GetMousePosition() + game_size * 0.5f;
			V2_int pos			= mouse_position / scale;
			// Make a small brush radius to draw obstacles
			auto brush_radius{ static_cast<int>(3.0f / scale.x) };
			if (brush_radius < 1) {
				brush_radius = 1;
			}

			for (int dy = -brush_radius; dy <= brush_radius; ++dy) {
				for (int dx = -brush_radius; dx <= brush_radius; ++dx) {
					int x = pos.x + dx;
					int y = pos.y + dy;
					if (x >= 0 && x < fluid.size.x && y >= 0 && y < fluid.size.y) {
						if (dx * dx + dy * dy <= brush_radius * brush_radius) {
							fluid.obstacles[x + static_cast<std::size_t>(y) * fluid.size.x] = true;
						}
					}
				}
			}
		}

		// fluid.DecreaseDensity();
		fluid.Update();

		Draw();
	}

	void Draw() {
		static bool density_graph{ false };
		if (ctx().input.KeyPressed(Key::D)) {
			density_graph = !density_graph;
		}

		for (auto j{ 0uz }; j < static_cast<std::size_t>(fluid.size.y); ++j) {
			for (auto i{ 0uz }; i < static_cast<std::size_t>(fluid.size.x); ++i) {
				V2_int position{ i, j };
				Color color{ 0, 0, 0, 255 };
				auto index{ i + j * fluid.size.x };

				if (fluid.obstacles[index]) {
					color = Color{ 255, 255, 255, 255 }; // White for obstacles
				} else {
					auto density = fluid.density[index];
					color.r		 = density > 255 ? 255 : static_cast<std::uint8_t>(density);
					if (density_graph) {
						color.g = static_cast<std::uint8_t>(density);
						if (density > 255 && density < 255 * 2) {
							color.g -= 255;
						}
					}
				}

				ctx().renderer.DrawShape(
					Transform{ -game_size * 0.5f + position * scale }, Rect{ scale }, color,
					Solid{}, Origin::TopLeft, Depth{}, BlendMode::Blend
				);
			}
		}
	}
};

int main(int, char**) {
	Application app{ "Fluid with Obstacles: Click (add), Arrows (flow), R "
					 "(reset gravity), Space (reset fluid), "
					 "D (toggle view)",
					 game_size };
	app.StartWith<FluidScene>();
}
