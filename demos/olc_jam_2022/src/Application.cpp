#include "core/Engine.h"
#include "interface/Input.h"
#include "interface/Draw.h"
#include "interface/Window.h"
#include "interface/Scene.h"
#include "utility/Log.h"
#include "math/Noise.h"
#include "math/Hash.h"

using namespace ptgn;

#include <array> // std::array

template <std::size_t W, std::size_t H>
struct FluidSimulation {
	inline static constexpr std::size_t size = W * H;
	inline static constexpr std::size_t iterations = 20;
	using container = std::array<float, size>;

	container u;
	container v;
	container u_p;
	container v_p;
	container dens;
	container dens_p;

	void Init() {
		std::fill(u.begin(), u.end(), 0.0f);
		std::fill(v.begin(), v.end(), 0.0f);
		u_p = u;
		v_p = v;
		std::fill(dens.begin(), dens.end(), 0.0f);
		dens_p = dens;
	}

	std::size_t IX(std::size_t i, std::size_t j) {
		return i + W * j;
	}

	void SetNoiseMap(float* noise, float scale = 1.0f) {
		for (std::size_t i = 0; i < W; ++i) {
			for (std::size_t j = 0; j < H; ++j) {
				auto index{ IX(i, j) };
				u[index] = std::sin(noise[index] * math::two_pi<float>) * scale;
				v[index] = std::cos(noise[index] * math::two_pi<float>) * scale;
			}
		}
	}
	void AddDensityNoiseMap(float* noise, float scale = 1.0f) {
		for (std::size_t i = 0; i < W; ++i) {
			for (std::size_t j = 0; j < H; ++j) {
				auto index{ IX(i, j) };
				dens[index] = noise[index] * scale;
			}
		}
	}
	
	void FadeDensity(float fraction = 0.99f) {
		for (auto& d : dens) {
			d *= fraction;
		}
	}
	void AddDensity(const V2_int& point, float amount, int radius = 0) {
		
		if (radius > 0) {
			// Add density in circle around cursor.
			for (int i{ -radius }; i <= radius; ++i) {
				for (int j{ -radius }; j <= radius; ++j) {
					if (i * i + j * j <= radius * radius) {
						std::size_t index{ IX(point.x + i, point.y + j) };
						if(index < dens.size()) {
							dens[index] += amount;
						}
					}
				}
			}
		} else {
			// Add density at cursor location.
			auto index{ IX(point.x, point.y) };
			assert(index < dens.size());
			dens[index] += amount;
		}
	}
	void AddVelocity(const V2_int& point, const V2_float& amount) {
		auto index{ IX(point.x, point.y) };
		assert(index < u.size());
		assert(index < v.size());
		u[index] += amount.x;
		v[index] += amount.y;
	}
	void SetBoundary(int b, float* x) {
		for (std::size_t j = 1; j <= H - 2; ++j) {
			x[IX(0, j)] = b == 1 ? -x[IX(1, j)] : x[IX(1, j)];
			x[IX(W - 2 + 1, j)] = b == 1 ? -x[IX(W - 2, j)] : x[IX(W - 2, j)];
		}
		for (std::size_t i = 1; i <= W - 2; ++i) {
			x[IX(i, 0)] = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
			x[IX(i, H - 2 + 1)] = b == 2 ? -x[IX(i, H - 2)] : x[IX(i, H - 2)];
		}
		x[IX(0, 0)] = 0.5 * (x[IX(1, 0)] + x[IX(0, 1)]);
		x[IX(0, H - 2 + 1)] = 0.5 * (x[IX(1, H - 2 + 1)] + x[IX(0, H - 2)]);
		x[IX(W - 2 + 1, 0)] = 0.5 * (x[IX(W - 2, 0)] + x[IX(W - 2 + 1, 1)]);
		x[IX(W - 2 + 1, H - 2 + 1)] = 0.5 * (x[IX(W - 2, H - 2 + 1)] + x[IX(W - 2 + 1, H - 2)]);
	}

