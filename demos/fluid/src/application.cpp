#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/game.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "events/mouse.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };

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

	FluidContainer(const V2_int& size, float dt, float diff, float visc) :
		size{ size }, length{ size.x * size.y }, dt{ dt }, diff{ diff }, visc{ visc } {
		px.resize(length, 0);
		py.resize(length, 0);
		x.resize(length, 0);
		y.resize(length, 0);
		previous_density.resize(length, 0);
		density.resize(length, 0);
	}

	~FluidContainer() = default;

	// Reset fluid to empty.
	void Reset() {
		std::fill(px.begin(), px.end(), 0.0f);
		std::fill(py.begin(), py.end(), 0.0f);
		std::fill(x.begin(), x.end(), 0.0f);
		std::fill(y.begin(), y.end(), 0.0f);
		std::fill(previous_density.begin(), previous_density.end(), 0.0f);
		std::fill(density.begin(), density.end(), 0.0f);
	}

	// Fade density over time.
	void DecreaseDensity(float fraction = 0.999) {
		for (auto& d : density) {
			d *= fraction;
		}
	}

	// Add density to the density field.
	void AddDensity(int xcoord, int ycoord, float amount, int radius = 0) {
		if (xcoord < 0 || xcoord > size.x - 1 || ycoord < 0 || ycoord > size.y - 1) {
			return;
		}

		if (radius > 0) {
			auto ycoordsize{ ycoord * size.x };
			for (int j{ -radius }; j <= radius; ++j) {
				auto row{ ycoordsize + j * size.x };
				for (int i{ -radius }; i <= radius; ++i) {
					if (i * i + j * j <= radius * radius) {
						auto index{ xcoord + i + row };
						this->density[index] += amount;
					}
				}
			}
		} else {
			auto index{ xcoord + ycoord * size.x };
			this->density[index] += amount;
		}
	}

	// Add velocity to the velocity field.
	void AddVelocity(int xcoord, int ycoord, float pxs, float pys) {
		if (xcoord < 0 || xcoord > size.x - 1 || ycoord < 0 || ycoord > size.y - 1) {
			return;
		}

		auto index{ xcoord + ycoord * size.x };
		this->x[index] += pxs;
		this->y[index] += pys;
	}

	// Fluid specific operations

	// Set boundaries to opposite of adjacent layer.
	void SetBoundaries(int b, std::vector<float>& xs) const {
		for (int i{ 1 }; i < size.x - 1; ++i) {
			int top					= size.x + i;
			int bottom				= length - 2 * size.x + i;
			xs[i]					= (b == 2 ? -xs[top] : xs[top]);
			xs[length - size.x + i] = (b == 2 ? -xs[bottom] : xs[bottom]);
		}

		for (int j{ 1 }; j < size.y - 1; ++j) {
			int row				 = j * size.x;
			xs[row]				 = (b == 1 ? -xs[row + 1] : xs[row + 1]);
			xs[row + size.x - 1] = (b == 1 ? -xs[row + size.x - 2] : xs[row + size.x - 2]);
		}

		// Set corner boundaries
		xs[0] = 0.33f * (xs[1] + xs[size.x] + xs[0]);
		xs[length - size.x] =
			0.33f * (xs[length - size.x + 1] + xs[length - 2 * size.x] + xs[length - size.x]);
		xs[size.x - 1] = 0.33f * (xs[size.x - 2] + xs[2 * size.x - 1] + xs[size.x - 1]);
		xs[length - 1] = 0.33f * (xs[length - 2] + xs[length - size.x - 1] + xs[length - 1]);
	}

	// Solve linear differential equation of density / velocity.
	void LinSolve(
		int b, std::vector<float>& xs, std::vector<float>& x0, float a, float c,
		std::size_t iterations
	) const {
		float c_reciprocal{ 1.0f / c };
		for (std::size_t iteration{ 0 }; iteration < iterations; ++iteration) {
			for (int j{ 1 }; j < size.y - 1; ++j) {
				auto row{ j * size.x };
				for (int i{ 1 }; i < size.x - 1; ++i) {
					auto index{ row + i };

					xs[index] =
						(x0[index] + a * (xs[index + 1] + xs[index] + xs[index - 1] +
										  xs[index + size.x] + xs[index] + xs[index - size.x])) *
						c_reciprocal;
				}
			}
			SetBoundaries(b, xs);
		}
	}

	// Diffuse density / velocity outward at each step.
	void Diffuse(
		int b, std::vector<float>& xs, std::vector<float>& x0, float diffusion, float delta_time,
		std::size_t iterations
	) {
		float a{ delta_time * diffusion * (size.x - 2) * (size.y - 2) };
		LinSolve(b, xs, x0, a, 1 + 6 * a, iterations);
	}

	// Converse 'mass' of density / velocity fields.
	void Project(
		std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p,
		std::vector<float>& div, std::size_t iterations
	) {
		for (int j{ 1 }; j < size.y - 1; ++j) {
			auto row{ j * size.x };
			for (int i{ 1 }; i < size.x - 1; ++i) {
				auto index{ row + i };
				div[index] = -0.5f * ((vx[index + 1] - vx[index - 1]) / size.x +
									  (vy[index + size.x] - vy[index - size.x]) / size.y);
				p[index]   = 0;
			}
		}

		SetBoundaries(0, div);
		SetBoundaries(0, p);

		LinSolve(0, p, div, 1, 6, iterations);

		for (int j{ 1 }; j < size.y - 1; ++j) {
			auto row{ j * size.x };
			for (int i{ 1 }; i < size.x - 1; ++i) {
				auto index{ row + i };
				vx[index] -= 0.5f * (p[index + 1] - p[index - 1]) * size.x;
				vy[index] -= 0.5f * (p[index + size.x] - p[index - size.x]) * size.y;
			}
		}

		SetBoundaries(1, vx);
		SetBoundaries(2, vy);
	}

	// Move density / velocity within the field to the next step.
	void Advect(
		int b, std::vector<float>& d, std::vector<float>& d0, std::vector<float>& u,
		std::vector<float>& v, float delta_time
	) {
		float dt0x{ delta_time * size.x };
		float dt0y{ delta_time * size.y };
		for (int j{ 1 }; j < size.y - 1; ++j) {
			auto row{ j * size.x };
			for (int i{ 1 }; i < size.x - 1; ++i) {
				auto index{ row + i };

				float xs{ i - dt0x * u[index] };
				float ys{ j - dt0y * v[index] };
				xs		= std::clamp(xs, 0.5f, size.x + 0.5f);
				auto i0 = (int)xs;
				auto i1 = i0 + 1;
				ys		= std::clamp(ys, 0.5f, size.y + 0.5f);
				auto j0 = (int)ys;
				auto j1 = j0 + 1;
				float s1{ xs - i0 };
				float s0{ 1 - s1 };
				float t1{ ys - j0 };
				float t0{ 1 - t1 };
				d[index] = s0 * (t0 * d0[i0 + j0 * size.x] + t1 * d0[i0 + j1 * size.x]) +
						   s1 * (t0 * d0[i1 + j0 * size.x] + t1 * d0[i1 + j1 * size.x]);
			}
		}
		SetBoundaries(b, d);
	}

	// Update the fluid.
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

