#include "protegon/protegon.h"

using namespace ptgn;

constexpr V2_int window_size{ 1280, 720 };

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
	static std::size_t IX(int xcoord, int ycoord, const V2_int& size) {
		// Clamp coordinates.
		xcoord = std::clamp(xcoord, 0, size.x - 1);
		ycoord = std::clamp(ycoord, 0, size.y - 1);

		return ycoord * size.x + xcoord;
	}

	// Add density to the density field.
	void AddDensity(int xcoord, int ycoord, float amount, int radius = 0) {
		if (radius > 0) {
			for (int i{ -radius }; i <= radius; ++i) {
				for (int j{ -radius }; j <= radius; ++j) {
					if (i * i + j * j <= radius * radius) {
						auto index{ IX(xcoord + i, ycoord + j, size) };
						this->density[index] += amount;
					}
				}
			}
		} else {
			auto index{ IX(xcoord, ycoord, size) };
			this->density[index] += amount;
		}
	}

	// Add velocity to the velocity field.
	void AddVelocity(int xcoord, int ycoord, float pxs, float pys) {
		auto index{ IX(xcoord, ycoord, size) };
		this->x[index] += pxs;
		this->y[index] += pys;
	}

	// Fluid specific operations

	// Set boundaries to opposite of adjacent layer.
	void SetBnd(int b, std::vector<float>& xs, const V2_int& N) {
		for (int i{ 1 }; i < N.x - 1; ++i) {
			auto top{ IX(i, 1, N) };
			auto bottom{ IX(i, N.y - 2, N) };
			xs[IX(i, 0, N)]		  = b == 2 ? -xs[top] : xs[top];
			xs[IX(i, N.y - 1, N)] = b == 2 ? -xs[bottom] : xs[bottom];
		}

		for (int j{ 1 }; j < N.y - 1; ++j) {
			auto left{ IX(1, j, N) };
			auto right{ IX(N.x - 2, j, N) };
			xs[IX(0, j, N)]		  = b == 1 ? -xs[left] : xs[left];
			xs[IX(N.x - 1, j, N)] = b == 1 ? -xs[right] : xs[right];
		}

		// Set corner boundaries
		xs[IX(0, 0, N)] = 0.33f * (xs[IX(1, 0, N)] + xs[IX(0, 1, N)] + xs[IX(0, 0, N)]);
		xs[IX(0, N.y - 1, N)] =
			0.33f * (xs[IX(1, N.y - 1, N)] + xs[IX(0, N.y - 2, N)] + xs[IX(0, N.y - 1, N)]);
		xs[IX(N.x - 1, 0, N)] =
			0.33f * (xs[IX(N.x - 2, 0, N)] + xs[IX(N.x - 1, 1, N)] + xs[IX(N.x - 1, 0, N)]);
		xs[IX(N.x - 1, N.y - 1, N)] =
			0.33f * (xs[IX(N.x - 2, N.y - 1, N)] + xs[IX(N.x - 1, N.y - 2, N)] +
					 xs[IX(N.x - 1, N.y - 1, N)]);
	}

	// Solve linear differential equation of density / velocity.
	void LinSolve(
		int b, std::vector<float>& xs, std::vector<float>& x0, float a, float c,
		std::size_t iterations, const V2_int& N
	) {
		float c_reciprocal{ 1.0f / c };
		for (std::size_t iteration{ 0 }; iteration < iterations; ++iteration) {
			for (int j{ 1 }; j < N.y - 1; ++j) {
				for (int i{ 1 }; i < N.x - 1; ++i) {
					auto index{ IX(i, j, N) };
					xs[index] = (x0[index] + a * (xs[IX(i + 1, j, N)] + xs[IX(i - 1, j, N)] +
												  xs[IX(i, j + 1, N)] + xs[IX(i, j - 1, N)] +
												  xs[index] + xs[index])) *
								c_reciprocal;
				}
			}
			SetBnd(b, xs, N);
		}
	}

	// Diffuse density / velocity outward at each step.
	void Diffuse(
		int b, std::vector<float>& xs, std::vector<float>& x0, float diffusion, float delta_time,
		std::size_t iterations, const V2_int& N
	) {
		float a{ delta_time * diffusion * (N.x - 2) * (N.y - 2) };
		LinSolve(b, xs, x0, a, 1 + 6 * a, iterations, N);
	}

	// Converse 'mass' of density / velocity fields.
	void Project(
		std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& p,
		std::vector<float>& div, std::size_t iterations, const V2_int& N
	) {
		for (int j{ 1 }; j < N.y - 1; ++j) {
			for (int i{ 1 }; i < N.x - 1; ++i) {
				auto index{ IX(i, j, N) };
				div[index] = -0.5f * ((vx[IX(i + 1, j, N)] - vx[IX(i - 1, j, N)]) / N.x +
									  (vy[IX(i, j + 1, N)] - vy[IX(i, j - 1, N)]) / N.y);
				p[index]   = 0;
			}
		}

		SetBnd(0, div, N);
		SetBnd(0, p, N);

		LinSolve(0, p, div, 1, 6, iterations, N);

		for (int j{ 1 }; j < N.y - 1; ++j) {
			for (int i{ 1 }; i < N.x - 1; ++i) {
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
		std::vector<float>& v, float delta_time, const V2_int& N
	) {
		float dt0x{ delta_time * N.x };
		float dt0y{ delta_time * N.y };
		for (int i{ 1 }; i < N.x - 1; ++i) {
			for (int j{ 1 }; j < N.y - 1; ++j) {
				auto index{ IX(i, j, N) };

				float xs{ i - dt0x * u[index] };
				float ys{ j - dt0y * v[index] };
				xs		= std::clamp(xs, 0.5f, N.x + 0.5f);
				auto i0 = (int)xs;
				auto i1 = i0 + 1;
				ys		= std::clamp(ys, 0.5f, N.y + 0.5f);
				auto j0 = (int)ys;
				auto j1 = j0 + 1;
				float s1{ xs - i0 };
				float s0{ 1 - s1 };
				float t1{ ys - j0 };
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

class FluidExample : public Scene {
public:
	const V2_float scale{ 6, 6 };
	FluidContainer fluid{ window_size / scale, 0.1f, 0.0001f,
						  0.000001f }; // Dt, Diffusion, Viscosity

	V2_float gravity;				   // Initial gravity

	float gravity_increment{ 1.0f };   // Increment by which gravity increases / decreases

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

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init(
		"Fluid: Click (add), Arrow keys (shift vector field), R (reset vector field), Space (reset "
		"fluid)",
		window_size
	);
	game.scene.Enter<FluidExample>("fluid");
	return 0;
}