	void Diffuse(int b, float* x, float* x0, float diff, float dt, std::size_t iterations) {
		float a = dt * diff * (W - 2) * (H - 2);
		for (std::size_t k = 0; k < iterations; ++k) {
			for (std::size_t i = 1; i <= W - 2; ++i) {
				for (std::size_t j = 1; j <= H - 2; ++j) {
					x[IX(i, j)] = (x0[IX(i, j)] + a * (x[IX(i - 1, j)] + x[IX(i + 1, j)] +
													   x[IX(i, j - 1)] + x[IX(i, j + 1)])) / (1 + 4 * a);
				}
			}
			SetBoundary(b, x);
		}
	}
	void Advect(int b, float* d, float* d0, float* u, float* v, float dt) {
		float dt0x = dt * (W - 2);
		float dt0y = dt * (H - 2);
		for (std::size_t i = 1; i <= W - 2; ++i) {
			for (std::size_t j = 1; j <= H - 2; ++j) {
				float x = i - dt0x * u[IX(i, j)]; 
				float y = j - dt0y * v[IX(i, j)];
				if (x < 0.5)
					x = 0.5; 
				if (x > W - 2 + 0.5)
					x = W - 2 + 0.5; 
				float i0 = (int)x;
				float i1 = i0 + 1;
				if (y < 0.5)
					y = 0.5;
				if (y > H - 2 + 0.5)
					y = H - 2 + 0.5;
				float j0 = (int)y;
				float j1 = j0 + 1;
				float s1 = x - i0;
				float s0 = 1 - s1;
				float t1 = y - j0;
				float t0 = 1 - t1;
				d[IX(i, j)] = s0 * (t0 * d0[IX(i0, j0)] + t1 * d0[IX(i0, j1)]) +
					          s1 * (t0 * d0[IX(i1, j0)] + t1 * d0[IX(i1, j1)]);
			}
		}
		SetBoundary(b, d);
	}
	void DensityStep(float* x, float* x0, float* u, float* v, float diff, float dt) {
		Diffuse(0, x0, x, diff, dt, iterations);
		Advect(0, x, x0, u, v, dt);
	}

	void Project(float* u, float* v, float* p, float* div, std::size_t iterations) {
		float hx = 1.0 / (W - 2);
		float hy = 1.0 / (H - 2);
		for (std::size_t i = 1; i <= W - 2; i++) {
			for (std::size_t j = 1; j <= H - 2; j++) {
				div[IX(i, j)] = -0.5 * (hx * (u[IX(i + 1, j)] - u[IX(i - 1, j)]) + 
					                    hy * (v[IX(i, j + 1)] - v[IX(i, j - 1)]));
				p[IX(i, j)] = 0;
			}
		}
		SetBoundary(0, div);
		SetBoundary(0, p);
		for (std::size_t k = 0; k < iterations; ++k) {
			for (std::size_t i = 1; i <= W - 2; ++i) {
				for (std::size_t j = 1; j <= H - 2; ++j) {
					p[IX(i, j)] = (div[IX(i, j)] + p[IX(i - 1, j)] + p[IX(i + 1, j)] +
								   p[IX(i, j - 1)] + p[IX(i, j + 1)]) / 4;
				}
			}
			SetBoundary(0, p);
		}
		for (std::size_t i = 1; i <= W - 2; ++i) {
			for (std::size_t j = 1; j <= H - 2; ++j) {
				u[IX(i, j)] -= 0.5 * (p[IX(i + 1, j)] - p[IX(i - 1, j)]) / hx;
				v[IX(i, j)] -= 0.5 * (p[IX(i, j + 1)] - p[IX(i, j - 1)]) / hy;
			}
		}
		SetBoundary(1, u);
		SetBoundary(2, v);
	}

	void VelocityStep(float* u, float* v, float* u0, float* v0, float visc, float dt) {
		Diffuse(1, u0, u, visc, dt, iterations);
		Diffuse(2, v0, v, visc, dt, iterations);
		Project(u0, v0, u, v, iterations);
		Advect(1, u, u0, u0, v0, dt);
		Advect(2, v, v0, u0, v0, dt);
		Project(u, v, u0, v0, iterations);
	}

