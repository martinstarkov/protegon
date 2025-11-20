#include "core/app/application.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/input/input_handler.h"
#include "core/input/key.h"
#include "core/input/mouse.h"
#include "ecs/components/origin.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int resolution{ 1280, 720 };

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

	FluidContainer(V2_int size, float dt, float diff, float visc) :
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

	void DecreaseDensity(float fraction = 0.999) {
		for (auto& d : density) {
			d *= fraction;
		}
	}

	void AddDensity(int xcoord, int ycoord, float amount, int radius = 0) {
		if (xcoord < 0 || xcoord >= size.x || ycoord < 0 || ycoord >= size.y) {
			return;
		}

		if (radius > 0) {
			auto ycoordsize{ ycoord * size.x };
			for (int j{ -radius }; j <= radius; ++j) {
				int row = ycoordsize + j * size.x;
				for (int i{ -radius }; i <= radius; ++i) {
					if (i * i + j * j <= radius * radius) {
						int index = xcoord + i + row;
						if (index >= 0 && index < length && !obstacles[index]) {
							this->density[index] += amount;
						}
					}
				}
			}
		} else {
			int index = xcoord + ycoord * size.x;
			if (!obstacles[index]) {
				this->density[index] += amount;
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
		for (int j = 1; j < size.y - 1; ++j) {
			for (int i = 1; i < size.x - 1; ++i) {
				int index = i + j * size.x;
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
		for (int i = 1; i < size.x - 1; ++i) {
			xs[i] = (b == 2 ? -xs[i + size.x] : xs[i + size.x]);
			xs[(size.y - 1) * size.x + i] =
				(b == 2 ? -xs[(size.y - 2) * size.x + i] : xs[(size.y - 2) * size.x + i]);
		}
		for (int j = 1; j < size.y - 1; ++j) {
			xs[j * size.x] = (b == 1 ? -xs[j * size.x + 1] : xs[j * size.x + 1]);
			xs[j * size.x + size.x - 1] =
				(b == 1 ? -xs[j * size.x + size.x - 2] : xs[j * size.x + size.x - 2]);
		}

		// Corners
		xs[0]		   = 0.33f * (xs[1] + xs[size.x] + xs[0]);
		xs[size.x - 1] = 0.33f * (xs[size.x - 2] + xs[2 * size.x - 1] + xs[size.x - 1]);
		xs[length - size.x] =
			0.33f * (xs[length - size.x + 1] + xs[length - 2 * size.x] + xs[length - size.x]);
		xs[length - 1] = 0.33f * (xs[length - 2] + xs[length - size.x - 1] + xs[length - 1]);
	}

	void LinSolve(
		int b, std::vector<float>& xs, std::vector<float>& x0, float a, float c,
		std::size_t iterations
	) const {
		float c_reciprocal = 1.0f / c;
		for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
			for (int j = 1; j < size.y - 1; ++j) {
				int row = j * size.x;
				for (int i = 1; i < size.x - 1; ++i) {
					int index = row + i;
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
		float a = delta_time * diffusion * (size.x - 2) * (size.y - 2);
		LinSolve(b, xs, x0, a, 1 + 4 * a, iterations);
	}

	void Project(
		std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p,
		std::vector<float>& div, std::size_t iterations
	) {
		for (int j = 1; j < size.y - 1; ++j) {
			int row = j * size.x;
			for (int i = 1; i < size.x - 1; ++i) {
				int index = row + i;
				if (obstacles[index]) {
					div[index] = 0.0f;
					p[index]   = 0.0f;
					continue;
				}
				div[index] = -0.5f * ((vx[index + 1] - vx[index - 1]) / size.x +
									  (vy[index + size.x] - vy[index - size.x]) / size.y);
				p[index]   = 0.0f;
			}
		}

		SetBoundaries(0, div);
		SetBoundaries(0, p);

		LinSolve(0, p, div, 1, 4, iterations);

		for (int j = 1; j < size.y - 1; ++j) {
			int row = j * size.x;
			for (int i = 1; i < size.x - 1; ++i) {
				int index = row + i;
				if (obstacles[index]) {
					vx[index] = 0.0f;
					vy[index] = 0.0f;
					continue;
				}
				vx[index] -= 0.5f * (p[index + 1] - p[index - 1]) * size.x;
				vy[index] -= 0.5f * (p[index + size.x] - p[index - size.x]) * size.y;
			}
		}

		SetBoundaries(1, vx);
		SetBoundaries(2, vy);
	}

	void Advect(
		int b, std::vector<float>& d, std::vector<float>& d0, std::vector<float>& u,
		std::vector<float>& v, float delta_time
	) {
		float dt0x = delta_time * size.x;
		float dt0y = delta_time * size.y;
		for (int j = 1; j < size.y - 1; ++j) {
			int row = j * size.x;
			for (int i = 1; i < size.x - 1; ++i) {
				int index = row + i;

				if (obstacles[index]) {
					d[index] = 0.0f;
					continue;
				}

				float xs = i - dt0x * u[index];
				float ys = j - dt0y * v[index];

				xs = std::clamp(xs, 0.5f, size.x - 1.5f);
				ys = std::clamp(ys, 0.5f, size.y - 1.5f);

				int i0 = (int)xs;
				int i1 = i0 + 1;
				int j0 = (int)ys;
				int j1 = j0 + 1;

				// Avoid sampling inside obstacles:
				int i0j0 = i0 + j0 * size.x;
				int i0j1 = i0 + j1 * size.x;
				int i1j0 = i1 + j0 * size.x;
				int i1j1 = i1 + j1 * size.x;

				if (obstacles[i0j0] || obstacles[i0j1] || obstacles[i1j0] || obstacles[i1j1]) {
					d[index] = 0.0f;
					continue;
				}

				float s1 = xs - i0;
				float s0 = 1 - s1;
				float t1 = ys - j0;
				float t0 = 1 - t1;

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
	FluidContainer fluid{ resolution / scale, 0.1f, 0.0001f, 0.000001f };
	V2_float gravity{};
	float gravity_increment{ 1.0f };

	bool initialized{ false };

	void Update() override {
		if (!initialized) {
			// No automatic obstacles here
			initialized = true;
		}

		if (input.KeyDown(Key::Space)) {
			fluid.Reset();
		}
		if (input.KeyDown(Key::R)) {
			gravity = {};
		}
		if (input.KeyDown(Key::Down)) {
			gravity.y += gravity_increment;
		}
		if (input.KeyDown(Key::Up)) {
			gravity.y -= gravity_increment;
		}
		if (input.KeyDown(Key::Left)) {
			gravity.x -= gravity_increment;
		}
		if (input.KeyDown(Key::Right)) {
			gravity.x += gravity_increment;
		}

		// Left click: add fluid
		if (input.MousePressed(Mouse::Left)) {
			auto mouse_position = input.GetMousePosition() + resolution * 0.5f;
			V2_int pos			= mouse_position / scale;
			fluid.AddDensity(pos.x, pos.y, 1000, static_cast<int>(10.0f / scale.x));
			fluid.AddVelocity(pos.x, pos.y, gravity.x, gravity.y);
		}

		// Right click: draw obstacles
		if (input.MousePressed(Mouse::Right)) {
			auto mouse_position = input.GetMousePosition() + resolution * 0.5f;
			V2_int pos			= mouse_position / scale;
			// Make a small brush radius to draw obstacles
			int brush_radius = static_cast<int>(3.0f / scale.x);
			if (brush_radius < 1) {
				brush_radius = 1;
			}

			for (int dy = -brush_radius; dy <= brush_radius; ++dy) {
				for (int dx = -brush_radius; dx <= brush_radius; ++dx) {
					int x = pos.x + dx;
					int y = pos.y + dy;
					if (x >= 0 && x < fluid.size.x && y >= 0 && y < fluid.size.y) {
						if (dx * dx + dy * dy <= brush_radius * brush_radius) {
							fluid.obstacles[x + y * fluid.size.x] = true;
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
		if (input.KeyDown(Key::D)) {
			density_graph = !density_graph;
		}

		for (int j = 0; j < fluid.size.y; ++j) {
			for (int i = 0; i < fluid.size.x; ++i) {
				V2_int position{ i, j };
				Color color{ 0, 0, 0, 255 };
				int index = i + j * fluid.size.x;

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

				Application::Get().render_.DrawRect(
					-resolution * 0.5f + position * scale, scale, color, -1.0f, Origin::TopLeft
				);
			}
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	Application::Get().Init(
		"Fluid with Obstacles: Click (add), Arrows (flow), R (reset gravity), Space (reset fluid), "
		"D (toggle view)",
		resolution
	);
	Application::Get().scene_.Enter<FluidScene>("");
	return 0;
}