private:
	// Clamp value to a range.
	template <typename T>
	inline T Clamp(T value, T low, T high) {
		return value >= high ? high : value <= low ? low : value;
	}
};

class FluidScene : public Scene {
public:
	const V2_float scale{ 6, 6 };
	FluidContainer fluid{ window_size / scale, 0.1f, 0.0001f,
						  0.000001f }; // Dt, Diffusion, Viscosity

	V2_float gravity;				   // Initial gravity

	float gravity_increment{ 1.0f };   // Increment by which gravity increases / decreases

	void Update() override {
		// Reset the screen.
		if (game.input.KeyDown(Key::Space)) {
			fluid.Reset();
		}

		// Reset gravity.
		if (game.input.KeyDown(Key::R)) {
			gravity = {};
		}
		// Increment gravity.
		if (game.input.KeyDown(Key::Down)) {
			gravity.y += gravity_increment;
		} else if (game.input.KeyDown(Key::Up)) {
			gravity.y -= gravity_increment;
		} else if (game.input.KeyDown(Key::Left)) {
			gravity.x -= gravity_increment;
		} else if (game.input.KeyDown(Key::Right)) {
			gravity.x += gravity_increment;
		}
		// Add fluid.
		if (game.input.MousePressed(Mouse::Left)) {
			// Add dye.
			auto mouse_position{ game.input.GetMousePosition() };
			V2_int pos{ mouse_position / scale };
			fluid.AddDensity(pos.x, pos.y, 1000, static_cast<int>(10.0f / scale.x));
			// Add gravity vector.
			fluid.AddVelocity(pos.x, pos.y, gravity.x, gravity.y);
		}

		// Fade overall dye levels slowly over time.
		fluid.DecreaseDensity();

		// Update fluid.
		fluid.Update();

		Draw();
	}

	void Draw() {
		static bool density_graph{ false };

		if (game.input.KeyDown(Key::D)) {
			density_graph = !density_graph;
		}

		for (int j{ 0 }; j < fluid.size.y; ++j) {
			auto row{ fluid.size.x * j };
			for (int i{ 0 }; i < fluid.size.x; ++i) {
				V2_int position{ i, j };
				Color color{ 0, 0, 0, 255 };

				auto index{ i + row };

				auto density{ fluid.density[index] };

				color.r = density > 255 ? 255 : static_cast<std::uint8_t>(density);

				if (density_graph) {
					color.g = static_cast<std::uint8_t>(density);
					if (density < 255.0f * 2.0f && density > 255.0f) {
						color.g -= 255;
					}
				}

				DrawDebugRect(position * scale, scale, color, Origin::TopLeft, -1.0f);
			}
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"Fluid: Click (add), Arrow keys (shift vector field), R (reset vector field), Space (reset "
		"fluid)",
		window_size
	);
	game.scene.Enter<FluidScene>("");
	return 0;
}