#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct ShapeSetColorScript : public Script {
	void OnStart() override;
	Color selected_color{ color::White };

	PTGN_REFLECT(ShapeSetColorScript, selected_color)
};

struct ShapeSetFillStyleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	bool option{ true };

	PTGN_REFLECT(ShapeSetFillStyleScript, number, option)
};

struct ShapeSetStrokeWidthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeSetStrokeWidthScript, number)
};

struct ShapeSetSizeScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeSetSizeScript, vector)
};

struct ShapeSetRadiusScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeSetRadiusScript, number)
};

struct ShapeSetGeometryScript : public Script {
	void OnStart() override;
	Shape selected_shape{};

	PTGN_REFLECT(ShapeSetGeometryScript, selected_shape)
};

struct ShapeRectSetBoundsOrSizeScript : public Script {
	void OnStart() override;
	V2_float vector{};
	V2_float vector2{};
	bool option{ true };
	std::string choice{};

	PTGN_REFLECT(ShapeRectSetBoundsOrSizeScript, vector, vector2, option, choice)
};

struct ShapeRectSetCenterScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeRectSetCenterScript, vector)
};

struct ShapeCircleSetDiameterScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeCircleSetDiameterScript, number)
};

struct ShapeRoundedRectSetCornerRadiusScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeRoundedRectSetCornerRadiusScript, number)
};

struct ShapePolygonSetVerticesScript : public Script {
	void OnStart() override;
	std::vector<V2_float> vertices{};

	PTGN_REFLECT(ShapePolygonSetVerticesScript, vertices)
};

struct ShapePolygonSetVertexScript : public Script {
	void OnStart() override;
	int index{};
	V2_float vector{};

	PTGN_REFLECT(ShapePolygonSetVertexScript, index, vector)
};

struct ShapeEllipseSetRadiiScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeEllipseSetRadiiScript, vector)
};

struct ShapeEllipseSetWidthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeEllipseSetWidthScript, number)
};

struct ShapeEllipseSetHeightScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeEllipseSetHeightScript, number)
};

struct ShapeTriangleSetVerticesScript : public Script {
	void OnStart() override;
	std::vector<V2_float> vertices{};

	PTGN_REFLECT(ShapeTriangleSetVerticesScript, vertices)
};

struct ShapeTriangleSetVertexScript : public Script {
	void OnStart() override;
	int index{};
	V2_float vector{};

	PTGN_REFLECT(ShapeTriangleSetVertexScript, index, vector)
};

struct ShapeLineSetStartScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeLineSetStartScript, vector)
};

struct ShapeLineSetEndScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeLineSetEndScript, vector)
};

struct ShapeLineSetEndpointsScript : public Script {
	void OnStart() override;
	V2_float vector{};
	V2_float vector2{};

	PTGN_REFLECT(ShapeLineSetEndpointsScript, vector, vector2)
};

struct ShapeLineSetWidthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeLineSetWidthScript, number)
};

struct ShapeCapsuleSetStartScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeCapsuleSetStartScript, vector)
};

struct ShapeCapsuleSetEndScript : public Script {
	void OnStart() override;
	V2_float vector{};

	PTGN_REFLECT(ShapeCapsuleSetEndScript, vector)
};

struct ShapeCapsuleSetRadiusScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeCapsuleSetRadiusScript, number)
};

struct ShapeCapsuleSetLengthScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeCapsuleSetLengthScript, number)
};

struct ShapeArcSetRadiusScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeArcSetRadiusScript, number)
};

struct ShapeArcSetStartAngleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeArcSetStartAngleScript, number)
};

struct ShapeArcSetEndAngleScript : public Script {
	void OnStart() override;
	float number{ 1.0f };

	PTGN_REFLECT(ShapeArcSetEndAngleScript, number)
};

struct ShapeArcSetDirectionScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(ShapeArcSetDirectionScript, option)
};

} // namespace ptgn
