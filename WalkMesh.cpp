#include "WalkMesh.hpp"

#include <glm/gtx/norm.hpp>
#include <iostream>

using namespace glm;

WalkMesh::WalkMesh(std::vector< glm::vec3 > const &vertices_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), triangles(triangles_) {
	// Construct next_vertex map
	next_vertex.clear();
	for (glm::uvec3 triangle : triangles) {
		next_vertex.insert({{glm::uvec2(triangle.x, triangle.y), triangle.z},
		{glm::uvec2(triangle.y, triangle.z), triangle.x}, {glm::uvec2(triangle.z, triangle.x), triangle.y}});
	}
}

// Adapted from this post: https://www.gamedev.net/forums/topic/552906-closest-point-on-triangle/
// With algorithm taken from this paper: https://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
vec3 closest_bary_pt_triangle(const vec3 *triangle, const vec3 &sourcePosition )
{
    vec3 edge0 = triangle[1] - triangle[0];
    vec3 edge1 = triangle[2] - triangle[0];
    vec3 v0 = triangle[0] - sourcePosition;

    float a = dot(edge0, edge0 );
    float b = dot(edge0, edge1 );
    float c = dot(edge1, edge1 );
    float d = dot(edge0, v0 );
    float e = dot(edge1, v0 );

    float det = a*c - b*b;
    float s = b*e - c*d;
    float t = b*d - a*e;

    if ( s + t < det )
    {
        if ( s < 0.f )
        {
            if ( t < 0.f )
            {
                if ( d < 0.f )
                {
                    s = clamp( -d/a, 0.f, 1.f );
                    t = 0.f;
                }
                else
                {
                    s = 0.f;
                    t = clamp( -e/c, 0.f, 1.f );
                }
            }
            else
            {
                s = 0.f;
                t = clamp( -e/c, 0.f, 1.f );
            }
        }
        else if ( t < 0.f )
        {
            s = clamp( -d/a, 0.f, 1.f );
            t = 0.f;
        }
        else
        {
            float invDet = 1.f / det;
            s *= invDet;
            t *= invDet;
        }
    }
    else
    {
        if ( s < 0.f )
        {
            float tmp0 = b+d;
            float tmp1 = c+e;
            if ( tmp1 > tmp0 )
            {
                float numer = tmp1 - tmp0;
                float denom = a-2*b+c;
                s = clamp( numer/denom, 0.f, 1.f );
                t = 1-s;
            }
            else
            {
                t = clamp( -e/c, 0.f, 1.f );
                s = 0.f;
            }
        }
        else if ( t < 0.f )
        {
            if ( a+d > b+e )
            {
                float numer = c+e-b-d;
                float denom = a-2*b+c;
                s = clamp( numer/denom, 0.f, 1.f );
                t = 1-s;
            }
            else
            {
                s = clamp( -e/c, 0.f, 1.f );
                t = 0.f;
            }
        }
        else
        {
            float numer = c+e-b-d;
            float denom = a-2*b+c;
            s = clamp( numer/denom, 0.f, 1.f );
            t = 1.f - s;
        }
    }

	// Convert to barycentric
	float wt0 = 1.f - s - t;
	float wt1 = s;
	float wt2 = t;

    return vec3(wt0, wt1, wt2);
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &start_point) const {
	WalkPoint closest;
	float closest_dist = FLT_MAX;
	//TODO: iterate through triangles
	for (glm::uvec3 triangle : triangles) {
		//TODO: for each triangle, find closest point on triangle to world_point
		vec3 pts[] = {vertices[triangle.x], vertices[triangle.y], vertices[triangle.z]};

		vec3 bary = closest_bary_pt_triangle(pts, start_point);
		vec3 world = bary.x * pts[0] + bary.y * pts[1] + bary.z * pts[2];

		// Use distance^2 to save on compute power
		float dist = distance2(world, start_point);

		//TODO: if point is closest, closest.triangle gets the current triangle,
		//closest.weights gets the barycentric coordinates
		if (dist < closest_dist) {
			closest_dist = dist;
			closest.triangle = triangle;
			closest.weights = bary;
		}
	}
	return closest;
}

