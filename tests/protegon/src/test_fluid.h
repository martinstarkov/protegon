#include <cstdint>
#include <memory>
#include <new>
#include <vector>

#include "camera/camera.h"
#include "common.h"
#include "core/game.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/origin.h"

class FluidContainer {
public:
	V2_int size;

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
		size{ size }, dt{ dt }, diff{ diff }, visc{ visc } {
		auto s{ size.x * size.y };
		px.resize(s, 0);
		py.resize(s, 0);
		x.resize(s, 0);
		y.resize(s, 0);
		previous_density.resize(s, 0);
		density.resize(s, 0);
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

	// Get clamped index based off of coordinates.
	static std::size_t IX(std::size_t x, std::size_t y, const V2_int& size) {
		// Clamp coordinates.
		if (x < 0) {
			x = 0;
		}
		if (x > size.x - 1) {
			x = size.x - 1;
		}
		if (y < 0) {
			y = 0;
		}
		if (y > size.y - 1) {
			y = size.y - 1;
		}

		return (y * size.x) + x;
	}

	// Add density to the density field.
	void AddDensity(std::size_t x, std::size_t y, float amount, int radius = 0) {
		if (radius > 0) {
			for (int i{ -radius }; i <= radius; ++i) {
				for (int j{ -radius }; j <= radius; ++j) {
					if (i * i + j * j <= radius * radius) {
						auto index{ IX(x + i, y + j, size) };
						this->density[index] += amount;
					}
				}
			}
		} else {
			auto index{ IX(x, y, size) };
			this->density[index] += amount;
		}
	}

	// Add velocity to the velocity field.
	void AddVelocity(std::size_t x, std::size_t y, float px, float py) {
		auto index{ IX(x, y, size) };
		this->x[index] += px;
		this->y[index] += py;
	}

	// Fluid specific operations

	// Set boundaries to opposite of adjacent layer.
	void SetBnd(int b, std::vector<float>& x, const V2_int& N) {
		for (std::size_t i{ 1 }; i < N.x - 1; ++i) {
			auto top{ IX(i, 1, N) };
			auto bottom{ IX(i, N.y - 2, N) };
			x[IX(i, 0, N)]		 = b == 2 ? -x[top] : x[top];
			x[IX(i, N.y - 1, N)] = b == 2 ? -x[bottom] : x[bottom];
		}

		for (std::size_t j{ 1 }; j < N.y - 1; ++j) {
			auto left{ IX(1, j, N) };
			auto right{ IX(N.x - 2, j, N) };
			x[IX(0, j, N)]		 = b == 1 ? -x[left] : x[left];
			x[IX(N.x - 1, j, N)] = b == 1 ? -x[right] : x[right];
		}

		// Set corner boundaries
		x[IX(0, 0, N)] = 0.33f * (x[IX(1, 0, N)] + x[IX(0, 1, N)] + x[IX(0, 0, N)]);
		x[IX(0, N.y - 1, N)] =
			0.33f * (x[IX(1, N.y - 1, N)] + x[IX(0, N.y - 2, N)] + x[IX(0, N.y - 1, N)]);
		x[IX(N.x - 1, 0, N)] =
			0.33f * (x[IX(N.x - 2, 0, N)] + x[IX(N.x - 1, 1, N)] + x[IX(N.x - 1, 0, N)]);
		x[IX(N.x - 1, N.y - 1, N)] =
			0.33f *
			(x[IX(N.x - 2, N.y - 1, N)] + x[IX(N.x - 1, N.y - 2, N)] + x[IX(N.x - 1, N.y - 1, N)]);
	}

	// Solve linear differential equation of density / velocity.
	void LinSolve(
		int b, std::vector<float>& x, std::vector<float>& x0, float a, float c,
		std::size_t iterations, const V2_int& N
	) {
		float c_reciprocal{ 1.0f / c };
		for (std::size_t iteration{ 0 }; iteration < iterations; ++iteration) {
			for (std::size_t j{ 1 }; j < N.y - 1; ++j) {
				for (std::size_t i{ 1 }; i < N.x - 1; ++i) {
					auto index{ IX(i, j, N) };
					x[index] = (x0[index] +
								a * (x[IX(i + 1, j, N)] + x[IX(i - 1, j, N)] + x[IX(i, j + 1, N)] +
									 x[IX(i, j - 1, N)] + x[index] + x[index])) *
							   c_reciprocal;
				}
			}
			SetBnd(b, x, N);
		}
	}

	// Diffuse density / velocity outward at each step.
	void Diffuse(
		int b, std::vector<float>& x, std::vector<float>& x0, float diff, float dt,
		std::size_t iterations, const V2_int& N
	) {
		float a{ dt * diff * (N.x - 2) * (N.y - 2) };
		LinSolve(b, x, x0, a, 1 + 6 * a, iterations, N);
	}

	// Converse 'mass' of density / velocity fields.
	void Project(
		std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p,
		std::vector<float>& div, std::size_t iterations, const V2_int& N
	) {
		for (std::size_t j{ 1 }; j < N.y - 1; ++j) {
			for (std::size_t i{ 1 }; i < N.x - 1; ++i) {
				auto index{ IX(i, j, N) };
				div[index] = -0.5f * ((vx[IX(i + 1, j, N)] - vx[IX(i - 1, j, N)]) / N.x +
									  (vy[IX(i, j + 1, N)] - vy[IX(i, j - 1, N)]) / N.y);
				p[index]   = 0;
			}
		}

		SetBnd(0, div, N);
		SetBnd(0, p, N);

		LinSolve(0, p, div, 1, 6, iterations, N);

		for (std::size_t j{ 1 }; j < N.y - 1; ++j) {
			for (std::size_t i{ 1 }; i < N.x - 1; ++i) {
				auto index{ IX(i, j, N) };
				vx[index] -= 0.5f * (p[IX(i + 1, j, N)] - p[IX(i - 1, j, N)]) * N.x;
				vy[index] -= 0.5f * (p[IX(i, j + 1, N)] - p[IX(i, j - 1, N)]) * N.y;
			}
		}

		SetBnd(1, vx, N);
		SetBnd(2, vy, N);
	}

	// Move density / velocity within the field to the next step.
	void Advect(
		int b, std::vector<float>& d, std::vector<float>& d0, std::vector<float>& u,
		std::vector<float>& v, float dt, const V2_int& N
	) {
		float dt0x{ dt * N.x };
		float dt0y{ dt * N.y };
		for (std::size_t i{ 1 }; i < N.x - 1; ++i) {
			for (std::size_t j{ 1 }; j < N.y - 1; ++j) {
				auto index{ IX(i, j, N) };

				float x{ i - dt0x * u[index] };
				float y{ j - dt0y * v[index] };
				x		= Clamp(x, 0.5f, N.x + 0.5f);
				auto i0 = (int)x;
				auto i1 = i0 + 1;
				y		= Clamp(y, 0.5f, N.y + 0.5f);
				auto j0 = (int)y;
				auto j1 = j0 + 1;
				float s1{ x - i0 };
				float s0{ 1 - s1 };
				float t1{ y - j0 };
				float t0{ 1 - t1 };
				d[index] = s0 * (t0 * d0[IX(i0, j0, N)] + t1 * d0[IX(i0, j1, N)]) +
						   s1 * (t0 * d0[IX(i1, j0, N)] + t1 * d0[IX(i1, j1, N)]);
			}
		}
		SetBnd(b, d, N);
	}

	// Update the fluid.
	void Update() {
		Diffuse(1, px, x, visc, dt, 4, size);
		Diffuse(2, py, y, visc, dt, 4, size);
		Project(px, py, x, y, 4, size);
		Advect(1, x, px, px, py, dt, size);
		Advect(2, y, py, px, py, dt, size);
		Project(x, y, px, py, 4, size);
		Diffuse(0, previous_density, density, diff, dt, 4, size);
		Advect(0, density, previous_density, x, y, dt, size);
	}

private:
	// Clamp value to a range.
	template <typename T>
	inline T Clamp(T value, T low, T high) {
		return value >= high ? high : value <= low ? low : value;
	}
};

class FluidTest : public Test {
public:
	const V2_float scale{ 6, 6 };
	const V2_float resolution{ 1280, 720 };
	FluidContainer fluid{ resolution / scale, 0.1f, 0.0001f,
						  0.000001f }; // Dt, Diffusion, Viscosity

