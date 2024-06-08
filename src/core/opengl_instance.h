#pragma once

// TODO: TEMPORARY
#include "protegon/shader.h"
#include "renderer/buffer.h"
#include "protegon/texture.h"
// TODO: TEMPORARY
#include <unordered_map>
#include <functional>

namespace ptgn {

class OpenGLInstance {
public:
	OpenGLInstance();
	~OpenGLInstance();
	[[nodiscard]] bool IsInitialized() const;
private:
	bool initialized_{ false };
	bool InitOpenGL();
};

namespace impl {

struct VertexPosColor {
	glsl::vec3 pos;
	glsl::vec4 color;
};

using VertexVector = std::vector<VertexPosColor>;

class Intersect {
public:
	Intersect();
	Intersect(V2_float intersect, float parameter);
	~Intersect();
	V2_float GetIntersectPoint();
	float GetParam();
	bool IsValidIntersect();
	float GetAngle();
	void SetAngle(float angleIn);
protected:
private:
	V2_float intersectVec;
	float param;
	bool isValid;
	float angle;
};

class IntersectFinder {
public:
	IntersectFinder() = default;
	virtual ~IntersectFinder() = default;
	virtual std::vector<Intersect> FindClosestIntersection(const VertexVector& ray, std::vector<V2_float>& shapeVectors, const float angle) = 0;

protected:
private:
};

class ClosestIntersectionFinder : public IntersectFinder {
public:
	ClosestIntersectionFinder() = default;
	~ClosestIntersectionFinder() = default;
	std::vector<Intersect> FindClosestIntersection(const VertexVector& ray, std::vector<V2_float>& shapeVectors, const float angle);


protected:
private:
};

struct LightKey {
public:
	LightKey(std::string key) : lightKey(key) {}

	std::string Key() const { return lightKey; }
private:
	std::string lightKey;
};

class Light {
public:
	Light() = default;
	virtual ~Light() = default;
	virtual void GenerateLight(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles) = 0;
	virtual void Render(Texture renderTarget, Shader shader) = 0;
	virtual V2_float GetVec() = 0;
	virtual void SetVec(const V2_float& lightVec) = 0;
	virtual std::string GetKey() = 0;
	virtual Color GetColor() = 0;
	virtual float GetIntensity() = 0;
	virtual bool ShouldRenderLight() = 0;
	bool shouldDebugLines = false;
protected:
private:
};

class SpotLight : public Light {
public:
	SpotLight(const ClosestIntersectionFinder& intersectionFinder, const std::string& lightName, const V2_float& initialPosition, const Color& color, const float initailItensity, const bool isDynamic);
	~SpotLight();
	SpotLight(const SpotLight& that);
	SpotLight(SpotLight&& that);

	V2_float GetVec();
	void SetVec(const V2_float& lightVec);
	std::string GetKey();
	Color GetColor();
	float GetIntensity();
	void GenerateLight(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles);
	void Render(Texture renderTarget, Shader shader);
	bool ShouldRenderLight();
protected:
private:
	std::vector<Intersect> GetIntersectPoints(std::vector<V2_float>& shapeVectors, const std::vector<float>& uniqueAngles);
	static bool CompareIntersects(Intersect vec1, Intersect vec2);
	void GenerateLightRays(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles);
	void BuildLightRayVertexes(VertexVector& rayLine, VertexVector& debugLightRays, std::vector<Intersect>& intersects);

	VertexVector lightVertexArray;
	VertexVector debugRays;
	V2_float lightVector;
	Color lightColor;
	std::string lightKey;
	float intensity;
	bool isDynamicLight;
	bool hasGeneratedLightBefore;
	ClosestIntersectionFinder intersectFinder;

};

static bool IntersectComp(V2_float v, Intersect vec1, Intersect vec2) {
	V2_float o1 = vec1.GetIntersectPoint();
	V2_float o2 = vec2.GetIntersectPoint();

	float ang1 = atan(((o1.y - v.y) / (o1.x - v.x)) * pi<float> / 180);
	float ang2 = atan((o2.y - v.y) / (o2.x - v.x) * pi<float> / 180);
	if (ang1 < ang2) return true;
	else if (ang1 > ang2) return false;

	return true;
}


class DirectionalLight : public Light {
public:
	DirectionalLight(const ClosestIntersectionFinder& intersectionFinder, const std::string& lightName, const V2_float& initialPosition,
		const Color& color, const float initailItensity, const float angle, const float openAngle, const bool isDynamic);
	~DirectionalLight();
	DirectionalLight(const DirectionalLight& that);