// Adapted from discussions on https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
vec3 bary_proj(const vec3 *triangle, const vec3 &pt) {
	// Project onto x,y plane
	vec3 v0 = triangle[1] - triangle[0], v1 = triangle[2] - triangle[0], v2 = pt - triangle[0];
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
	vec3 result;
    result.y = (d11 * d20 - d01 * d21) / denom;
    result.z = (d00 * d21 - d01 * d20) / denom;
    result.x = 1.0f - result.y - result.z;
	return result;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step, bool edge) const {
	//TODO: project step to barycentric coordinates to get weights_step
	vec3 pts[] = {vertices[wp.triangle.x], vertices[wp.triangle.y], vertices[wp.triangle.z]};
	vec3 bary_weights = bary_proj(pts, world_point(wp) + step);
	vec3 weights_step = bary_weights - wp.weights;
	/*if (length2(step) > 0.0005f) {
		std::cout << "BARY: " << bary_weights.x << ", " << bary_weights.y << ", " << bary_weights.z << std::endl;
		std::cout << "IPOS: " << wp.weights.x << ", " << wp.weights.y << ", " << wp.weights.z << std::endl;
		std::cout << "WSTEP: " << weights_step.x << ", " << weights_step.y << ", " << weights_step.z << std::endl;
	}*/
	//TODO: when does wp.weights + t * weights_step cross a triangle edge?
	vec3 final_weights = wp.weights + weights_step;
	float t = 1.0f;
	uint32_t opp_corner;
	uvec2 edge_crossed_verts;
	uvec2 edge_crossed_indices;
	uint32_t zero_vert = 0;
	for (int i=0; i<3; i++) {
		if (final_weights[i] < 0.f) {
			float t_test = abs(wp.weights[i] / weights_step[i]);
			if (t_test < t) {
				int vindex = (i + 1) % 3;
				int uindex = (i + 2) % 3;
				edge_crossed_verts = uvec2(wp.triangle[vindex], wp.triangle[uindex]);
				edge_crossed_indices = uvec2(vindex, uindex);
				opp_corner = wp.triangle[i];
				t = t_test;
				zero_vert = i;
			}
		} else if (final_weights[i] > 1.f) {
			float t_test = abs((1.f - wp.weights[i]) / weights_step[i]);
			if (t_test < t) {
				t = t_test;
				int vindex = (i + 1) % 3;
				int uindex = (i + 2) % 3;
				if (weights_step[vindex] > weights_step[uindex]) {
					edge_crossed_verts = uvec2(wp.triangle[i], wp.triangle[vindex]);
					edge_crossed_indices = uvec2(i, vindex);
					opp_corner = wp.triangle[uindex];
				} else {
					edge_crossed_verts = uvec2(wp.triangle[uindex], wp.triangle[i]);
					edge_crossed_indices = uvec2(uindex, i);
					opp_corner = wp.triangle[vindex];
				}
			}
		}
	}

	//std::cout << "t: " << t << std::endl;
	if (t >= 1.0f) { //if a triangle edge is not crossed
		//TODO: wp.weights gets moved by weights_step, nothing else needs to be done.
		wp.weights += weights_step;
	} else { //if a triangle edge is crossed
		//TODO: wp.weights gets moved to triangle edge, and step gets reduced
		wp.weights += weights_step * t;
		vec3 new_step = step * (1.f - t);
		//if there is another triangle over the edge:
		auto vertex = next_vertex.find(uvec2(edge_crossed_verts.y, edge_crossed_verts.x));
		//std::cout << "EDGE REACHED" << std::endl;
		if (vertex != next_vertex.end()) {
			//std::cout << "NEXT TRI" << vertex -> second << std::endl;
			//TODO: wp.triangle gets updated to adjacent triangle

			//if (!edge) {
				wp.triangle = uvec3(vertex->first.x, vertex->first.y, vertex->second);
				vec3 new_weights = vec3(0, 0, 0);
				new_weights.x = wp.weights[edge_crossed_indices.y];
				new_weights.y = 1.f - new_weights.x;
				wp.weights = new_weights;
				//TODO: step gets rotated over the edge
				walk(wp, new_step, true);
			//}
		//else if there is no other triangle over the edge:
		} else {
			//TODO: wp.triangle stays the same.
			//TODO: step gets updated to slide along the edge
			
			// Only do it if not already walking along edge
			if (!edge) {
				//std::cout << "EDGE HIT: " << edge_crossed_indices.x << ", " << edge_crossed_indices.y << std::endl;
				vec3 edge_vector = vertices[edge_crossed_verts.x] - vertices[edge_crossed_verts.y];
				vec3 median = vertices[opp_corner] - (vertices[edge_crossed_verts.x] + vertices[edge_crossed_verts.y]) / 2.f;
				
				// Now, project the step onto the vector, with some adjustment for floating point errors
				walk(wp, edge_vector * dot(new_step, edge_vector) / dot(edge_vector, edge_vector) + median * 0.000001f, true);
			}
		}
	}
	//std::cout << "FPOS: " << wp.weights.x << ", " << wp.weights.y << ", " << wp.weights.z << std::endl;
}
