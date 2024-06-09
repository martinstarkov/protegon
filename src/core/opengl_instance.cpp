#include "opengl_instance.h"

#include <string>

#include <SDL.h>
#include <SDL_image.h>

#include "game.h"
#include "protegon/hash.h"
#include "protegon/window.h"
#include "renderer/gl_loader.h"
#include "renderer/buffer.h"
#include "protegon/shader.h"
#include "renderer/vertex_array.h"
#include "utility/platform.h"
#include <numeric>

namespace ptgn {

ptgn::OpenGLInstance::OpenGLInstance() {
#ifndef PTGN_PLATFORM_MACOS
	initialized_ = InitOpenGL();
	// TODO: Potentially make this optional in the future:
#else
	// TODO: Figure out what to do here for Apple.
	initialized_ = false;
#endif
	PTGN_CHECK(initialized_, "Failed to initialize OpenGL");
}

OpenGLInstance::~OpenGLInstance() {
	// TODO: Figure out if something needs to be done here.
	initialized_ = false;
}

bool OpenGLInstance::IsInitialized() const {
	return initialized_;
}

bool OpenGLInstance::InitOpenGL() {
#ifndef PTGN_PLATFORM_MACOS

#define STR(x) #x
#define GLE(name, caps_name) gl##name = (PFNGL##caps_name##PROC) SDL_GL_GetProcAddress(STR(gl##name));
	GL_LIST
#undef GLE
#undef STR

#define GLE(name, caps_name) gl##name && 
		return GL_LIST true;
#undef GLE

#else
		// TODO: Figure out if something needs to be done separately on apple.
		return true;
#endif
}

namespace impl {

const std::vector<std::uint32_t> indices = {
	0, 1, 2,
	2, 3, 1
};

const std::vector<std::uint32_t> indices2 = {
	0, 1, 2,
};

struct Vertex {
	glsl::vec3 pos;
	glsl::vec4 color;
};

const std::vector<Vertex> vao_vert = {
	Vertex{ glsl::vec3{ -1.0f, -1.0f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{  1.0f, -1.0f, 0.0f }, glsl::vec4{ 0.0f, 0.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{ -1.0f,  1.0f, 0.0f }, glsl::vec4{ 1.0f, 1.0f, 0.0f, 1.0f } },
	Vertex{ glsl::vec3{  1.0f,  1.0f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 1.0f, 1.0f } },
};


const std::vector<Vertex> vao_vert2 = {
	Vertex{ glsl::vec3{ -0.5f, -0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{  0.5f, -0.5f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 0.0f, 1.0f } },
	Vertex{ glsl::vec3{  0.0f,  0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 0.0f, 1.0f } },
};

VertexArray vao;
IndexBuffer ibo;
VertexBuffer vbo;

//---------------------------------------------------------------------------
static void vao_init() {

	vao = VertexArray{ PrimitiveMode::Triangles };
	vbo = { vao_vert };
	ibo = { indices };

	vao.SetVertexBuffer(vbo);
	vao.SetIndexBuffer(ibo);

	vbo.Unbind();
	vao.Unbind();
}

static void PresentBuffer(Texture& backBuffer, Shader& shader, float playing_time, const Rectangle<int>& dest_rect) {
	//SDL_GL_BindTexture(backBuffer, NULL, NULL);

	auto renderer{ global::GetGame().sdl.GetRenderer() };
	
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
	SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_ADD);


	backBuffer.SetAsRendererTarget();
	SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_ADD);


	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
	SDL_RenderClear(renderer.get());

	shader.Bind();

	V2_float size = window::GetSize();
	/*uniform vec2 lightpos;
	uniform vec3 lightColor;
	uniform float screenHeight;
	uniform float intensity;*/
	/*shader.SetUniform("iResolution", size.x, size.y, 0.0f);
	shader.SetUniform("iTime", playing_time);*/

	shader.SetUniform("lightpos", size.x / 2, size.y / 2);
	shader.SetUniform("lightColor", 1.0f, 0.0f, 0.0f);
	shader.SetUniform("intensity", 10.0f);
	shader.SetUniform("screenHeight", size.y);

	//shader.SetUniform("iResolution", size.x, size.y, 0.0f);
	//shader.SetUniform("iTime", playing_time);

	vao.Draw();

	shader.Unbind();

	SDL_SetRenderTarget(renderer.get(), NULL);
	backBuffer.SetBlendMode(Texture::BlendMode::BLEND);
	// OpenGL coordinate system is flipped vertically compared to SDL
	backBuffer.Draw(dest_rect, {}, 0, Texture::Flip::VERTICAL, nullptr);
}

void DrawRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

static void DrawShader(Texture target, Shader shader) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	target.SetAsRendererTarget();
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
	SDL_RenderClear(renderer.get());

	shader.Bind();

	VertexArray vao = VertexArray{ PrimitiveMode::Quads };

	V2_float size = window::GetSize();

	std::vector<VertexPosColor> vertices{
		VertexPosColor{ { 0.0f, 0.0f, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } },
		VertexPosColor{ { size.x, 0.0f, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } },
		VertexPosColor{ { size.x, size.y, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } },
		VertexPosColor{ { 0.0f, size.y, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } }
	};

	VertexBuffer vbo{ vertices };

	vao.SetVertexBuffer(vbo);
	vao.SetIndexBuffer({ { 0, 1, 2, 3 } });

	vao.Draw();

	shader.Unbind();
}

static void DrawShader(Texture target, Shader shader, VertexVector vertices) {
	if (vertices.size() == 0) return;

	auto renderer{ global::GetGame().sdl.GetRenderer() };
	target.SetAsRendererTarget();
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
	SDL_RenderClear(renderer.get());

	shader.Bind();

	VertexArray vao = VertexArray(PrimitiveMode::Quads);

	V2_int size = window::GetSize();


	VertexBuffer vbo{ vertices };

	std::vector<std::uint32_t> indices;
	std::iota(std::begin(indices), std::end(indices), 0);

	vao.SetVertexBuffer(vbo);
	vao.SetIndexBuffer({ indices });

	vao.Draw();

	shader.Unbind();
}

Intersect::Intersect(V2_float intersect, float parameter)
	: intersectVec(intersect), param(parameter), isValid(true) {}

Intersect::Intersect() {}


Intersect::~Intersect() {}

bool Intersect::IsValidIntersect() {
	PTGN_INFO("IsValidIntersection: ", isValid);
	return isValid;
}

V2_float Intersect::GetIntersectPoint() {
	return intersectVec;
}

float Intersect::GetParam() {
	return param;
}

float Intersect::GetAngle() {
	return angle;
}

void Intersect::SetAngle(float angleIn) {
	angle = angleIn;
}

Intersect LightEngine::GetLineIntersect(VertexVector ray, VertexVector segment) {
	// parametric line intersection
	//p1 + (p2-p1)T
	float r_px = ray.at(0).pos.at(0);
	float r_py = ray.at(0).pos.at(1);
	float r_dx = ray.at(1).pos.at(0) - ray.at(0).pos.at(0);
	float r_dy = ray.at(1).pos.at(1) - ray.at(0).pos.at(1);

	float s_px = segment.at(0).pos.at(0);
	float s_py = segment.at(0).pos.at(1);
	float s_dx = segment.at(1).pos.at(0) - segment.at(0).pos.at(0);
	float s_dy = segment.at(1).pos.at(1) - segment.at(0).pos.at(1);

	//parallel check

	V2_float normalRay = V2_float(r_dx, r_dy).Normalized();
	V2_float normalSeg = V2_float(s_dx, s_dy).Normalized();
	float dot = normalRay.Dot(normalSeg);

	if (dot == 0)  // if they point in same direction, then theres no intersection
	{
		return  Intersect(V2_float(0, 0), 0);
	}

	//get constant params
	float T2 = (r_dx * (s_py - r_py) + r_dy * (r_px - s_px)) / (s_dx * r_dy - s_dy * r_dx);
	float T1 = (s_px + s_dx * T2 - r_px) / r_dx;

	if (T1 < 0) {
		return Intersect(V2_float(0, 0), 0);
	}
	if (T2 < 0 || T2>1) {
		return Intersect(V2_float(0, 0), 0);
	}

	return Intersect(V2_float(r_px + r_dx * T1, r_py + r_dy * T1), T1);
}

std::vector<Intersect> ClosestIntersectionFinder::FindClosestIntersection(const VertexVector& ray, std::vector<V2_float>& shapeVectors, const float angle) {

	Intersect closestInterect(V2_float(799, 799), 1000);
	std::vector<Intersect> intersects;
	for (int i = 0; i < shapeVectors.size(); i += 2) {
		if (i >= shapeVectors.size() || i + 1 >= shapeVectors.size()) break;
		V2_float seg1 = shapeVectors.at(i);
		V2_float seg2 = shapeVectors.at(i + 1);
		VertexVector segLine;
		segLine.push_back(VertexPosColor{ { seg1.x, seg1.y, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } });
		segLine.push_back(VertexPosColor{ { seg2.x, seg2.y, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f } });
		Intersect intersect = LightEngine::GetLineIntersect(ray, segLine);

		if (intersect.GetIntersectPoint().x > 0 && intersect.GetIntersectPoint().y > 0) {
			if (intersect.GetParam() < closestInterect.GetParam()) {
				closestInterect = intersect;
			}
		}
	}

	if (closestInterect.GetParam() < 1000) {
		closestInterect.SetAngle(angle);
		intersects.push_back(closestInterect);
	}

	return intersects;
}

SpotLight::SpotLight(const ClosestIntersectionFinder& intersectionFinder, const std::string& lightName, const V2_float& initialPosition, const Color& color, const float initialItensity, const bool isDynamic)
	: Light(), intersectFinder(intersectionFinder), lightKey(lightName), lightVector(initialPosition), lightColor(color), intensity(initialItensity), isDynamicLight(isDynamic) {}

SpotLight::~SpotLight() {}

SpotLight::SpotLight(const SpotLight& that) {
	lightVertexArray = that.lightVertexArray;
	debugRays = that.debugRays;
	lightVector = that.lightVector;
	lightColor = that.lightColor;
	lightKey = that.lightKey;
	intensity = that.intensity;
}

SpotLight::SpotLight(SpotLight&& that) : lightVertexArray(that.lightVertexArray), debugRays(that.debugRays), lightVector(that.lightVector), lightColor(that.lightColor),
lightKey(that.lightKey), intensity(that.intensity) {}

V2_float SpotLight::GetVec() {
	return lightVector;
}


std::string SpotLight::GetKey() {
	return lightKey;
}

void SpotLight::SetVec(const V2_float& lightVec) {
	if (isDynamicLight) {
		lightVector = lightVec;
	}
}

Color SpotLight::GetColor() {
	return lightColor;
}

float SpotLight::GetIntensity() {
	return intensity;
}

bool SpotLight::CompareIntersects(Intersect vec1, Intersect vec2) {
	if (vec1.GetAngle() - vec2.GetAngle() < 0) {
		return true;
	} else if (vec1.GetAngle() - vec2.GetAngle() > 0) {
		return false;
	}

	return false;
}

std::vector<Intersect> SpotLight::GetIntersectPoints(std::vector<V2_float>& shapeVectors, const std::vector<float>& uniqueAngles) {
	std::vector<Intersect> intersects;
	for (int uniqueAngleIndex = 0; uniqueAngleIndex < uniqueAngles.size(); uniqueAngleIndex++) {
		float angle = uniqueAngles.at(uniqueAngleIndex);
		float x = cos(angle);
		float y = sin(angle);

		VertexVector ray;
		ray.push_back(VertexPosColor{ { lightVector.x, lightVector.y, 0.0f}, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f }
	});
		ray.push_back(VertexPosColor{ { lightVector.x + x, lightVector.y + y, 0.0f }, { color::Black.r / 255.0f, color::Black.g / 255.0f, color::Black.b / 255.0f, color::Black.a / 255.0f }
});

		std::vector<Intersect> closestIntersections = intersectFinder.FindClosestIntersection(ray, shapeVectors, angle);
		intersects.insert(intersects.end(), closestIntersections.begin(), closestIntersections.end());
	}
	std::sort(intersects.begin(), intersects.end(), CompareIntersects);
	return intersects;
}

bool SpotLight::ShouldRenderLight() {
	bool shouldDrawLight;
	if (!isDynamicLight && !hasGeneratedLightBefore) {
		return true;
	} else if (isDynamicLight) {
		return true;
	} else {
		return false;
	}
}

void SpotLight::GenerateLight(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles) {
	if (!isDynamicLight && !hasGeneratedLightBefore) {
		GenerateLightRays(shapePoints, uniqueAngles);
		hasGeneratedLightBefore = true;
	} else if (isDynamicLight) {
		GenerateLightRays(shapePoints, uniqueAngles);
	}
}

void SpotLight::GenerateLightRays(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles) {
	std::vector<Intersect> intersects = GetIntersectPoints(shapePoints, uniqueAngles);
	if (intersects.size() == 0) return;
	VertexVector lightRays;
	VertexVector debugLightRays;
	BuildLightRayVertexes(lightRays, debugLightRays, intersects);
	V2_float i1 = intersects.at(0).GetIntersectPoint();
	lightRays.push_back(VertexPosColor{ { i1.x, i1.y, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } });
	lightVertexArray.clear();
	lightVertexArray = lightRays;

	if (shouldDebugLines) {
		debugRays = debugLightRays;
	}
}

void SpotLight::BuildLightRayVertexes(VertexVector& rayLine, VertexVector& debugLightRays, std::vector<Intersect>& intersects) {
	rayLine.push_back(VertexPosColor{ { lightVector.x, lightVector.y, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } });
	for (int i = 0; i < intersects.size(); i++) {
		V2_float v = intersects.at(i).GetIntersectPoint();
		rayLine.push_back(VertexPosColor{ { v.x, v.y, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } });
		if (shouldDebugLines) {
			debugLightRays.push_back(VertexPosColor{{ lightVector.x, lightVector.y, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }});
			V2_float v1 = intersects.at(i).GetIntersectPoint();
			debugLightRays.push_back(VertexPosColor{ { v1.x, v1.y, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		}
	}
}


void SpotLight::Render(Texture target, Shader shader) {

	DrawShader(target, shader, lightVertexArray);

	if (shouldDebugLines) {
		DrawShader(target, shader, debugRays);
	}

}

DirectionalLight::DirectionalLight(const ClosestIntersectionFinder& intersectionFinder, const std::string& lightName, const V2_float& initialPosition,
	const Color& color, const float initialIntensity, const float angleIn, const float openAngle, const bool isDynamic)
	: Light(), intersectFinder(intersectionFinder), lightKey(lightName), lightVector(initialPosition), lightColor(color), intensity(initialIntensity), facingAngle(angleIn), fieldOfView(openAngle), isDynamicLight(isDynamic) {
	BuildLightRays(directionalRays);
}

void DirectionalLight::BuildLightRays(std::vector<V2_float>& lightRays) {

	float fangle = facingAngle * M_PI / 180;
	float offsetAngle = fieldOfView * M_PI / 180;
	//float radius = 200; TODO use a radius value

	lightRays.clear();
	float endAngle = fangle + offsetAngle / 2;

	directionalRays.push_back(V2_float(lightVector.x + cos((fangle - offsetAngle / 2)), lightVector.y + sin((fangle - offsetAngle / 2))));
	directionalRays.push_back(V2_float(lightVector.x + cos(endAngle), lightVector.y + sin(endAngle)));

}


DirectionalLight::~DirectionalLight() {}


V2_float DirectionalLight::GetVec() {
	return lightVector;
}


std::string DirectionalLight::GetKey() {
	return lightKey;
}

void DirectionalLight::SetVec(const V2_float& lightVec) {
	lightVector = lightVec;
	BuildLightRays(directionalRays);
}

Color DirectionalLight::GetColor() {
	return lightColor;
}

float DirectionalLight::GetIntensity() {
	return intensity;
}

bool DirectionalLight::ShouldRenderLight() {
	bool shouldDrawLight;
	if (!isDynamicLight && !hasGeneratedLightBefore) {
		shouldDrawLight = true;
	} else if (isDynamicLight) {
		shouldDrawLight = true;
	} else {
		shouldDrawLight = false;
	}
	return shouldDrawLight;
}

bool DirectionalLight::IsRayInFieldOfView(float facingAngle, float fieldOfViewAngle, V2_float ray) {

	V2_float lightDir(cos(facingAngle), sin(facingAngle));
	V2_float normalRay = V2_float(ray.x - lightVector.x, ray.y - lightVector.y).Normalized();
	V2_float normalLightDir = lightDir.Normalized();

	float a = acos((normalRay.x * normalLightDir.x) + (normalRay.y * normalLightDir.y));
	return a < fieldOfViewAngle;

}

std::vector<Intersect> DirectionalLight::GetIntersectPoints(std::vector<V2_float>& shapeVectors, const std::vector<float>& uniqueAngles) {
	std::vector<Intersect> intersects;
	float fangle = facingAngle * M_PI / 180;
	float offsetAngle = fieldOfView * M_PI / 180;

	for (int uniqueAngleIndex = 0; uniqueAngleIndex < uniqueAngles.size(); uniqueAngleIndex++) {
		float angle = uniqueAngles.at(uniqueAngleIndex);
		float x = cos(angle);
		float y = sin(angle);

		V2_float lightVectorToEdge(lightVector.x + x, lightVector.y + y);
		VertexVector ray;
		ray.push_back(VertexPosColor{ {lightVector.x, lightVector.y, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}});
		ray.push_back(VertexPosColor{ {lightVectorToEdge.x, lightVectorToEdge.y, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}});

		if (IsRayInFieldOfView(fangle, offsetAngle, lightVectorToEdge)) {
			std::vector<Intersect> closestIntersections = intersectFinder.FindClosestIntersection(ray, shapeVectors, angle);
			intersects.insert(intersects.end(), closestIntersections.begin(), closestIntersections.end());
		}
	}

	if (directionalRays.size() == 0) return {};
	//add the start ray for the directional light, this is the angle the direction light is pointing in minus openingAngle/2
	AddFieldOfViewRay(directionalRays.at(0), shapeVectors, intersects, (fangle - offsetAngle / 2));
	//add the end ray for directional light, this is the angle the direction light is pointing in plus openingAngle/2
	AddFieldOfViewRay(directionalRays.at(directionalRays.size() - 1), shapeVectors, intersects, (fangle + offsetAngle / 2));

	std::sort(intersects.begin(), intersects.end(), [&](auto& a, auto& b) {
		return IntersectComp(lightVector, a, b);
	});
	return intersects;
}

void DirectionalLight::AddFieldOfViewRay(V2_float rayLines, std::vector<V2_float>& shapeVectors, std::vector<Intersect>& intersects, const float angle) {
	VertexVector ray;
	ray.push_back(VertexPosColor{ { lightVector.x, lightVector.y, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }});
	ray.push_back(VertexPosColor{ { rayLines.x, rayLines.y, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }});
	std::vector<Intersect> closestIntersections = intersectFinder.FindClosestIntersection(ray, shapeVectors, angle);
	intersects.insert(intersects.end(), closestIntersections.begin(), closestIntersections.end());
}

void DirectionalLight::GenerateLight(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles) {
	if (!isDynamicLight && !hasGeneratedLightBefore) {
		GenerateLightRays(shapePoints, uniqueAngles);
		hasGeneratedLightBefore = true;
	} else if (isDynamicLight) {
		GenerateLightRays(shapePoints, uniqueAngles);
	}
}

void DirectionalLight::GenerateLightRays(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles) {
	std::vector<Intersect> intersects = GetIntersectPoints(shapePoints, uniqueAngles);
	VertexVector lightRays;
	VertexVector debugLightRays;
	BuildLightRayVertexes(lightRays, debugLightRays, intersects);

	lightVertexArray.clear();
	lightVertexArray = lightRays;

	if (shouldDebugLines) {
		debugRays = debugLightRays;
	}
}

void DirectionalLight::BuildLightRayVertexes(VertexVector& rayLine, VertexVector& debugLightRays, std::vector<Intersect>& intersects) {
	rayLine.push_back(VertexPosColor{ { lightVector.x, lightVector.y, 0.0f }, {1.0f, 1.0f, 1.0f, 1.0f}});
	for (int i = 0; i < intersects.size(); i++) {
		V2_float v = intersects.at(i).GetIntersectPoint();
		rayLine.push_back(VertexPosColor{ { v.x, v.y, 0.0f }, {1.0f, 1.0f, 1.0f, 1.0f}});
		if (shouldDebugLines) {
			debugLightRays.push_back(VertexPosColor{ { lightVector.x, lightVector.y, 0.0f }, {1.0f, 0.0f, 0.0f, 1.0f}});
			V2_float v1 = intersects.at(i).GetIntersectPoint();
			debugLightRays.push_back(VertexPosColor{ { v1.x, v1.y, 0.0f }, {1.0f, 0.0f, 0.0f, 1.0f}});
		}
	}
}

void DirectionalLight::Render(Texture target, Shader shader) {

	DrawShader(target, shader, lightVertexArray);

	if (shouldDebugLines) {
		DrawShader(target, shader, debugRays);
	}

}

LightEngine::LightEngine(int width, int height, Color engCol) : renderColor(engCol) {
	blurShaderX = Shader("resources/shader/main_vert.glsl", "resources/shader/blur_x.glsl");
	// TODO: Add multiple shader support.
	blurShaderY = Shader("resources/shader/main_vert.glsl", "resources/shader/blur_y.glsl");

	//lightShader = Shader("resources/shader/main_vert.glsl", "resources/shader/lightFs.glsl");
	lightShader = Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");
	//lightShader.SetUniform("texture", sf::Shader::CurrentTexture);
	//lightShader.Bind();
	//lightShader.SetUniform("screenHeight", height);
	//lightShader.Unbind();
	//lightRenderTex.Create(width, height);

}

LightEngine::~LightEngine() {
	lights.clear();
}


void LightEngine::AddPoints(std::vector<V2_float>& points, VertexVector vertextArray) {
	for (int i = 0; i < vertextArray.size(); i++) {
		points.push_back({ vertextArray.at(i).pos.at(0), vertextArray.at(i).pos.at(1) });
	}
}


void LightEngine::AddUniquePoints(std::vector<V2_float>& shapePoints, std::unordered_map<V2_float, std::size_t>& points, VertexVector vertextArray) {
	for (int i = 0; i < vertextArray.size(); i++) {
		V2_float v{ vertextArray.at(i).pos.at(0), vertextArray.at(i).pos.at(1) };
		if (points.find(v) == points.end()) {
			shapePoints.push_back(v);
			points.emplace(v, Hash(v));
		}
	}
}


void LightEngine::AddShape(const VertexVector& shape) {
	AddUniquePoints(uniquePoints, shapePointsSet, shape);
	AddPoints(shapeVectors, shape);
}

std::vector<float> LightEngine::GetUniqueAngles(const V2_float& position) {
	std::vector<float> uniqueAngles;
	for (int i = 0; i < uniquePoints.size(); i++) {
		V2_float point = uniquePoints.at(i);
		float angle = atan2(point.y - position.y, point.x - position.x);
		uniqueAngles.push_back((angle - 0.0001f));
		uniqueAngles.push_back(angle);
		uniqueAngles.push_back((angle + 0.0001f));
	}
	return uniqueAngles;
}


void LightEngine::Draw(float playing_time) {

	//auto renderer{ global::GetGame().sdl.GetRenderer() };

	//lightRenderTex.SetBlendMode(Texture::BlendMode::ADDITIVE);
	//lightRenderTex.SetAsRendererTarget();
	//
	//SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
	//SDL_RenderClear(renderer.get());

	//lightShader.Bind();

	//V2_float size = window::GetSize();

	//for (auto lightItr = lights.begin(); lightItr != lights.end(); ++lightItr) {
	//	/*std::unique_ptr<Light>& light = lightItr->second;

	//	if (light->ShouldRenderLight()) {
	//		std::vector<float> angles = GetUniqueAngles(light->GetVec());
	//		light->GenerateLight(shapeVectors, angles);
	//	}

	//	if (shoulDebugLines) {
	//		light->shouldDebugLines = true;
	//	}

	//	Color lightColor = light->GetColor();
	//	lightShader.SetUniform("lightpos", light->GetVec().x, light->GetVec().y);
	//	lightShader.SetUniform("lightColor", lightColor.r, lightColor.g, lightColor.b);
	//	lightShader.SetUniform("intensity", light->GetIntensity());

	//	*/

	//} 

	//std::vector<VertexPosColor> vertices{
	//	VertexPosColor{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
	//	VertexPosColor{ { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
	//	VertexPosColor{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
	//	VertexPosColor{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
	//};
	//VertexArray vao = VertexArray::Create();

	//VertexBuffer vbo{ vertices };

	//vao.AddVertexBuffer(vbo);
	//vao.SetIndexBuffer({ { 0, 1, 2, 3 } });

	//vao.Bind();
	//glDrawElements(GL_QUADS, vao.GetIndexBuffer().GetCount(), vao.GetIndexBuffer().GetType(), nullptr);
	//vao.Unbind();

	//lightShader.SetUniform("iResolution", size.x, size.y, 0.0f);
	//lightShader.SetUniform("iTime", playing_time);

	//lightShader.Unbind();

	//SDL_SetRenderTarget(renderer.get(), NULL);
	//lightRenderTex.SetBlendMode(Texture::BlendMode::BLEND);
	//Rectangle<float> dest_rect{
	//	{},
	//	size
	//};

	//lightRenderTex.Draw(dest_rect, {}, 0, Texture::Flip::VERTICAL, nullptr);

	//SDL_RenderClear(renderWindow);

	//lightRenderTex.SetBlendMode(Texture::BlendMode::ADDITIVE);
	//Shader r1 = lightShader;
	//// BLEND MULTIPLY
	//SDL_SetRenderDrawBlendMode(renderWindow, SDL_BLENDMODE_MUL);

	//Shader r2X;
	//Shader r2Y;

	//if (shouldUseSoftBlur) {
	//	r2X = blurShaderX;
	//	r2Y = blurShaderY;
	//}

	//if (r2X.IsValid() && r2Y.IsValid()) {
	//	DrawShader(lightRenderTex, r2X);
	//	DrawShader(lightRenderTex, r2Y);
	//}

	//SDL_SetRenderTarget(renderWindow, NULL);
	//lightRenderTex.SetBlendMode(Texture::BlendMode::BLEND);

	//const V2_int window_size = window::GetSize();

	//Rectangle<float> dest_rect{ {}, window_size };

	//lightRenderTex.Draw(dest_rect, {}, 0, Texture::Flip::VERTICAL, nullptr);
}


LightKey LightEngine::AddLight(const std::string& key, const V2_float& lightVector, const Color& lightColor, const float intensity, const bool isDynamic) {
	auto it = lights.find(key);
	if (it == lights.end()) {
		lights.emplace(std::make_pair(key, std::unique_ptr<Light>(new SpotLight(intersectFinder, key, lightVector, lightColor, intensity, isDynamic))));
	}

	return LightKey(key);
}

LightKey LightEngine::AddDirectionalLight(const std::string& key, const V2_float& lightVector, const Color& lightColor,
	const float intensity, const float angleIn, const float openingAngle, const bool isDynamic) {
	auto it = lights.find(key);
	if (it == lights.end()) {
		lights.emplace(std::make_pair(key, std::unique_ptr<Light>(new DirectionalLight(intersectFinder, key, lightVector, lightColor, intensity, angleIn, openingAngle, isDynamic))));
	}

	return LightKey(key);

}


void LightEngine::RemoveLight(const LightKey& lightKey) {
	lights.erase(lightKey.Key());
}

void LightEngine::SetPosition(const LightKey& lightKey, const V2_float& newPosition) {
	std::unordered_map<std::string, std::unique_ptr<Light>>::iterator got = lights.find(lightKey.Key());
	if (!(got == lights.end())) {
		std::unique_ptr<Light> light(std::move(got->second));
		light->SetVec(newPosition);
		got->second = std::move(light);
	}
}

void LightEngine::DebugLightRays(bool debugLines) {
	shoulDebugLines = debugLines;
}

void LightEngine::EnableSoftShadow(bool shouldUseSoftShadow) {
	shouldUseSoftBlur = shouldUseSoftShadow;
}

void TestOpenGL() {

	Game& game = global::GetGame();
	OpenGLInstance& opengl = game.opengl;
	PTGN_ASSERT(opengl.IsInitialized());
	SDLInstance& sdl = game.sdl;
	auto renderer = sdl.GetRenderer();
	auto win = sdl.GetWindow();

	std::string vertex_source = R"(
		#version 330 core

		layout(location = 0) in vec3 pos;
		layout(location = 1) in vec4 color;

		out vec3 v_Position;
		out vec4 v_Color;

		void main() {
			v_Position = pos;
			v_Color = color;
			gl_Position = vec4(pos, 1.0);
		}
	)";

	std::string fragment_source = R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec3 v_Position;
		in vec4 v_Color;

		void main() {
			color = vec4(v_Position * 0.5 + 0.5, 1.0);
			color = v_Color;
		}
	)";

	Shader shader;
	//shader = Shader(ShaderSource{ vertex_source }, ShaderSource{ fragment_source });
	shader = Shader("resources/shader/main_vert.glsl", "resources/shader/lightFs.glsl");
	//shader = Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");

	const float WIN_WIDTH = (float)window::GetSize().x;
	const float WIN_HEIGHT = (float)window::GetSize().y;

	GLfloat iResolution[3] = { WIN_WIDTH, WIN_HEIGHT, 0 };
	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	PTGN_INFO("Window size: ", window::GetSize());

	Texture texTarget{ Texture::AccessType::TARGET, window::GetSize() };

	int done = 0;
	int useShader = 0;
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);

	Texture drawTarget{ Texture::AccessType::TARGET, window::GetSize() };

	SDL_Rect rect1;
	SDL_Rect rect2;
	SDL_Rect rect3;

	rect1.x = 200;
	rect1.y = 200;

	rect1.w = 60;
	rect1.h = 40;

	rect2.x = 400;
	rect2.y = 400;

	rect2.w = 50;
	rect2.h = 70;

	rect3.x = 0;
	rect3.y = 0;

	rect3.w = 80;
	rect3.h = 80;

	vao_init();

	//VertexVector triangle = VertexVector::Create();

	std::vector<VertexPosColor> triangle_vertices{
		VertexPosColor{ glsl::vec3{ -0.5f - 0.5f, -0.5f - 0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 1.0f, 1.0f } },
		VertexPosColor{ glsl::vec3{  0.5f - 0.5f, -0.5f - 0.5f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 0.0f, 1.0f } },
		VertexPosColor{ glsl::vec3{  0.0f - 0.5f,  0.5f - 0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 0.0f, 1.0f } }
	};
	
	//triangle.AddVertexBuffer({ triangle_vertices });
	
	//triangle.SetIndexBuffer({ { 0, 1, 2 } });

	//LightEngine light_engine(800, 800, Color(32, 32, 32, 255));

	//light_engine.lightRenderTex = drawTarget;

	/*light_engine.AddShape(triangle_vertices);
	light_engine.DebugLightRays(false);
	light_engine.EnableSoftShadow(true);

	LightKey mouseLight = light_engine.AddLight("mouse light", V2_float(400, 400), color::White, 5, true);
	light_engine.AddLight("mouse light 2", V2_float(300, 350), color::Yellow, 5, false);
	light_engine.AddLight("mouse light 3", V2_float(300, 370), color::Red, 5, false);
	light_engine.AddLight("mouse light 4", V2_float(300, 400), color::Yellow, 5, false);
	light_engine.AddLight("mouse light 5", V2_float(350, 350), color::Green, 5, false);
	light_engine.AddDirectionalLight("mouse light 6", V2_float(500, 500), color::Cyan, 5, 180, 20, false);*/


	while (!done) {
		SDL_SetRenderTarget(renderer.get(), NULL);
		SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
		SDL_RenderClear(renderer.get());

		SDL_Point mouse;

		SDL_GetMouseState(&mouse.x, &mouse.y);

		Rectangle<int> dest_rect;

		dest_rect.w = WIN_WIDTH;
		dest_rect.h = WIN_HEIGHT;

		dest_rect.x = mouse.x - dest_rect.w / 2;
		dest_rect.y = mouse.y - dest_rect.h / 2;

		curr_time = clock();
		playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

		//DrawRect(renderer.get(), rect1, { 0, 0, 255, 255 });

		PresentBuffer(drawTarget, shader, playtime_in_second, dest_rect);

		//light_engine.Draw(playtime_in_second);

		//light_engine.SetPosition(mouseLight, V2_float{ (float)mouse.x, (float)mouse.y });

		//DrawRect(renderer.get(), rect2, { 255, 0, 0, 255 });
		SDL_RenderPresent(renderer.get());
		
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				done = 1;
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_SPACE) {
					useShader ^= 1;
					PTGN_INFO("useShader = ", (useShader ? "true" : "false"));
				}
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					done = 1;
				}
			}
		}
	}
}



} // namespace impl

} // namespace ptgn