	V2_float GetVec();
	void SetVec(const V2_float& lightVec);
	std::string GetKey();
	Color GetColor();
	float GetIntensity();
	void GenerateLight(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles);
	void Render(Texture renderTarget, Shader shader);
	bool ShouldRenderLight();

protected:
private:
	std::vector<Intersect> GetIntersectPoints(std::vector<V2_float>& shapeVectors, const std::vector<float>& uniqueAngles);
	void BuildLightRays(std::vector<V2_float>& lightRays);
	Intersect GetIntersect(std::vector<V2_float>& shapeVectors, VertexVector ray);
	bool IsRayInFieldOfView(float facingAngle, float fieldOfViewAngle, V2_float ray);
	void AddIntersect(const VertexVector& ray, std::vector<V2_float>& shapeVectors, std::vector<Intersect>& intersects);
	void AddFieldOfViewRay(V2_float rayLines, std::vector<V2_float>& shapeVectors, std::vector<Intersect>& intersects, const float angle);
	void GenerateLightRays(std::vector<V2_float>& shapePoints, std::vector<float>& uniqueAngles);
	void BuildLightRayVertexes(VertexVector& rayLine, VertexVector& debugLightRays, std::vector<Intersect>& intersects);

	VertexVector lightVertexArray;
	VertexVector debugRays;
	V2_float lightVector;
	Color lightColor;
	std::string lightKey;
	float intensity;
	float facingAngle;
	float fieldOfView;
	ClosestIntersectionFinder intersectFinder;
	bool isDynamicLight;
	bool hasGeneratedLightBefore;
	std::vector<V2_float> directionalRays;
};

class LightEngine {
public:
	LightEngine(int width, int height, Color);
	~LightEngine();
	LightKey AddLight(const std::string& key, const V2_float& lightVector, const Color& lightColor, const float intensity, const bool isDynamic);
	LightKey AddDirectionalLight(const std::string& key, const V2_float& lightVector, const Color& lightColor, const float intensity, const float angleIn, const float openingAngle, const bool isDynamic);
	void RemoveLight(const LightKey& lightKey);
	void SetPosition(const LightKey& lightKey, const V2_float& newPosition);
	std::vector<LightKey> GetLightKeys();
	void Draw(float playing_time);
	void AddShape(const VertexVector& shape);
	void EnableSoftShadow(bool shouldUseSoftShadow);
	void DebugLightRays(bool shouldDebugLines);

	static Intersect GetLineIntersect(VertexVector ray, VertexVector segment);
	Texture lightRenderTex;
protected:
private:

	void AddUniquePoints(std::vector<V2_float>& shapePoints, std::unordered_map<V2_float, std::size_t>& points, VertexVector vertextArray);
	void AddPoints(std::vector<V2_float>& points, VertexVector vertextArray);
	std::vector<float> GetUniqueAngles(const V2_float& position);

	std::unordered_map<std::string, std::unique_ptr<Light>> lights;
	std::unordered_map<V2_float, std::size_t> shapePointsSet;
	std::vector<V2_float> uniquePoints;
	std::vector<V2_float> shapeVectors;

	bool shoulDebugLines = false;
	bool shouldUseSoftBlur = false;
	Shader lightShader;
	Shader blurShaderX;
	Shader blurShaderY;
	Color renderColor;
	ClosestIntersectionFinder intersectFinder;

};

void TestOpenGL();

} // namespace impl

} // namespace ptgn