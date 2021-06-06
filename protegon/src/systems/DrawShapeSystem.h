#pragma once

#include "core/ECS.h"
#include "components/TransformComponent.h"
#include "components/ShapeComponent.h"
#include "components/ColorComponent.h"
#include "components/Tags.h"

namespace ptgn {

template <typename Renderer, bool SOLID = true>
struct DrawShapeSystem {
	void operator()(ecs::Entity entity,  
					TransformComponent& transform, 
					ShapeComponent& shape,
					RenderComponent& render) {
		auto type{ shape.shape->GetType() };
		auto color{ colors::BLACK };
		if (entity.HasComponent<ColorComponent>()) {
			color = entity.GetComponent<ColorComponent>().color;
		}
		if (type == ShapeType::AABB) {
			if constexpr (SOLID) {
				Renderer::DrawSolidRectangle(transform.transform.position,
											 shape.shape->CastTo<AABB>().size,
											 color);
			} else {
				Renderer::DrawRectangle(transform.transform.position,
										shape.shape->CastTo<AABB>().size,
										color);
			}
		} else if (type == ShapeType::CIRCLE) {
			if constexpr (SOLID) {
				Renderer::DrawSolidCircle(transform.transform.position,
										  shape.shape->CastTo<Circle>().radius,
										  color);
			} else {
				Renderer::DrawCircle(transform.transform.position,
									 shape.shape->CastTo<Circle>().radius,
									 color);
			}
		}
	}
};

} // namespace ptgn