	void Step(float diff, float visc, float dt) {
		VelocityStep(u.data(), v.data(), u_p.data(), v_p.data(), visc, dt);
		DensityStep(dens.data(), dens_p.data(), u.data(), v.data(), diff, dt);
		dens_p = dens;
		u_p = u;
		v_p = v;
	}
};

class MainScene : public Scene {
public:
	inline static constexpr V2_int size{ 160, 90 };
	inline static constexpr std::size_t scale{ 8 };
	FluidSimulation<size.x, size.y> sim;
	V2_int prev;
	math::ValueNoise<float> noise{ size, 5 };
	std::vector<float> map;
	MainScene() {
		window::SetColor(color::CYAN);

		map = noise.GenerateNoiseMap({}, 5, 0.02f, 1.8, 0.35f);
		sim.Init();
		sim.SetNoiseMap(map.data(), 0.05f);
		sim.AddDensityNoiseMap(map.data(), 300);
	}
	math::RNG<float> rng{ 0, 1 };
	math::RNG<float> rng_f{ 0.005f, 0.035f };
	V2_float position;
	V2_float velocity;
	V2_float acceleration;
	virtual void Update(float dt) override final {
		acceleration = V2_float{ rng(), rng() } * 0.05f;
		velocity += acceleration;
		position += V2_float{ -0.01f, -0.01f };
		map = noise.GenerateNoiseMap(position, 5, 0.01f, 1.8, 0.35f);
		noise.Generate(position);
		//sim.SetNoiseMap(map.data(), 0.05f);
		V2_int mouse{ input::GetMouseScreenPosition() };
		//sim.AddDensityNoiseMap(map.data(), 300);
		//sim.AddVelocity(mouse / scale, Sign(mouse - prev) * 1.0f);
		prev = mouse;
		if (input::MousePressed(Mouse::LEFT))
			sim.AddDensity(input::GetMouseScreenPosition() / scale, 400, 2);
		//sim.Step(0.0005f, 0.0005f, dt);
		//sim.FadeDensity();
		//for (std::size_t i = 0; i < size.x; ++i) {
		//	for (std::size_t j = 0; j < size.y; ++j) {
		//		V2_int point{ i * scale, j * scale };
		//		V2_int scale_size{ scale, scale };
		//		auto index{ sim.IX(i, j) };
		//		
		//		float value = sim.dens[index];
		//		//std::uint8_t red = value >= 255 ? 255 : static_cast<std::uint8_t>(value);
		//		
		//		float rand_value = map[i + size.x * j];
		//		std::uint8_t red{ (std::uint8_t)(255.0f * rand_value) };

		//		Color color{ red, red, red, red };

		//		/*if (value > 0.8) {
		//			color.r = (std::uint8_t)(255.0f * (1.0f - value));
		//			color.g = (std::uint8_t)(255.0f * (1.0f - value));
		//			color.b = (std::uint8_t)(255.0f * (1.0f - value));
		//			color.a = red;
		//		}*/

		//		V2_float norm_velocity = V2_float{ sim.u[index], sim.v[index] } / 0.05f;
		//		Line<int> direction{ point, V2_int{ point + norm_velocity * 5.0f } };
		//		AABB<std::size_t> aabb{ point, scale_size };
		//		
		//		draw::SolidAABB(aabb, color);
		//		//draw::Line(direction, color::WHITE);
		//	}
		//}
	}
};

class WeatherGame : public Engine {
	virtual void Init() override final {
		scene::Load<MainScene>("main");
		scene::SetActive("main");
	}
	virtual void Update(float dt) override final {
		scene::Update(dt);
	}
};

int main(int c, char** v) {
	WeatherGame game;
	game.Start("", V2_int{ 1280, 720 }, true, V2_int{}, window::Flags::NONE, true, false);
	return 0;
}