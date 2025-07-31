#include "debug/stats.h"

#include "debug/log.h"

namespace ptgn::impl {

void Stats::Reset() {
	ResetRendererRelated();
	ResetCollisionRelated();
}

void Stats::ResetCollisionRelated() {
	overlap_point_line		= 0;
	overlap_point_circle	= 0;
	overlap_point_rect		= 0;
	overlap_point_capsule	= 0;
	overlap_point_triangle	= 0;
	overlap_point_polygon	= 0;
	overlap_line_line		= 0;
	overlap_line_circle		= 0;
	overlap_line_rect		= 0;
	overlap_line_capsule	= 0;
	overlap_circle_circle	= 0;
	overlap_circle_rect		= 0;
	overlap_circle_capsule	= 0;
	overlap_triangle_rect	= 0;
	overlap_rect_rect		= 0;
	overlap_rect_capsule	= 0;
	overlap_capsule_capsule = 0;
	overlap_polygon_polygon = 0;

	intersect_circle_circle	  = 0;
	intersect_circle_rect	  = 0;
	intersect_circle_polygon  = 0;
	intersect_rect_rect		  = 0;
	intersect_polygon_polygon = 0;

	raycast_line_line	 = 0;
	raycast_line_circle	 = 0;
	raycast_line_rect	 = 0;
	raycast_line_capsule = 0;
	raycast_circle_rect	 = 0;
	raycast_rect_rect	 = 0;
}

void Stats::ResetRendererRelated() {
	shader_binds	   = 0;
	texture_binds	   = 0;
	buffer_binds	   = 0;
	vertex_array_binds = 0;
	frame_buffer_binds = 0;
	blend_mode_changes = 0;
	viewport_changes   = 0;
	clears			   = 0;
	clear_colors	   = 0;
	draw_calls		   = 0;
	gl_calls		   = 0;
}

void Stats::PrintCollisionOverlap() const {
	PTGN_LOG("overlap_point_line: ", overlap_point_line);
	PTGN_LOG("overlap_point_circle: ", overlap_point_circle);
	PTGN_LOG("overlap_point_rect: ", overlap_point_rect);
	PTGN_LOG("overlap_point_capsule: ", overlap_point_capsule);
	PTGN_LOG("overlap_point_triangle: ", overlap_point_triangle);
	PTGN_LOG("overlap_point_polygon: ", overlap_point_polygon);
	PTGN_LOG("overlap_line_line: ", overlap_line_line);
	PTGN_LOG("overlap_line_circle: ", overlap_line_circle);
	PTGN_LOG("overlap_line_rect: ", overlap_line_rect);
	PTGN_LOG("overlap_line_capsule: ", overlap_line_capsule);
	PTGN_LOG("overlap_circle_circle: ", overlap_circle_circle);
	PTGN_LOG("overlap_circle_rect: ", overlap_circle_rect);
	PTGN_LOG("overlap_circle_capsule: ", overlap_circle_capsule);
	PTGN_LOG("overlap_triangle_rect: ", overlap_triangle_rect);
	PTGN_LOG("overlap_rect_rect: ", overlap_rect_rect);
	PTGN_LOG("overlap_rect_capsule: ", overlap_rect_capsule);
	PTGN_LOG("overlap_rect_capsule: ", overlap_capsule_capsule);
	PTGN_LOG("overlap_polygon_polygon: ", overlap_polygon_polygon);
}

void Stats::PrintCollisionIntersect() const {
	PTGN_LOG("intersect_circle_circle: ", intersect_circle_circle);
	PTGN_LOG("intersect_circle_rect: ", intersect_circle_rect);
	PTGN_LOG("intersect_rect_rect: ", intersect_rect_rect);
	PTGN_LOG("intersect_polygon_polygon: ", intersect_polygon_polygon);
}

void Stats::PrintCollisionRaycast() const {
	PTGN_LOG("raycast_line_line: ", raycast_line_line);
	PTGN_LOG("raycast_line_circle: ", raycast_line_circle);
	PTGN_LOG("raycast_line_rect: ", raycast_line_rect);
	PTGN_LOG("raycast_line_capsule: ", raycast_line_capsule);
	PTGN_LOG("raycast_circle_rect: ", raycast_circle_rect);
	PTGN_LOG("raycast_rect_rect: ", raycast_rect_rect);
}

void Stats::PrintRenderer() const {
	PTGN_LOG("shader_binds: ", shader_binds);
	PTGN_LOG("texture_binds: ", texture_binds);
	PTGN_LOG("buffer_binds: ", buffer_binds);
	PTGN_LOG("vertex_array_binds: ", vertex_array_binds);
	PTGN_LOG("frame_buffer_binds: ", frame_buffer_binds);
	PTGN_LOG("blend_mode_changes: ", blend_mode_changes);
	PTGN_LOG("viewport_changes: ", viewport_changes);
	PTGN_LOG("clears: ", clears);
	PTGN_LOG("clear_colors: ", clear_colors);
	PTGN_LOG("draw_calls: ", draw_calls);
	PTGN_LOG("gl_calls: ", gl_calls);
}

} // namespace ptgn::impl