	V2_float gravity;				   // Initial gravity

	float gravity_increment{ 1.0f };   // Increment by which gravity increases / decreases

	void Shutdown() override {
		game.window.SetSize({ 800, 800 });
	}

	void Init() override {
		game.window.SetSize(resolution);
	}

	void Update() override {
		// Reset the screen.
		if (game.input.KeyDown(Key::SPACE)) {
			fluid.Reset();
		}
		// Reset gravity.
		if (game.input.KeyDown(Key::R)) {
			gravity = {};
		}
		// Increment gravity.
		if (game.input.KeyDown(Key::DOWN)) {
			gravity.y += gravity_increment;
		} else if (game.input.KeyDown(Key::UP)) {
			gravity.y -= gravity_increment;
		} else if (game.input.KeyDown(Key::LEFT)) {
			gravity.x -= gravity_increment;
		} else if (game.input.KeyDown(Key::RIGHT)) {
			gravity.x += gravity_increment;
		}
		// Add fluid.
		if (game.input.MousePressed(Mouse::Left)) {
			// Add dye.
			auto mouse_position{ game.input.GetMousePosition() };
			fluid.AddDensity(
				mouse_position.x / scale.x, mouse_position.y / scale.y, 1000, 10 / scale.x
			);
			// Add gravity vector.
			fluid.AddVelocity(
				mouse_position.x / scale.x, mouse_position.y / scale.y, gravity.x, gravity.y
			);
		}

		// Fade overall dye levels slowly over time.
		fluid.DecreaseDensity();

		// Update fluid.
		fluid.Update();
	}

	void Draw() override {
		static bool density_graph{ false };
		if (game.input.KeyDown(Key::D)) {
			density_graph = !density_graph;
		}

		for (int i{ 0 }; i < fluid.size.x; ++i) {
			for (int j{ 0 }; j < fluid.size.y; ++j) {
				V2_int position{ i, j };
				Color color{ 0, 0, 0, 255 };

				auto index = fluid.IX(i, j, fluid.size);

				auto density{ fluid.density[index] };

				color.r = density > 255 ? 255 : static_cast<std::uint8_t>(density);

				if (density_graph) {
					color.g = static_cast<std::uint8_t>(density);
					if (density < 255.0f * 2.0f && density > 255.0f) {
						color.g -= 255;
					}
				}

				Rect rect{ position * scale, scale, Origin::TopLeft };
				rect.Draw(color, -1.0f);
			}
		}
	}
};

void TestFluid() {
	std::vector<std::shared_ptr<Test>> tests;

	tests.emplace_back(new FluidTest());

	AddTests(tests);
}